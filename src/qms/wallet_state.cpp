// Copyright (c) 2026, The Qwertycoin Project
// SPDX-License-Identifier: BSD-3-Clause

#include "wallet_state.h"

#include <algorithm>
#include <cstring>
#include <locale>
#include <stdexcept>

#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"
#include "memwipe.h"
#include "string_coding.h"
#include "string_tools.h"

namespace qwertycoin::qms
{
  namespace
  {
    constexpr size_t max_incomplete_messages = 64;
    constexpr size_t max_incomplete_per_contact = 16;
    constexpr size_t max_reassembly_bytes = 8 * 1024 * 1024;
    constexpr size_t max_seen_messages = 4096;

    void wipe_json(rapidjson::Value &value)
    {
      if (value.IsString() && value.GetStringLength() != 0)
        memwipe(const_cast<char *>(value.GetString()), value.GetStringLength());
      else if (value.IsArray())
        for (auto &item : value.GetArray()) wipe_json(item);
      else if (value.IsObject())
        for (auto member = value.MemberBegin(); member != value.MemberEnd(); ++member)
          wipe_json(member->value);
    }

    std::string hex(const uint8_t *data, size_t size)
    {
      return epee::string_tools::buff_to_hex_nodelimer(
        std::string(reinterpret_cast<const char *>(data), size));
    }

    bytes unhex(const std::string &value)
    {
      std::string decoded;
      if ((value.size() & 1) != 0
          || !epee::string_tools::parse_hexstr_to_binbuff(value, decoded)
          || decoded.size() * 2 != value.size())
        throw std::runtime_error("QMS2 value is not canonical hexadecimal");
      return bytes(decoded.begin(), decoded.end());
    }

    std::string base64(const bytes &value)
    {
      return epee::string_encoding::base64_encode(value.data(), value.size());
    }

    bytes unbase64(const std::string &value)
    {
      const std::string decoded = epee::string_encoding::base64_decode(value);
      if (decoded.empty() && !value.empty())
        throw std::runtime_error("invalid QMS2 base64 state");
      return bytes(decoded.begin(), decoded.end());
    }

    std::string required_string(const rapidjson::Value &object, const char *name)
    {
      const auto found = object.FindMember(name);
      if (found == object.MemberEnd() || !found->value.IsString())
        throw std::runtime_error(std::string("missing QMS2 state field: ") + name);
      return std::string(found->value.GetString(), found->value.GetStringLength());
    }
  }

  struct wallet_state::impl
  {
    hash32 network{};
    rapidjson::Document document;
    std::unique_ptr<crypto_backend> crypto;
    id16 invitation_id{};
    hash32 fingerprint{};
    bytes contact_package;

    ~impl()
    {
      wipe_json(document);
      if (!contact_package.empty()) memwipe(contact_package.data(), contact_package.size());
      memwipe(invitation_id.data(), invitation_id.size());
      memwipe(fingerprint.data(), fingerprint.size());
    }

    rapidjson::Document::AllocatorType &allocator() { return document.GetAllocator(); }

    void set_string(const char *name, const std::string &value)
    {
      rapidjson::Value encoded(value.data(), value.size(), allocator());
      auto found = document.FindMember(name);
      if (found == document.MemberEnd())
        document.AddMember(rapidjson::Value(name, allocator()), encoded, allocator());
      else
      {
        wipe_json(found->value);
        found->value = encoded;
      }
    }

    void set_uint64(const char *name, uint64_t value)
    {
      auto found = document.FindMember(name);
      if (found == document.MemberEnd())
        document.AddMember(rapidjson::StringRef(name), value, allocator());
      else
        found->value.SetUint64(value);
    }

    rapidjson::Value &array(const char *name)
    {
      auto found = document.FindMember(name);
      if (found == document.MemberEnd())
      {
        document.AddMember(rapidjson::Value(name, allocator()),
          rapidjson::Value(rapidjson::kArrayType), allocator());
        found = document.FindMember(name);
      }
      if (!found->value.IsArray())
        throw std::runtime_error(std::string("invalid QMS2 state array: ") + name);
      return found->value;
    }

    void sync_crypto_state()
    {
      set_string("cryptoState", base64(crypto->state()));
      set_string("contactPackage", base64(contact_package));
      set_string("invitationId", hex(invitation_id.data(), invitation_id.size()));
      set_string("fingerprint", hex(fingerprint.data(), fingerprint.size()));
    }
  };

