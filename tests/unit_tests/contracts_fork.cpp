// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include <string>
#include <vector>

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>

#include "gtest/gtest.h"

#include "cryptonote_config.h"
#include "cryptonote_core/blockchain.h"
#include "cryptonote_core/cryptonote_core.h"

namespace
{
  namespace po = boost::program_options;

  boost::filesystem::path make_temp_dir()
  {
    boost::system::error_code ec;
    const auto path = boost::filesystem::temp_directory_path()
        / boost::filesystem::unique_path("contracts-fork-%%%%%%%%%%%%%%%%", ec);
    if (ec || !boost::filesystem::create_directory(path, ec) || ec)
      return {};
    return path;
  }

  po::variables_map parse(const std::vector<std::string> &args)
  {
    po::options_description desc;
    cryptonote::core::init_options(desc);
    po::variables_map vm;
    po::store(po::command_line_parser(args).options(desc).run(), vm);
    po::notify(vm);
    return vm;
  }

  std::vector<std::string> regtest_args(const boost::filesystem::path &dir)
  {
    return {
      "--regtest", "--data-dir", dir.string(), "--disable-dns-checkpoints",
      "--check-updates=disabled", "--offline", "--fixed-difficulty=1",
    };
  }
}

TEST(contracts_fork, gate_starts_at_contracts_version)
{
  static_assert(HF_VERSION_QWC_CONTRACTS == 18, "contracts block major version");
  static_assert(!is_qwc_contracts_hardfork(HF_VERSION_QWC_EPOSE), "EPoSE version must not enable contracts rules");
  static_assert(is_qwc_contracts_hardfork(HF_VERSION_QWC_CONTRACTS), "contracts version enables contracts rules");
  ASSERT_FALSE(is_qwc_contracts_hardfork(0));
  ASSERT_FALSE(is_qwc_contracts_hardfork(16));
  ASSERT_FALSE(is_qwc_contracts_hardfork(17));
  ASSERT_TRUE(is_qwc_contracts_hardfork(18));
  ASSERT_TRUE(is_qwc_contracts_hardfork(19));
  ASSERT_TRUE(is_qwc_epose_v2_hardfork(18));
}

TEST(contracts_fork, constants_follow_the_decisions)
{
  ASSERT_EQ(10u, QWC_WINDOW_POOL_LEAD_BLOCKS);
  ASSERT_EQ(10u, QWC_WINDOW_UNLOCK_DELAY_BLOCKS);
  ASSERT_EQ(720u, QWC_WALLET_DEFAULT_WINDOW_BLOCKS);
  ASSERT_EQ(100u, QWC_WALLET_DEFAULT_WINDOW_ROUNDING);
  ASSERT_EQ(50u, QWC_CHAINLOCK_QUORUM_MAX);
  ASSERT_EQ(10u, QWC_CHAINLOCK_MIN_NODES);
  ASSERT_EQ(720u, QWC_LOCK_MAX_AGE_BLOCKS);
  ASSERT_EQ(65536u, QWC_CONTRACT_TX_EXTRA_MAX_SIZE);
  ASSERT_EQ(2160u, QWC_VM_UNDO_RETENTION_BLOCKS);
  ASSERT_EQ(static_cast<uint64_t>(CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW), QWC_MINT_UNLOCK_BLOCKS);
}

TEST(contracts_fork, chainlock_threshold_is_ceil_of_68_percent)
{
  ASSERT_EQ(34u, qwc_chainlock_threshold(50));
  ASSERT_EQ(7u, qwc_chainlock_threshold(10));
  ASSERT_EQ(1u, qwc_chainlock_threshold(1));
  for (uint64_t n = 1; n <= 200; ++n)
  {
    const uint64_t t = qwc_chainlock_threshold(n);
    ASSERT_GE(t * 100, n * 68) << n;
    ASSERT_LT((t - 1) * 100, n * 68) << n;
  }
}

TEST(contracts_fork, regtest_fork_height_is_off_by_default)
{
  const auto vm = parse({"--regtest"});
  ASSERT_TRUE(command_line::is_arg_defaulted(vm, cryptonote::arg_regtest_fork_height));
  ASSERT_EQ(0u, cryptonote::core::get_regtest_fork_height_from_args(vm));
}

TEST(contracts_fork, regtest_fork_height_requires_regtest)
{
  ASSERT_EQ(25u, cryptonote::core::get_regtest_fork_height_from_args(parse({"--regtest", "--regtest-fork-height=25"})));
  ASSERT_THROW(cryptonote::core::get_regtest_fork_height_from_args(parse({"--regtest-fork-height=25"})), std::runtime_error);
}

TEST(contracts_fork, regtest_hard_fork_table_without_option_stays_on_epose)
{
  const auto dir = make_temp_dir();
  ASSERT_FALSE(dir.empty());
  {
    cryptonote::core core(nullptr);
    ASSERT_TRUE(core.init(parse(regtest_args(dir))));
    const cryptonote::Blockchain &bc = core.get_blockchain_storage();
    ASSERT_EQ(HF_VERSION_QWC_EPOSE, bc.get_ideal_hard_fork_version());
    for (uint64_t h : {0u, 1u, 25u, 1000000u})
      ASSERT_EQ(HF_VERSION_QWC_EPOSE, bc.get_ideal_hard_fork_version(h)) << h;
    ASSERT_TRUE(core.deinit());
  }
  boost::system::error_code ec;
  boost::filesystem::remove_all(dir, ec);
}

TEST(contracts_fork, regtest_hard_fork_table_with_option_activates_contracts_at_height)
{
  const auto dir = make_temp_dir();
  ASSERT_FALSE(dir.empty());
  {
    auto args = regtest_args(dir);
    args.push_back("--regtest-fork-height=25");
    cryptonote::core core(nullptr);
    ASSERT_TRUE(core.init(parse(args)));
    const cryptonote::Blockchain &bc = core.get_blockchain_storage();
    ASSERT_EQ(HF_VERSION_QWC_CONTRACTS, bc.get_ideal_hard_fork_version());
    ASSERT_EQ(HF_VERSION_QWC_EPOSE, bc.get_ideal_hard_fork_version(0));
    ASSERT_EQ(HF_VERSION_QWC_EPOSE, bc.get_ideal_hard_fork_version(24));
    ASSERT_EQ(HF_VERSION_QWC_CONTRACTS, bc.get_ideal_hard_fork_version(25));
    ASSERT_EQ(HF_VERSION_QWC_CONTRACTS, bc.get_ideal_hard_fork_version(26));
    ASSERT_TRUE(core.deinit());
  }
  boost::system::error_code ec;
  boost::filesystem::remove_all(dir, ec);
}
