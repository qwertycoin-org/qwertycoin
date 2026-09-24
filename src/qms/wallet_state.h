// Copyright (c) 2026, The Qwertycoin Project
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "crypto_backend.h"
#include "protocol.h"

namespace qwertycoin::qms
{
  struct wallet_contact
  {
    std::string label;
    std::string fingerprint;
    std::string contact_id;
    bool removed = false;
  };

  struct wallet_send_plan
  {
    id16 message_id{};
    std::string contact_fingerprint;
    std::string contact_label;
    bytes next_state;
    std::vector<std::vector<uint8_t>> carrier_extras;
    size_t envelope_size = 0;
  };

  struct wallet_receive_result
  {
    bool accepted_fragment = false;
    bool completed = false;
    std::string message_id;
    std::string contact_fingerprint;
    std::string contact_label;
    std::string text;
  };

  /**
   * Application-neutral QMS2 wallet state shared by native clients. The
   * serialized JSON is then protected by wallet2::store_qms_state; this class
   * never writes plaintext files and never performs network I/O.
   */
  class wallet_state
  {
  public:
    wallet_state(const std::string &serialized, const hash32 &genesis);
    ~wallet_state();
    wallet_state(wallet_state &&) noexcept;
    wallet_state &operator=(wallet_state &&) noexcept;
    wallet_state(const wallet_state &) = delete;
    wallet_state &operator=(const wallet_state &) = delete;

    std::string serialize() const;
    std::string own_invitation_hex() const;
    std::string own_fingerprint_hex() const;
    std::vector<wallet_contact> contacts() const;
    bool history_enabled() const;
    void set_history_enabled(bool enabled);
    void clear_history();
    std::string import_contact(const std::string &label,
        const bytes &contact_package, uint64_t now);
    wallet_send_plan prepare_send(const std::string &contact_fingerprint,
        const std::string &text, uint64_t now) const;
    wallet_receive_result ingest_carrier(const bytes &transaction_extra,
        uint64_t height, const std::string &block_hash,
        const std::string &transaction_id, uint64_t now);
    void accept_prepared(const wallet_send_plan &plan,
        const std::string &encrypted_journal, size_t transaction_count,
        uint64_t total_fee, uint64_t now);
    bool has_prepared() const;
    std::string prepared_journal() const;
    std::string prepared_contact_fingerprint() const;
    std::string prepared_message_id() const;
    void update_prepared_journal(const std::string &encrypted_journal,
        size_t transaction_count, uint64_t total_fee, uint64_t now);
    void clear_prepared();

  private:
    struct impl;
    std::unique_ptr<impl> m_impl;
  };

  std::string wallet_state_context(const hash32 &genesis,
      const std::string &primary_address);
}