  wallet_state::wallet_state(const std::string &serialized, const hash32 &genesis)
    : m_impl(new impl())
  {
    m_impl->network = genesis;
    if (serialized.empty())
    {
      m_impl->document.SetObject();
      m_impl->document.AddMember("profile", CRYPTO_PROFILE_TRIPLE_RATCHET,
        m_impl->allocator());
      m_impl->document.AddMember("historyEnabled", false, m_impl->allocator());
      m_impl->array("contacts");
      m_impl->array("messages");
      m_impl->array("seenMessages");
      m_impl->document.AddMember("incomplete", rapidjson::Value(rapidjson::kObjectType),
        m_impl->allocator());
      m_impl->set_string("preparedJournal", "");
      m_impl->set_string("preparedMessageId", "");
      m_impl->set_string("preparedContactFingerprint", "");
      m_impl->crypto.reset(new crypto_backend(crypto_backend::create()));
      const auto prepared = m_impl->crypto->prepare_contact_package(genesis);
      m_impl->crypto.reset(new crypto_backend(prepared.next_state));
      m_impl->invitation_id = prepared.invitation_id;
      m_impl->fingerprint = prepared.fingerprint;
      m_impl->contact_package = prepared.package;
      m_impl->sync_crypto_state();
      return;
    }

    m_impl->document.Parse(serialized.data(), serialized.size());
    if (m_impl->document.HasParseError() || !m_impl->document.IsObject())
      throw std::runtime_error("invalid QMS2 wallet state JSON");
    const auto profile = m_impl->document.FindMember("profile");
    if (profile == m_impl->document.MemberEnd() || !profile->value.IsInt()
        || profile->value.GetInt() != CRYPTO_PROFILE_TRIPLE_RATCHET)
      throw std::runtime_error("unsupported or legacy QMS wallet profile");
    const bytes crypto_state = unbase64(required_string(m_impl->document, "cryptoState"));
    m_impl->contact_package = unbase64(required_string(m_impl->document, "contactPackage"));
    const bytes invitation = unhex(required_string(m_impl->document, "invitationId"));
    const bytes fingerprint = unhex(required_string(m_impl->document, "fingerprint"));
    if (crypto_state.empty() || m_impl->contact_package.empty()
        || invitation.size() != m_impl->invitation_id.size()
        || fingerprint.size() != m_impl->fingerprint.size())
      throw std::runtime_error("incomplete QMS2 wallet identity");
    std::memcpy(m_impl->invitation_id.data(), invitation.data(), invitation.size());
    std::memcpy(m_impl->fingerprint.data(), fingerprint.data(), fingerprint.size());
    m_impl->crypto.reset(new crypto_backend(crypto_state));
    m_impl->array("contacts");
    m_impl->array("messages");
    m_impl->array("seenMessages");
    auto history = m_impl->document.FindMember("historyEnabled");
    if (history == m_impl->document.MemberEnd())
      m_impl->document.AddMember("historyEnabled", false, m_impl->allocator());
    else if (!history->value.IsBool())
      throw std::runtime_error("invalid QMS2 history preference");
  }

  wallet_state::~wallet_state() = default;
  wallet_state::wallet_state(wallet_state &&) noexcept = default;
  wallet_state &wallet_state::operator=(wallet_state &&) noexcept = default;

  std::string wallet_state::serialize() const
  {
    m_impl->sync_crypto_state();
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    if (!m_impl->document.Accept(writer))
      throw std::runtime_error("cannot serialize QMS2 wallet state");
    return std::string(buffer.GetString(), buffer.GetSize());
  }

  std::string wallet_state::own_invitation_hex() const
  {
    return hex(m_impl->contact_package.data(), m_impl->contact_package.size());
  }

  std::string wallet_state::own_fingerprint_hex() const
  {
    return hex(m_impl->fingerprint.data(), m_impl->fingerprint.size());
  }

