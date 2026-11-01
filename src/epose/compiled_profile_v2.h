// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "cryptonote_config.h"

namespace qwertycoin
{
namespace epose
{
  // Generated from docs/epose/PARAMETER_MANIFEST_V2.json. Runtime files and
  // command-line flags are deliberately not consensus parameter sources.
  constexpr char MAINNET_FINAL_GENESIS_HASH_V2[] =
      "4f95857586e2c66063c277370eda99cd75897d773af09f0c3cd1e22f7e87db39";
  constexpr char MAINNET_FINAL_PARAMETER_SET_HASH_V2[] =
      "2c26755094535871dd3ede7bd1b50aba82a9fb6831f0a17f32968eb0385145c6";
  constexpr char DEVNET_GENESIS_HASH_V2[] =
      "75226372244a7740d785f77526dd585d56e1033c3f8d7508df9fc5bd88798288";
  constexpr char DEVNET_PARAMETER_SET_HASH_V2[] =
      "f3f91e840096c635b6294046c4a2503ff9e2522116a3805e76e76fc7b917385d";
  constexpr char FAKECHAIN_PARAMETER_SET_HASH_V2[] =
      "710c2c46678d237534274946f332e1228056297185685fb984def3c526f65362";

  inline bool compiled_profile_hashes_v2(
      cryptonote::network_type nettype,
      const char *&genesis_hash,
      const char *&parameter_set_hash)
  {
    switch (nettype)
    {
    case cryptonote::MAINNET:
      genesis_hash = MAINNET_FINAL_GENESIS_HASH_V2;
      parameter_set_hash = MAINNET_FINAL_PARAMETER_SET_HASH_V2;
      return true;
    case cryptonote::DEVNET:
      genesis_hash = DEVNET_GENESIS_HASH_V2;
      parameter_set_hash = DEVNET_PARAMETER_SET_HASH_V2;
      return true;
    default:
      return false;
    }
  }
} // namespace epose
} // namespace qwertycoin
