// Copyright (c) 2026, The Qwertycoin Project
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice, this list
//    of conditions and the following disclaimer in the documentation and/or other
//    materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its contributors may be
//    used to endorse or promote products derived from this software without specific
//    prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "gtest/gtest.h"

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>

#include <cstring>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#include "cryptonote_core/cryptonote_core.h"
#include "epose/service_keystore_v2.h"

namespace
{
  namespace po = boost::program_options;

  struct parsed_epose_options
  {
    po::variables_map variables;
    bool used_alias;
  };

  struct option_sets
  {
    po::options_description canonical{"Canonical"};
    po::options_description compatibility{"Compatibility"};
    po::options_description all{"All"};

    option_sets()
    {
      cryptonote::core::init_options(canonical);
      cryptonote::init_epose_compatibility_options(compatibility);
      all.add(canonical);
      all.add(compatibility);
    }
  };

  parsed_epose_options parse_command_line(
      option_sets &sets, const std::vector<std::string> &arguments)
  {
    auto parsed = po::command_line_parser(arguments).options(sets.all).run();
    const bool used_alias = cryptonote::normalize_epose_option_aliases(parsed);
    po::variables_map variables;
    po::store(parsed, variables);
    po::notify(variables);
    return {std::move(variables), used_alias};
  }

  parsed_epose_options parse_config(
      option_sets &sets, const std::string &config)
  {
    std::istringstream stream(config);
    auto parsed = po::parse_config_file(stream, sets.all);
    const bool used_alias = cryptonote::normalize_epose_option_aliases(parsed);
    po::variables_map variables;
    po::store(parsed, variables);
    po::notify(variables);
    return {std::move(variables), used_alias};
  }

  struct temporary_directory
  {
    boost::filesystem::path path = boost::filesystem::unique_path(
        boost::filesystem::temp_directory_path() / "qwc-epose-options-%%%%-%%%%");

    temporary_directory() { boost::filesystem::create_directories(path); }
    ~temporary_directory()
    {
      boost::system::error_code ignored;
      boost::filesystem::remove_all(path, ignored);
    }
  };

  crypto::hash hash_text(const char *text)
  {
    return crypto::cn_fast_hash(text, std::strlen(text));
  }

  qwertycoin::epose::service_keystore_context_v2 keystore_context()
  {
    return {cryptonote::MAINNET, hash_text("genesis"), hash_text("parameters")};
  }
}

TEST(epose_public_options, canonical_names_are_visible_and_aliases_are_hidden)
{
  option_sets sets;
  std::ostringstream canonical;
  canonical << sets.canonical;
  EXPECT_NE(std::string::npos, canonical.str().find("--epose-service"));
  EXPECT_NE(std::string::npos, canonical.str().find("--epose-host"));
  EXPECT_EQ(std::string::npos, canonical.str().find("--epose-v2-service"));
  EXPECT_EQ(std::string::npos, canonical.str().find("--epose-v2-endpoint-host"));
}

TEST(epose_public_options, canonical_and_alias_inputs_resolve_identically)
{
  option_sets sets;
  const auto canonical = parse_command_line(sets, {
      "--epose-service",
      "--epose-keystore=/tmp/qwc-epose-test-keystore",
      "--epose-reward-address=QWC_TEST_ADDRESS",
      "--epose-host=service.example.org",
      "--epose-port=08198",
      "--epose-discovery-endpoint=http://one.example.org:8198",
      "--epose-discovery-endpoint=http://two.example.org:8198"});
  const auto alias = parse_command_line(sets, {
      "--epose-v2-service",
      "--epose-v2-keystore=/tmp/qwc-epose-test-keystore",
      "--epose-v2-reward-address=QWC_TEST_ADDRESS",
      "--epose-v2-endpoint-host=service.example.org",
      "--epose-v2-endpoint-port=8198",
      "--epose-v2-discovery-endpoint=http://one.example.org:8198",
      "--epose-v2-discovery-endpoint=http://two.example.org:8198"});

  EXPECT_FALSE(canonical.used_alias);
  EXPECT_TRUE(alias.used_alias);
  EXPECT_EQ(command_line::get_arg(canonical.variables, cryptonote::arg_epose_v2_service),
            command_line::get_arg(alias.variables, cryptonote::arg_epose_v2_service));
  EXPECT_EQ(command_line::get_arg(canonical.variables, cryptonote::arg_epose_v2_keystore),
            command_line::get_arg(alias.variables, cryptonote::arg_epose_v2_keystore));
  EXPECT_EQ(command_line::get_arg(canonical.variables, cryptonote::arg_epose_v2_reward_address),
            command_line::get_arg(alias.variables, cryptonote::arg_epose_v2_reward_address));
  EXPECT_EQ(command_line::get_arg(canonical.variables, cryptonote::arg_epose_v2_endpoint_host),
            command_line::get_arg(alias.variables, cryptonote::arg_epose_v2_endpoint_host));
  EXPECT_EQ(command_line::get_arg(canonical.variables, cryptonote::arg_epose_v2_endpoint_port),
            command_line::get_arg(alias.variables, cryptonote::arg_epose_v2_endpoint_port));
  EXPECT_EQ(command_line::get_arg(canonical.variables, cryptonote::arg_epose_v2_discovery_endpoint),
            command_line::get_arg(alias.variables, cryptonote::arg_epose_v2_discovery_endpoint));
}

