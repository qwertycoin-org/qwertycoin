#include <gtest/gtest.h>

#ifndef _WIN32

#include <csignal>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <boost/filesystem.hpp>
#include <lmdb.h>

#include "blockchain_db/lmdb/db_lmdb.h"

namespace
{
  struct scoped_lmdb_folder
  {
    boost::filesystem::path path = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path("qwc-lmdb-remove-%%%%-%%%%-%%%%");

    scoped_lmdb_folder() { boost::filesystem::create_directories(path); }
    ~scoped_lmdb_folder()
    {
      boost::system::error_code ignored;
      boost::filesystem::remove_all(path, ignored);
    }

    std::string data_file() const { return (path / "data.mdb").string(); }
    std::string lock_file() const { return (path / "lock.mdb").string(); }
  };

  bool write_one_record(const std::string &folder, MDB_env *&env)
  {
    MDB_txn *txn = nullptr;
    MDB_dbi dbi;
    char key_bytes[] = "key";
    char value_bytes[] = "value";
    MDB_val key{sizeof(key_bytes), key_bytes};
    MDB_val value{sizeof(value_bytes), value_bytes};
    return mdb_env_create(&env) == 0
        && mdb_env_open(env, folder.c_str(), MDB_NOSYNC, 0644) == 0
        && mdb_txn_begin(env, nullptr, 0, &txn) == 0
        && mdb_dbi_open(txn, nullptr, 0, &dbi) == 0
        && mdb_put(txn, dbi, &key, &value, 0) == 0
        && mdb_txn_commit(txn) == 0;
  }

  struct env_holder_process
  {
    pid_t pid = -1;
    bool ready = false;

    explicit env_holder_process(const std::string &folder)
    {
      int ready_pipe[2];
      if (pipe(ready_pipe) != 0)
        return;
      pid = fork();
      if (pid == 0)
      {
        close(ready_pipe[0]);
        MDB_env *env = nullptr;
        if (write_one_record(folder, env) && write(ready_pipe[1], "r", 1) == 1)
          pause();
        _exit(1);
      }
      close(ready_pipe[1]);
      char signal_byte = 0;
      ready = pid > 0 && read(ready_pipe[0], &signal_byte, 1) == 1;
      close(ready_pipe[0]);
    }

    ~env_holder_process()
    {
      if (pid > 0)
      {
        kill(pid, SIGKILL);
        waitpid(pid, nullptr, 0);
      }
    }
  };
}

TEST(lmdb_remove_data_file, refuses_while_another_process_holds_the_env_open)
{
  scoped_lmdb_folder folder;
  env_holder_process holder(folder.path.string());
  ASSERT_TRUE(holder.ready);

  EXPECT_FALSE(cryptonote::BlockchainLMDB().remove_data_file(folder.path.string()));
  EXPECT_TRUE(boost::filesystem::exists(folder.data_file()));
  EXPECT_TRUE(boost::filesystem::exists(folder.lock_file()));
}

TEST(lmdb_remove_data_file, removes_data_and_lock_file_of_a_closed_env)
{
  scoped_lmdb_folder folder;
  MDB_env *env = nullptr;
  const bool written = write_one_record(folder.path.string(), env);
  mdb_env_close(env);
  ASSERT_TRUE(written);

  EXPECT_TRUE(cryptonote::BlockchainLMDB().remove_data_file(folder.path.string()));
  EXPECT_FALSE(boost::filesystem::exists(folder.data_file()));
  EXPECT_FALSE(boost::filesystem::exists(folder.lock_file()));
}

TEST(lmdb_remove_data_file, accepts_a_folder_without_database)
{
  scoped_lmdb_folder folder;
  EXPECT_TRUE(cryptonote::BlockchainLMDB().remove_data_file(folder.path.string()));
  EXPECT_FALSE(boost::filesystem::exists(folder.lock_file()));
}

#endif
