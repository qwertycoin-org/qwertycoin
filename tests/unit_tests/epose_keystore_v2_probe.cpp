// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include <cstring>
#include <iostream>
#include <string>

#include "epose/service_keystore_v2.h"
#include "string_tools.h"

namespace
{
  crypto::hash hash_text(const char *text)
  {
    return crypto::cn_fast_hash(text, std::strlen(text));
  }
}

int main(int argc, char **argv)
{
  if (argc != 2)
  {
    std::cerr << "usage: epose_keystore_v2_probe <isolated-test-keystore>\n";
    return 2;
  }

  const qwertycoin::epose::service_keystore_context_v2 context{
      cryptonote::MAINNET,
      hash_text("windows-process-restart-genesis"),
      hash_text("windows-process-restart-parameters")};
  qwertycoin::epose::service_keystore_v2 keystore{};
  std::string error;
  const qwertycoin::epose::service_keystore_status_v2 status =
      qwertycoin::epose::load_or_create_service_keystore_v2(
          argv[1], context, keystore, error);
  if (status != qwertycoin::epose::service_keystore_status_v2::created
      && status != qwertycoin::epose::service_keystore_status_v2::loaded)
  {
    std::cerr << "keystore probe failed: " << error << '\n';
    return 1;
  }

  std::cout << "status="
            << (status == qwertycoin::epose::service_keystore_status_v2::created
                    ? "created" : "loaded")
            << " operator_public_key="
            << epee::string_tools::pod_to_hex(keystore.operator_public_key)
            << " service_public_key="
            << epee::string_tools::pod_to_hex(keystore.service_public_key)
            << '\n';
  return 0;
}