TEST(epose_public_options, defaults_remain_unchanged)
{
  option_sets sets;
  const auto result = parse_command_line(sets, {});
  EXPECT_FALSE(result.used_alias);
  EXPECT_FALSE(command_line::get_arg(result.variables, cryptonote::arg_epose_v2_service));
  EXPECT_TRUE(command_line::get_arg(result.variables, cryptonote::arg_epose_v2_keystore).empty());
  EXPECT_TRUE(command_line::get_arg(result.variables, cryptonote::arg_epose_v2_reward_address).empty());
  EXPECT_TRUE(command_line::get_arg(result.variables, cryptonote::arg_epose_v2_endpoint_host).empty());
  EXPECT_EQ(0, command_line::get_arg(result.variables, cryptonote::arg_epose_v2_endpoint_port));
  EXPECT_TRUE(command_line::get_arg(
      result.variables, cryptonote::arg_epose_v2_discovery_endpoint).empty());
}

TEST(epose_public_options, equal_old_and_new_values_are_applied_once)
{
  option_sets sets;
  const auto result = parse_config(sets,
      "epose-service=true\n"
      "epose-v2-service=true\n"
      "epose-port=08198\n"
      "epose-v2-endpoint-port=8198\n"
      "epose-discovery-endpoint=http://one.example.org:8198\n"
      "epose-discovery-endpoint=http://two.example.org:8198\n"
      "epose-v2-discovery-endpoint=http://one.example.org:8198\n"
      "epose-v2-discovery-endpoint=http://two.example.org:8198\n");

  EXPECT_TRUE(result.used_alias);
  EXPECT_TRUE(command_line::get_arg(result.variables, cryptonote::arg_epose_v2_service));
  EXPECT_EQ(8198, command_line::get_arg(result.variables, cryptonote::arg_epose_v2_endpoint_port));
  EXPECT_EQ((std::vector<std::string>{
                "http://one.example.org:8198", "http://two.example.org:8198"}),
            command_line::get_arg(result.variables, cryptonote::arg_epose_v2_discovery_endpoint));
}

TEST(epose_public_options, conflicting_old_and_new_values_are_rejected)
{
  option_sets sets;
  EXPECT_THROW(parse_config(sets,
      "epose-service=false\nepose-v2-service=true\n"), po::error);
  EXPECT_THROW(parse_config(sets,
      "epose-host=one.example.org\nepose-v2-endpoint-host=two.example.org\n"), po::error);
  EXPECT_THROW(parse_config(sets,
      "epose-port=8198\nepose-v2-endpoint-port=8199\n"), po::error);
  EXPECT_THROW(parse_config(sets,
      "epose-discovery-endpoint=http://one.example.org:8198\n"
      "epose-discovery-endpoint=http://two.example.org:8198\n"
      "epose-v2-discovery-endpoint=http://two.example.org:8198\n"
      "epose-v2-discovery-endpoint=http://one.example.org:8198\n"), po::error);
}

TEST(epose_public_options, command_line_precedence_is_preserved_across_input_stages)
{
  option_sets sets;
  auto command_line_parsed = po::command_line_parser(
      std::vector<std::string>{"--epose-host=cli.example.org"})
      .options(sets.all).run();
  EXPECT_FALSE(cryptonote::normalize_epose_option_aliases(command_line_parsed));

  std::istringstream config("epose-v2-endpoint-host=config.example.org\n");
  auto config_parsed = po::parse_config_file(config, sets.all);
  EXPECT_TRUE(cryptonote::normalize_epose_option_aliases(config_parsed));

  po::variables_map variables;
  po::store(command_line_parsed, variables);
  po::store(config_parsed, variables);
  po::notify(variables);
  EXPECT_EQ("cli.example.org",
            command_line::get_arg(variables, cryptonote::arg_epose_v2_endpoint_host));
}

TEST(epose_public_options, canonical_and_alias_paths_load_the_same_existing_keystore)
{
  option_sets sets;
  temporary_directory directory;
  const boost::filesystem::path path = directory.path / "service.keys";
  const auto canonical = parse_config(
      sets, "epose-keystore=" + path.string() + "\n");
  const auto alias = parse_config(
      sets, "epose-v2-keystore=" + path.string() + "\n");
  const std::string canonical_path = command_line::get_arg(
      canonical.variables, cryptonote::arg_epose_v2_keystore);
  const std::string alias_path = command_line::get_arg(
      alias.variables, cryptonote::arg_epose_v2_keystore);
  ASSERT_EQ(canonical_path, alias_path);

  qwertycoin::epose::service_keystore_v2 created{};
  qwertycoin::epose::service_keystore_v2 loaded{};
  std::string error;
  ASSERT_EQ(qwertycoin::epose::service_keystore_status_v2::created,
      qwertycoin::epose::load_or_create_service_keystore_v2(
          canonical_path, keystore_context(), created, error)) << error;
  ASSERT_EQ(qwertycoin::epose::service_keystore_status_v2::loaded,
      qwertycoin::epose::load_or_create_service_keystore_v2(
          alias_path, keystore_context(), loaded, error)) << error;
  EXPECT_EQ(created.operator_public_key, loaded.operator_public_key);
  EXPECT_EQ(created.service_public_key, loaded.service_public_key);
  EXPECT_EQ(1, std::distance(
      boost::filesystem::directory_iterator(directory.path),
      boost::filesystem::directory_iterator()));
}
