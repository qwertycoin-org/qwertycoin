#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "protocol.h"

namespace qwertycoin
{
namespace qms
{
  struct prepared_contact_package
  {
    id16 invitation_id{};
    bytes package;
    bytes next_state;
  };

  struct prepared_contact_import
  {
    std::string contact_id;
    hash32 fingerprint{};
    bytes next_state;
  };

  struct ratchet_ciphertext
  {
    uint8_t message_type = 0;
    bytes data;
  };

  struct prepared_ratchet_send
  {
    ratchet_ciphertext ciphertext;
    id16 message_id{};
    bytes next_state;
  };

  struct prepared_ratchet_receive
  {
    std::string text;
    id16 message_id{};
    bytes next_state;
  };

  // Transactional native wrapper around the pinned Rust/libsignal backend.
  // Preparing an operation never changes this object's committed state. The
  // caller must durably store `next_state` together with the ciphertext/send
  // journal, then construct a new engine from that state.
  class crypto_backend
  {
  public:
    static crypto_backend create();
    explicit crypto_backend(bytes encoded_state);

    const bytes& state() const noexcept { return state_; }

    prepared_contact_package prepare_contact_package(const hash32& genesis) const;
    prepared_contact_import prepare_import_contact(
      const id16& local_invitation_id, const bytes& remote_package,
      uint64_t now_unix_seconds) const;
    prepared_ratchet_send prepare_send_text(
      const std::string& contact_id, const std::string& text,
      uint64_t now_unix_seconds) const;
    prepared_ratchet_receive prepare_receive_text(
      const std::string& contact_id, const ratchet_ciphertext& ciphertext) const;
    envelope_context transport_context(const std::string& contact_id,
                                       bool outgoing) const;

  private:
    bytes state_;
  };
}
}