  std::vector<wallet_contact> wallet_state::contacts() const
  {
    std::vector<wallet_contact> result;
    const auto found = m_impl->document.FindMember("contacts");
    if (found == m_impl->document.MemberEnd() || !found->value.IsArray())
      return result;
    for (const auto &value : found->value.GetArray())
    {
      if (!value.IsObject()) continue;
      wallet_contact item;
      item.label = required_string(value, "label");
      item.fingerprint = required_string(value, "fingerprint");
      item.contact_id = required_string(value, "contactId");
      const auto removed = value.FindMember("removed");
      item.removed = removed != value.MemberEnd() && removed->value.IsBool()
        && removed->value.GetBool();
      result.push_back(std::move(item));
    }
    return result;
  }

  bool wallet_state::history_enabled() const
  {
    const auto found = m_impl->document.FindMember("historyEnabled");
    return found != m_impl->document.MemberEnd()
      && found->value.IsBool() && found->value.GetBool();
  }

  void wallet_state::set_history_enabled(bool enabled)
  {
    auto found = m_impl->document.FindMember("historyEnabled");
    if (found == m_impl->document.MemberEnd())
      m_impl->document.AddMember("historyEnabled", enabled, m_impl->allocator());
    else
      found->value.SetBool(enabled);
  }

  void wallet_state::clear_history()
  {
    auto &messages = m_impl->array("messages");
    wipe_json(messages);
    messages.Clear();
  }

  std::string wallet_state::import_contact(const std::string &label,
      const bytes &contact_package, uint64_t now)
  {
    if (label.empty()) throw std::runtime_error("contact name is required");
    const auto prepared = m_impl->crypto->prepare_import_contact(
      m_impl->invitation_id, contact_package, now);
    const std::string fingerprint = hex(prepared.fingerprint.data(), prepared.fingerprint.size());
    if (fingerprint == own_fingerprint_hex())
      throw std::runtime_error("cannot import this wallet's own QMS2 invitation");

    auto &contacts = m_impl->array("contacts");
    for (auto &value : contacts.GetArray())
    {
      if (!value.IsObject() || required_string(value, "fingerprint") != fingerprint)
        continue;
      if (required_string(value, "package") != base64(contact_package))
        throw std::runtime_error("contact fingerprint is already bound to different package data");
      auto removed = value.FindMember("removed");
      if (removed == value.MemberEnd() || !removed->value.IsBool() || !removed->value.GetBool())
        return fingerprint; // idempotent import
      value["removed"].SetBool(false);
      value["label"].SetString(label.data(), label.size(), m_impl->allocator());
      return fingerprint;
    }

    rapidjson::Value contact(rapidjson::kObjectType);
    contact.AddMember("label", rapidjson::Value(label.data(), label.size(), m_impl->allocator()), m_impl->allocator());
    contact.AddMember("fingerprint", rapidjson::Value(fingerprint.data(), fingerprint.size(), m_impl->allocator()), m_impl->allocator());
    contact.AddMember("contactId", rapidjson::Value(prepared.contact_id.data(), prepared.contact_id.size(), m_impl->allocator()), m_impl->allocator());
    const std::string package = base64(contact_package);
    contact.AddMember("package", rapidjson::Value(package.data(), package.size(), m_impl->allocator()), m_impl->allocator());
    contact.AddMember("confirmed", true, m_impl->allocator());
    contact.AddMember("removed", false, m_impl->allocator());
    contacts.PushBack(contact, m_impl->allocator());
    m_impl->crypto.reset(new crypto_backend(prepared.next_state));
    return fingerprint;
  }

  wallet_send_plan wallet_state::prepare_send(
      const std::string &contact_fingerprint, const std::string &text, uint64_t now) const
  {
    if (has_prepared())
      throw std::runtime_error("cancel or send the existing QMS2 plan first");
    std::string contact_id;
    std::string contact_label;
    for (const auto &item : contacts())
      if (!item.removed && item.fingerprint == contact_fingerprint)
      {
        contact_id = item.contact_id;
        contact_label = item.label;
        break;
      }
    if (contact_id.empty()) throw std::runtime_error("unknown QMS2 contact fingerprint");

    const auto prepared = m_impl->crypto->prepare_send_text(contact_id, text, now);
    bytes inner;
    inner.reserve(1 + prepared.ciphertext.data.size());
    inner.push_back(prepared.ciphertext.message_type);
    inner.insert(inner.end(), prepared.ciphertext.data.begin(), prepared.ciphertext.data.end());
    const auto context = m_impl->crypto->transport_context(contact_id, true);
    const auto envelope = seal_outer_envelope(context, prepared.message_id, inner);
    const auto fragments = fragment_envelope(context, prepared.message_id, envelope);

    wallet_send_plan result;
    result.message_id = prepared.message_id;
    result.contact_fingerprint = contact_fingerprint;
    result.contact_label = contact_label;
    result.next_state = prepared.next_state;
    result.envelope_size = envelope.size();
    result.carrier_extras.reserve(fragments.size());
    for (const auto &fragment : fragments)
    {
      std::vector<uint8_t> extra;
      if (!append_carrier_nonces(extra, fragment))
        throw std::runtime_error("QMS2 fragment exceeds unchanged tx_extra limits");
      result.carrier_extras.push_back(std::move(extra));
    }
    return result;
  }

