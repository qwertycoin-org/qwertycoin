// Copyright (c) 2026, The Qwertycoin Project
// SPDX-License-Identifier: BSD-3-Clause

#include <iostream>
#include <stdexcept>

#include "include_base_utils.h"
#include "fuzzer.h"
#include "qms/protocol.h"

BEGIN_INIT_SIMPLE_FUZZER()
END_INIT_SIMPLE_FUZZER()

BEGIN_SIMPLE_FUZZER()
  const qwertycoin::qms::bytes input(buf, buf + len);

  // Exercise every parser that consumes unauthenticated blockchain or
  // contact-package bytes. Exceptions are the expected fail-closed result;
  // the harness is interested in memory safety, hangs, and parser crashes.
  try
  {
    const auto fragment = qwertycoin::qms::decode_fragment(input);
    const auto canonical = qwertycoin::qms::encode_fragment(fragment);
    if (canonical != input)
      throw std::runtime_error("QMS fragment parser accepted non-canonical bytes");
  }
  catch (const std::exception &) {}

  try
  {
    (void)qwertycoin::qms::extract_carrier_fragments(input);
  }
  catch (const std::exception &) {}

  try
  {
    (void)qwertycoin::qms::decode_invitation(input);
  }
  catch (const std::exception &) {}
END_SIMPLE_FUZZER()
