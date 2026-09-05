// Copyright (c) 2026, Qwertycoin
//
// SPDX-License-Identifier: BSD-3-Clause

#include "include_base_utils.h"
#include "cryptonote_basic/cryptonote_format_utils.h"
#include "epose/chain_state.h"
#include "epose/service_node.h"
#include "fuzzer.h"

#include <cstring>
#include <limits>
#include <string>
#include <vector>

BEGIN_INIT_SIMPLE_FUZZER()
END_INIT_SIMPLE_FUZZER()

BEGIN_SIMPLE_FUZZER()
  const std::string blob(reinterpret_cast<const char*>(buf), len);
  uint64_t fuzz_epoch = std::numeric_limits<uint64_t>::max();
  if (len >= sizeof(fuzz_epoch))
    std::memcpy(&fuzz_epoch, buf, sizeof(fuzz_epoch));

  qwertycoin::epose::service_node_identity identity{};
  if (qwertycoin::epose::parse_identity(blob, identity))
  {
    qwertycoin::epose::verify_registration_signature(identity, cryptonote::TESTNET);
    qwertycoin::epose::verify_registration_signature(identity, cryptonote::MAINNET);
    crypto::hash previous_epoch_hash{};
    qwertycoin::epose::verify_admission_proof(identity, cryptonote::TESTNET, previous_epoch_hash);
    previous_epoch_hash = crypto::cn_fast_hash(blob.data(), blob.size());
    qwertycoin::epose::verify_admission_proof(identity, cryptonote::TESTNET, previous_epoch_hash);
  }
  qwertycoin::epose::parse_registration_tx_extra_nonce(blob, identity);

  qwertycoin::epose::service_attestation attestation{};
  if (qwertycoin::epose::parse_attestation(blob, attestation))
  {
    qwertycoin::epose::verify_attestation_signature(attestation, cryptonote::TESTNET);
    qwertycoin::epose::verify_attestation_signature(attestation, cryptonote::MAINNET);
  }
  qwertycoin::epose::parse_attestation_tx_extra_nonce(blob, attestation);

  if (!blob.empty()
      && blob.size() <= TX_EXTRA_NONCE_MAX_COUNT
      && (static_cast<uint8_t>(blob[0]) == qwertycoin::epose::EPOSE_TX_EXTRA_NONCE_REGISTRATION
          || static_cast<uint8_t>(blob[0]) == qwertycoin::epose::EPOSE_TX_EXTRA_NONCE_ATTESTATION))
  {
    const cryptonote::blobdata extra_nonce(blob);
    std::vector<uint8_t> tx_extra;
    if (cryptonote::add_extra_nonce_to_tx_extra(tx_extra, extra_nonce))
    {
      std::vector<qwertycoin::epose::service_node_identity> registrations;
      std::vector<qwertycoin::epose::service_attestation> attestations;
      qwertycoin::epose::extract_registrations_from_tx_extra(tx_extra, registrations);
      qwertycoin::epose::extract_attestations_from_tx_extra(tx_extra, attestations);

      qwertycoin::epose::chain_state state(cryptonote::TESTNET, 4);
      crypto::hash previous_epoch_hash{};
      qwertycoin::epose::tx_extra_apply_summary summary{};
      state.apply_tx_extra(tx_extra, previous_epoch_hash, &summary);
      state.apply_tx_extra(tx_extra, previous_epoch_hash, &summary, fuzz_epoch);
      qwertycoin::epose::chain_state mainnet_state(cryptonote::MAINNET, 4);
      mainnet_state.apply_tx_extra(tx_extra, previous_epoch_hash, &summary, fuzz_epoch);
    }
  }
END_SIMPLE_FUZZER()