  wallet_receive_result wallet_state::ingest_carrier(
      const bytes &transaction_extra, uint64_t height,
      const std::string &block_hash, const std::string &transaction_id,
      uint64_t now)
  {
    wallet_receive_result result;
    const auto fragments = extract_carrier_fragments(transaction_extra);
    if (fragments.size() != 1 || fragments[0].profile != CRYPTO_PROFILE_TRIPLE_RATCHET)
      return result;
    const auto &fragment = fragments[0];
    const std::string message_id = hex(fragment.message_id.data(), fragment.message_id.size());

    std::string contact_id;
    std::string contact_fingerprint;
    std::string contact_label;
    envelope_context context;
    auto &contacts = m_impl->array("contacts");
    for (const auto &contact : contacts.GetArray())
    {
      if (!contact.IsObject()) continue;
      const auto removed = contact.FindMember("removed");
      if (removed != contact.MemberEnd() && removed->value.IsBool()
          && removed->value.GetBool())
        continue;
      const std::string candidate = required_string(contact, "contactId");
      if (candidate.empty()) continue;
      try
      {
        for (const auto &candidate_context :
             m_impl->crypto->transport_contexts(candidate, false))
        {
          if (!verify_envelope_fragment(candidate_context, fragment)) continue;
          contact_id = candidate;
          contact_fingerprint = required_string(contact, "fingerprint");
          contact_label = required_string(contact, "label");
          context = candidate_context;
          break;
        }
        if (!contact_id.empty()) break;
      }
      catch (...) {}
    }
    if (contact_id.empty()) return result;

    result.accepted_fragment = true;
    result.message_id = message_id;
    result.contact_fingerprint = contact_fingerprint;
    result.contact_label = contact_label;
    const std::string seen_key = contact_id + ":" + message_id;
    auto &seen = m_impl->array("seenMessages");
    for (const auto &entry : seen.GetArray())
      if (entry.IsString()
          && seen_key == std::string(entry.GetString(), entry.GetStringLength()))
        return result;

    auto incomplete_member = m_impl->document.FindMember("incomplete");
    if (incomplete_member == m_impl->document.MemberEnd()
        || !incomplete_member->value.IsObject())
      throw std::runtime_error("invalid QMS2 incomplete-message state");
    auto &incomplete_all = incomplete_member->value;
    auto existing = incomplete_all.FindMember(seen_key.c_str());
    if (existing != incomplete_all.MemberEnd())
    {
      if (!existing->value.IsObject())
        throw std::runtime_error("invalid QMS2 incomplete-message entry");
      const auto count = existing->value.FindMember("count");
      if (count == existing->value.MemberEnd() || !count->value.IsUint()
          || count->value.GetUint() != fragment.count)
      {
        incomplete_all.RemoveMember(seen_key.c_str());
        return result;
      }
    }
    else
    {
      if (incomplete_all.MemberCount() >= max_incomplete_messages) return result;
      size_t per_contact = 0;
      for (auto member = incomplete_all.MemberBegin(); member != incomplete_all.MemberEnd(); ++member)
        if (member->value.IsObject())
        {
          const auto id = member->value.FindMember("contactId");
          if (id != member->value.MemberEnd() && id->value.IsString()
              && contact_id == std::string(id->value.GetString(), id->value.GetStringLength()))
            ++per_contact;
        }
      if (per_contact >= max_incomplete_per_contact) return result;

      rapidjson::Value item(rapidjson::kObjectType);
      item.AddMember("contactId", rapidjson::Value(contact_id.data(), contact_id.size(), m_impl->allocator()), m_impl->allocator());
      item.AddMember("fingerprint", rapidjson::Value(contact_fingerprint.data(), contact_fingerprint.size(), m_impl->allocator()), m_impl->allocator());
      item.AddMember("count", unsigned(fragment.count), m_impl->allocator());
      item.AddMember("parts", rapidjson::Value(rapidjson::kArrayType), m_impl->allocator());
      rapidjson::Value key(seen_key.data(), seen_key.size(), m_impl->allocator());
      incomplete_all.AddMember(key, item, m_impl->allocator());
      existing = incomplete_all.FindMember(seen_key.c_str());
    }

    auto parts_member = existing->value.FindMember("parts");
    if (parts_member == existing->value.MemberEnd() || !parts_member->value.IsArray())
      throw std::runtime_error("invalid QMS2 fragment state");
    auto &parts = parts_member->value;
    const bytes encoded_fragment = encode_fragment(fragment);
    const std::string encoded = base64(encoded_fragment);
    for (const auto &part : parts.GetArray())
      if (part.IsObject())
      {
        const auto index = part.FindMember("index");
        if (index == part.MemberEnd() || !index->value.IsUint()
            || index->value.GetUint() != fragment.index)
          continue;
        if (required_string(part, "data") != encoded)
          incomplete_all.RemoveMember(seen_key.c_str());
        return result;
      }

    size_t stored_bytes = 0;
    for (auto member = incomplete_all.MemberBegin(); member != incomplete_all.MemberEnd(); ++member)
      if (member->value.IsObject())
      {
        const auto stored_parts = member->value.FindMember("parts");
        if (stored_parts == member->value.MemberEnd() || !stored_parts->value.IsArray()) continue;
        for (const auto &stored_part : stored_parts->value.GetArray())
          if (stored_part.IsObject())
            stored_bytes += unbase64(required_string(stored_part, "data")).size();
      }
    if (stored_bytes + encoded_fragment.size() > max_reassembly_bytes) return result;

    rapidjson::Value part(rapidjson::kObjectType);
    part.AddMember("index", unsigned(fragment.index), m_impl->allocator());
    part.AddMember("data", rapidjson::Value(encoded.data(), encoded.size(), m_impl->allocator()), m_impl->allocator());
    parts.PushBack(part, m_impl->allocator());
    if (parts.Size() != fragment.count) return result;

    std::vector<qwertycoin::qms::fragment> complete;
    complete.reserve(parts.Size());
    for (const auto &stored_part : parts.GetArray())
      complete.push_back(decode_fragment(unbase64(required_string(stored_part, "data"))));
    const auto envelope = reassemble(complete);
    const auto opened = open_outer_envelope(context, fragment.message_id, envelope);
    if (opened.size() < 2) throw std::runtime_error("empty QMS2 inner ciphertext");
    ratchet_ciphertext ciphertext;
    ciphertext.message_type = opened.front();
    ciphertext.data.assign(opened.begin() + 1, opened.end());
    const auto received = m_impl->crypto->prepare_receive_text(contact_id, ciphertext);
    if (received.message_id != fragment.message_id)
      throw std::runtime_error("QMS2 message identifier mismatch");

    m_impl->crypto.reset(new crypto_backend(received.next_state));
    incomplete_all.RemoveMember(seen_key.c_str());
    seen.PushBack(rapidjson::Value(seen_key.data(), seen_key.size(), m_impl->allocator()), m_impl->allocator());
    if (seen.Size() > max_seen_messages) seen.Erase(seen.Begin());

    const auto history = m_impl->document.FindMember("historyEnabled");
    if (history != m_impl->document.MemberEnd() && history->value.IsBool()
        && history->value.GetBool())
    {
      rapidjson::Value message(rapidjson::kObjectType);
      message.AddMember("id", rapidjson::Value(message_id.data(), message_id.size(), m_impl->allocator()), m_impl->allocator());
      message.AddMember("contact", rapidjson::Value(contact_fingerprint.data(), contact_fingerprint.size(), m_impl->allocator()), m_impl->allocator());
      message.AddMember("label", rapidjson::Value(contact_label.data(), contact_label.size(), m_impl->allocator()), m_impl->allocator());
      message.AddMember("text", rapidjson::Value(received.text.data(), received.text.size(), m_impl->allocator()), m_impl->allocator());
      message.AddMember("direction", rapidjson::Value("in", m_impl->allocator()), m_impl->allocator());
      message.AddMember("status", rapidjson::Value("confirmed", m_impl->allocator()), m_impl->allocator());
      message.AddMember("height", height, m_impl->allocator());
      message.AddMember("blockHash", rapidjson::Value(block_hash.data(), block_hash.size(), m_impl->allocator()), m_impl->allocator());
      message.AddMember("txId", rapidjson::Value(transaction_id.data(), transaction_id.size(), m_impl->allocator()), m_impl->allocator());
      message.AddMember("timestamp", now, m_impl->allocator());
      m_impl->array("messages").PushBack(message, m_impl->allocator());
    }

    result.completed = true;
    result.text = received.text;
    return result;
  }

