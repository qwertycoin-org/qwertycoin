#include <gtest/gtest.h>

#include "cryptonote_config.h"
#include "version.h"

TEST(NetworkBanner, network_names)
{
  EXPECT_STREQ("Mainnet", cryptonote::network_type_name(cryptonote::MAINNET));
  EXPECT_STREQ("Testnet", cryptonote::network_type_name(cryptonote::TESTNET));
  EXPECT_STREQ("Stagenet", cryptonote::network_type_name(cryptonote::STAGENET));
  EXPECT_STREQ("Fakechain", cryptonote::network_type_name(cryptonote::FAKECHAIN));
  EXPECT_STREQ("Devnet", cryptonote::network_type_name(cryptonote::DEVNET));
  EXPECT_STREQ("Undefined", cryptonote::network_type_name(cryptonote::UNDEFINED));
}

TEST(NetworkBanner, lowercase_network_names)
{
  EXPECT_EQ("mainnet", cryptonote::network_type_name_lowercase(cryptonote::MAINNET));
  EXPECT_EQ("testnet", cryptonote::network_type_name_lowercase(cryptonote::TESTNET));
  EXPECT_EQ("stagenet", cryptonote::network_type_name_lowercase(cryptonote::STAGENET));
  EXPECT_EQ("fakechain", cryptonote::network_type_name_lowercase(cryptonote::FAKECHAIN));
  EXPECT_EQ("devnet", cryptonote::network_type_name_lowercase(cryptonote::DEVNET));
  EXPECT_EQ("undefined", cryptonote::network_type_name_lowercase(cryptonote::UNDEFINED));
}

TEST(NetworkBanner, banners)
{
  const std::string mainnet = qwertycoin_wallet_banner(cryptonote::MAINNET);
  const std::string testnet = qwertycoin_wallet_banner(cryptonote::TESTNET);
  const std::string devnet = qwertycoin_wallet_banner(cryptonote::DEVNET);

  EXPECT_NE(std::string::npos, mainnet.find("QWC v2 Mainnet"));
  EXPECT_NE(std::string::npos, testnet.find("QWC v2 Testnet"));
  EXPECT_NE(std::string::npos, devnet.find("QWC v2 Devnet"));
  EXPECT_NE(std::string::npos, mainnet.find(QWERTYCOIN_VERSION_FULL));
  EXPECT_NE(std::string::npos, testnet.find(QWERTYCOIN_VERSION_FULL));
  EXPECT_NE(std::string::npos, devnet.find(QWERTYCOIN_VERSION_FULL));
  EXPECT_EQ(std::string::npos, devnet.find("Mainnet"));
}