  void wallet_state::accept_prepared(const wallet_send_plan &plan,
      const std::string &encrypted_journal, size_t transaction_count,
      uint64_t total_fee, uint64_t now)
  {
    if (encrypted_journal.empty() || plan.next_state.empty()
        || transaction_count == 0 || transaction_count > MAX_FRAGMENTS)
      throw std::runtime_error("incomplete QMS2 transaction plan");
    m_impl->crypto.reset(new crypto_backend(plan.next_state));
    m_impl->set_string("preparedJournal", epee::string_encoding::base64_encode(encrypted_journal));
    m_impl->set_string("preparedMessageId", hex(plan.message_id.data(), plan.message_id.size()));
    m_impl->set_string("preparedContactFingerprint", plan.contact_fingerprint);
    m_impl->set_uint64("preparedTransactionCount", transaction_count);
    m_impl->set_uint64("preparedFee", total_fee);
    m_impl->set_uint64("preparedAt", now);
  }

  bool wallet_state::has_prepared() const
  {
    return !prepared_journal().empty();
  }

  std::string wallet_state::prepared_journal() const
  {
    const auto found = m_impl->document.FindMember("preparedJournal");
    if (found == m_impl->document.MemberEnd() || !found->value.IsString()
        || found->value.GetStringLength() == 0)
      return {};
    return epee::string_encoding::base64_decode(
      std::string(found->value.GetString(), found->value.GetStringLength()));
  }

  std::string wallet_state::prepared_contact_fingerprint() const
  {
    const auto found = m_impl->document.FindMember("preparedContactFingerprint");
    return found != m_impl->document.MemberEnd() && found->value.IsString()
      ? std::string(found->value.GetString(), found->value.GetStringLength()) : std::string();
  }

  std::string wallet_state::prepared_message_id() const
  {
    const auto found = m_impl->document.FindMember("preparedMessageId");
    return found != m_impl->document.MemberEnd() && found->value.IsString()
      ? std::string(found->value.GetString(), found->value.GetStringLength()) : std::string();
  }

  void wallet_state::update_prepared_journal(const std::string &encrypted_journal,
      size_t transaction_count, uint64_t total_fee, uint64_t now)
  {
    if (!has_prepared())
      throw std::runtime_error("no QMS2 transaction plan is pending");
    if (encrypted_journal.empty() || transaction_count == 0
        || transaction_count > MAX_FRAGMENTS)
      throw std::runtime_error("incomplete QMS2 transaction journal update");
    m_impl->set_string("preparedJournal",
      epee::string_encoding::base64_encode(encrypted_journal));
    m_impl->set_uint64("preparedTransactionCount", transaction_count);
    m_impl->set_uint64("preparedFee", total_fee);
    m_impl->set_uint64("preparedAt", now);
  }

  void wallet_state::clear_prepared()
  {
    m_impl->set_string("preparedJournal", "");
    m_impl->set_string("preparedMessageId", "");
    m_impl->set_string("preparedContactFingerprint", "");
    m_impl->set_uint64("preparedTransactionCount", 0);
    m_impl->set_uint64("preparedFee", 0);
    m_impl->set_uint64("preparedAt", 0);
  }

  std::string wallet_state_context(const hash32 &genesis,
      const std::string &primary_address)
  {
    if (primary_address.empty())
      throw std::runtime_error("QMS2 state requires a primary wallet address");
    return std::string("QWC-QMS2-WALLET|")
      + hex(genesis.data(), genesis.size()) + "|" + primary_address;
  }
}
