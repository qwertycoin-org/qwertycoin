// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause
#include "release_policy.h"
#include <algorithm>
#include <cstring>
#include <limits>
#include <set>

namespace qwertycoin { namespace epose { namespace release_policy_prototype {
namespace {
template<class T> void put(std::string &out, const T &v)
{ out.append(reinterpret_cast<const char *>(&v), sizeof(v)); }
void u8(std::string &out, uint8_t v) { out.push_back(static_cast<char>(v)); }
void u64(std::string &out, uint64_t v)
{ for (unsigned s=0; s<64; s+=8) u8(out, (v >> s) & 255); }
struct reader
{
  const std::string &s; size_t offset = 0;
  template<class T> bool get(T &v)
  {
    if (offset > s.size() || s.size()-offset < sizeof(v)) return false;
    std::memcpy(&v, s.data()+offset, sizeof(v)); offset += sizeof(v); return true;
  }
  bool number(uint64_t &v)
  {
    if (offset > s.size() || s.size()-offset < 8) return false;
    v=0; for (unsigned b=0; b<64; b+=8) v |= uint64_t(uint8_t(s[offset++])) << b;
    return true;
  }
};
template<class T> bool zero(const T &v) { const T z{}; return std::memcmp(&v,&z,sizeof(v)) == 0; }
bool keys_valid(const key_set &keys)
{
  for (size_t i=0;i<keys.size();++i) {
    if (!crypto::check_key(keys[i])) return false;
    for (size_t j=0;j<i;++j) if (keys[i]==keys[j]) return false;
  }
  return true;
}
bool label_valid(const std::string &s)
{
  if (s.empty() || s.size()>32) return false;
  unsigned dots=0, digits=0;
  for (char c:s) {
    if (c=='.') { if (!digits || ++dots>2) return false; digits=0; }
    else if (c>='0' && c<='9') ++digits;
    else return false;
  }
  return dots==2 && digits;
}
std::string body(const record &r)
{
  std::string out("QWCRP001",8);
  u8(out,r.version); u8(out,static_cast<uint8_t>(r.op));
  u64(out,r.sequence); put(out,r.previous); u64(out,r.generation);
  u64(out,r.release_id); u64(out,r.predecessor);
  u8(out,static_cast<uint8_t>(r.label.size())); out+=r.label;
  u64(out,r.target_epoch); u64(out,r.reference_sequence);
  for (const auto &k:r.next_keys) put(out,k);
  u8(out,r.signer_mask); u8(out,r.next_signer_mask);
  return out;
}
std::string domain(const char *name, const receipt_context_v2 &ctx)
{
  std::string out(name); u8(out,static_cast<uint8_t>(ctx.nettype));
  put(out,ctx.genesis_hash); put(out,ctx.parameter_set_hash); return out;
}
crypto::hash digest(const std::string &s) { return crypto::cn_fast_hash(s.data(),s.size()); }
bool mask_valid(uint8_t mask) { return mask==3 || mask==5 || mask==6 || mask==7; }
bool authorize(uint8_t mask, const std::array<crypto::signature,3> &sigs,
    const key_set &keys, const crypto::hash &hash)
{
  if (!mask_valid(mask)) return false;
  for (size_t i=0;i<3;++i) {
    if ((mask & (1<<i)) ? !crypto::check_signature(hash,keys[i],sigs[i]) : !zero(sigs[i]))
      return false;
  }
  return true;
}
crypto::hash claim_hash(const receipt &r, const receipt_context_v2 &ctx, bool verifier)
{
  auto s=domain(verifier ? "QWC_RP_VERIFIER_CLAIM_1" : "QWC_RP_SUBJECT_CLAIM_1",ctx);
  u8(s,r.version); put(s,hash_authenticated_service_receipt_v2(r.service,ctx));
  put(s,r.service.verifier_signature);
  u64(s,r.subject_release); u64(s,r.verifier_release); put(s,r.policy_hash);
  if (verifier) put(s,r.subject_claim);
  return digest(s);
}
}
std::string encode(const record &r)
{
  if (r.label.size()>32) return {};
  auto s=body(r); for (const auto &sig:r.signatures) put(s,sig);
  for (const auto &sig:r.next_signatures) put(s,sig);
  return s;
}
bool decode(const std::string &s, record &out)
{
  out={}; if (s.size()>max_record_bytes || s.size()<8 || s.substr(0,8)!="QWCRP001") return false;
  record r{}; reader in{s,8}; uint8_t op=0,len=0;
  if (!in.get(r.version) || r.version!=1 || !in.get(op) || op<1 || op>4
      || !in.number(r.sequence) || !in.get(r.previous) || !in.number(r.generation)
      || !in.number(r.release_id) || !in.number(r.predecessor) || !in.get(len)
      || len>32 || in.offset>s.size() || s.size()-in.offset<len) return false;
  r.op=static_cast<operation>(op); r.label=s.substr(in.offset,len); in.offset+=len;
  if (!in.number(r.target_epoch) || !in.number(r.reference_sequence)) return false;
  for (auto &k:r.next_keys) if (!in.get(k)) return false;
  if (!in.get(r.signer_mask) || !in.get(r.next_signer_mask)) return false;
  for (auto &sig:r.signatures) if (!in.get(sig)) return false;
  for (auto &sig:r.next_signatures) if (!in.get(sig)) return false;
  if (in.offset!=s.size()) return false;
  out=std::move(r); return true;
}
crypto::hash signing_hash(const record &r, const receipt_context_v2 &context)
{ return digest(domain("QWC_RELEASE_POLICY_1",context)+body(r)); }
void sign(record &r, size_t index, const crypto::secret_key &key,
    const receipt_context_v2 &ctx, bool next)
{
  if (index>=3) return;
  // Set BOTH masks before signing: masks are part of the authorization message.
  crypto::public_key pub{}; if (!crypto::secret_key_to_public_key(key,pub)) return;
  crypto::generate_signature(signing_hash(r,ctx),pub,key,
      next ? r.next_signatures[index] : r.signatures[index]);
}
chain::chain(configuration config):config_(std::move(config))
{
  uint64_t lock=0;
  valid_=config_.context.nettype==cryptonote::TESTNET
      && config_.context.genesis_hash!=crypto::null_hash
      && config_.context.parameter_set_hash!=crypto::null_hash
      && config_.timing.valid() && keys_valid(config_.authority)
      && config_.h0 && *config_.h0>0 && config_.minimum_notice_blocks>0
      && lock_height(config_.first_enforced_epoch,lock)
      && lock>=*config_.h0 && lock-*config_.h0>=config_.minimum_notice_blocks;
}
bool chain::valid() const { return valid_; }
bool chain::lock_height(uint64_t epoch, uint64_t &height) const
{
  uint64_t start=0;
  if (epoch<2 || !config_.timing.epoch_start(epoch-1,start) || start==0) return false;
  height=start-1; return true;
}
uint64_t chain::next_sequence() const
{ return records_.empty() ? 1 : records_.back().value.sequence+1; }
crypto::hash chain::tip_policy_hash() const
{ return records_.empty() ? crypto::null_hash : records_.back().hash; }
status chain::validate(const record &r, uint64_t height) const
{
  if (r.sequence==0 || r.sequence!=next_sequence() || r.previous!=tip_policy_hash())
    return status::bad_continuation;
  key_set keys=config_.authority; uint64_t generation=0;
  uint64_t latest=0; const entry *last_schedule=nullptr;
  std::set<uint64_t> cancelled;
  for (const auto &e:records_)
    if (e.value.op==operation::cancel) cancelled.insert(e.value.reference_sequence);
  for (const auto &e:records_) {
    if (e.value.op==operation::rotate) { keys=e.value.next_keys; ++generation; }
    if (e.value.op==operation::publish) latest=e.value.release_id;
    if (e.value.op==operation::schedule && !cancelled.count(e.value.sequence)) last_schedule=&e;
  }
  if (r.generation!=generation || !authorize(r.signer_mask,r.signatures,keys,signing_hash(r,config_.context)))
    return status::invalid_authorization;
  if (r.op!=operation::rotate && (r.next_signer_mask || !zero(r.next_keys) || !zero(r.next_signatures)))
    return status::malformed_record;
  if (r.op!=operation::publish && (r.predecessor || !r.label.empty())) return status::malformed_record;
  if (r.op!=operation::schedule && r.target_epoch) return status::malformed_record;
  if (r.op!=operation::cancel && r.reference_sequence) return status::malformed_record;
  if (r.op==operation::publish) {
    if (r.release_id<=latest || r.predecessor!=latest || !label_valid(r.label)) return status::invalid_release;
    for (const auto &e:records_) if (e.value.op==operation::publish && e.value.label==r.label) return status::invalid_release;
  } else if (r.op==operation::schedule) {
    const entry *release=nullptr;
    for (const auto &e:records_) if (e.value.op==operation::publish && e.value.release_id==r.release_id) release=&e;
    if (!release || !release->value.predecessor || r.target_epoch<config_.first_enforced_epoch
        || (!last_schedule && r.target_epoch!=config_.first_enforced_epoch)
        || (last_schedule && (r.target_epoch<=last_schedule->value.target_epoch
            || r.release_id<=last_schedule->value.release_id))) return status::invalid_schedule;
    uint64_t lock=0;
    if (!lock_height(r.target_epoch,lock) || height>lock
        || lock-height<config_.minimum_notice_blocks) return status::too_late;
  } else if (r.op==operation::cancel) {
    if (r.release_id || !last_schedule || last_schedule->value.sequence!=r.reference_sequence)
      return status::invalid_schedule;
    uint64_t lock=0;
    if (!lock_height(last_schedule->value.target_epoch,lock) || height>lock
        || lock-height<config_.minimum_notice_blocks) return status::too_late;
    // Never leave the first enforced epoch without a bootstrap window.
    if (last_schedule->value.target_epoch==config_.first_enforced_epoch) return status::invalid_schedule;
  } else if (r.op==operation::rotate) {
    if (r.release_id || generation==std::numeric_limits<uint64_t>::max()
        || !keys_valid(r.next_keys)
        || !authorize(r.next_signer_mask,r.next_signatures,r.next_keys,signing_hash(r,config_.context)))
      return status::invalid_authorization;
  }
  return status::accepted;
}
status chain::apply_block(uint64_t height, const std::vector<std::string> &payloads)
{
  if (!config_.h0) return status::disabled;
  if (!valid_) return status::invalid_configuration;
  if ((!tip_ && height!=0) || (tip_ && (*tip_==std::numeric_limits<uint64_t>::max() || height!=*tip_+1)))
    return status::nonsequential_height;
  if (payloads.size()>1 || (!payloads.empty() && records_.size()>=max_policy_records)) return status::resource_limit;
  if (!payloads.empty()) {
    if (height<*config_.h0) return status::before_h0;
    record r{}; if (!decode(payloads.front(),r)) return status::malformed_record;
    const status result=validate(r,height); if (result!=status::accepted) return result;
    const auto hash=digest(domain("QWC_RELEASE_POLICY_RECORD_1",config_.context)+payloads.front());
    records_.push_back({height,std::move(r),hash});
  }
  tip_=height;
  retained_from_=std::max(retained_from_,height>=undo_depth ? height-undo_depth+1 : uint64_t(0));
  return status::accepted;
}
bool chain::disconnect_tip()
{
  if (!valid_ || !tip_ || *tip_<retained_from_) return false;
  if (!records_.empty() && records_.back().height==*tip_) records_.pop_back();
  if (*tip_==0) tip_=boost::none; else --*tip_;
  return true;
}
bool chain::view(uint64_t epoch, epoch_view &out) const
{
  out={}; uint64_t lock=0;
  if (!valid_ || epoch<config_.first_enforced_epoch || !lock_height(epoch,lock) || !tip_ || *tip_<lock) return false;
  out.epoch=epoch; out.lock_height=lock;
  const entry *schedule=nullptr;
  std::set<uint64_t> cancelled;
  for (const auto &e:records_) if (e.height<=lock && e.value.op==operation::cancel)
    cancelled.insert(e.value.reference_sequence);
  for (const auto &e:records_) {
    if (e.height>lock) break;
    out.sequence=e.value.sequence;
    if (e.value.op!=operation::schedule || e.value.target_epoch>epoch) continue;
    if (!cancelled.count(e.value.sequence)) schedule=&e;
  }
  if (!schedule) return false;
  out.head=schedule->value.release_id;
  for (const auto &e:records_) if (e.value.op==operation::publish && e.value.release_id==out.head)
    out.minimum=e.value.predecessor;
  auto s=domain("QWC_RELEASE_EPOCH_BINDING_1",config_.context); u64(s,epoch); u64(s,lock);
  crypto::hash last{};
  for (const auto &e:records_) if (e.height<=lock) last=e.hash;
  put(s,last); out.policy_hash=digest(s);
  return out.minimum!=0;
}
status chain::eligible(uint64_t release, uint64_t epoch) const
{ return eligible(release,epoch,tip_ ? *tip_ : 0); }
status chain::eligible(uint64_t release, uint64_t epoch, uint64_t inclusion_height) const
{
  epoch_view v{};
  if (!view(epoch,v) || !tip_ || inclusion_height<v.lock_height || inclusion_height>*tip_)
    return status::policy_unavailable;
  bool found=false;
  for (const auto &e:records_) if (e.height<=inclusion_height && e.value.op==operation::publish
      && e.value.release_id==release) found=true;
  if (!found) return status::unknown_release;
  return release<v.minimum ? status::below_minimum : status::accepted;
}
crypto::hash chain::state_hash() const
{
  auto s=domain("QWC_RELEASE_STATE_1",config_.context);
  u8(s,tip_ ? 1 : 0); u64(s,tip_ ? *tip_ : 0);
  for (const auto &e:records_) { u64(s,e.height); put(s,e.hash); }
  return digest(s);
}
std::string encode_receipt(const receipt &r)
{
  // Serialize fields explicitly, without native integer padding or ABI layout.
  const auto &c=r.service.challenge;
  std::string s("QWCRR001",8); u8(s,r.version); u8(s,c.version); u8(s,c.service_kind);
  u64(s,c.epoch); u64(s,c.round); put(s,c.snapshot_hash); put(s,c.anchor_hash);
  put(s,c.subject_public_key); put(s,c.verifier_public_key); put(s,c.endpoint_descriptor_hash);
  put(s,c.nonce); put(s,c.requested_object_hash); put(s,r.service.response_object_hash);
  put(s,r.service.subject_signature); put(s,r.service.verifier_signature);
  u64(s,r.subject_release); u64(s,r.verifier_release); put(s,r.policy_hash);
  put(s,r.subject_claim); put(s,r.verifier_claim); return s;
}
bool decode_receipt(const std::string &s, const receipt_context_v2 &ctx, receipt &out)
{
  out={}; if (s.size()>max_record_bytes || s.size()<8 || s.substr(0,8)!="QWCRR001") return false;
  receipt r{}; auto &c=r.service.challenge; reader in{s,8};
  if (!in.get(r.version) || r.version!=1 || !in.get(c.version) || !in.get(c.service_kind)
      || !in.number(c.epoch) || !in.number(c.round) || !in.get(c.snapshot_hash) || !in.get(c.anchor_hash)
      || !in.get(c.subject_public_key) || !in.get(c.verifier_public_key) || !in.get(c.endpoint_descriptor_hash)
      || !in.get(c.nonce) || !in.get(c.requested_object_hash) || !in.get(r.service.response_object_hash)
      || !in.get(r.service.subject_signature) || !in.get(r.service.verifier_signature)
      || !in.number(r.subject_release) || !in.number(r.verifier_release) || !in.get(r.policy_hash)
      || !in.get(r.subject_claim) || !in.get(r.verifier_claim) || in.offset!=s.size()
      || !validate_authenticated_service_receipt_v2(r.service,ctx)) return false;
  out=std::move(r); return true;
}
void sign_subject(receipt &r, const crypto::secret_key &key, const receipt_context_v2 &ctx)
{ crypto::generate_signature(claim_hash(r,ctx,false),r.service.challenge.subject_public_key,key,r.subject_claim); }
void sign_verifier(receipt &r, const crypto::secret_key &key, const receipt_context_v2 &ctx)
{ crypto::generate_signature(claim_hash(r,ctx,true),r.service.challenge.verifier_public_key,key,r.verifier_claim); }
status apply_receipt(const receipt &r, const chain &policies, membership_pipeline_v2 &members,
    uint64_t height, const crypto::hash &anchor)
{
  epoch_view v{};
  if (!policies.view(r.service.challenge.epoch,v)) return status::policy_unavailable;
  if (r.version!=1 || r.policy_hash!=v.policy_hash) return status::wrong_policy;
  for (uint64_t id:{r.subject_release,r.verifier_release}) {
    const status result=policies.eligible(id,r.service.challenge.epoch,height);
    if (result!=status::accepted) return result;
  }
  const auto &ctx=policies.config().context;
  if (!crypto::check_signature(claim_hash(r,ctx,false),r.service.challenge.subject_public_key,r.subject_claim)
      || !crypto::check_signature(claim_hash(r,ctx,true),r.service.challenge.verifier_public_key,r.verifier_claim))
    return status::invalid_receipt;
  const auto result=members.apply_authenticated_receipt(r.service,ctx,height,anchor);
  return result==pipeline_status_v2::accepted || result==pipeline_status_v2::idempotent_duplicate
      ? status::accepted : status::invalid_receipt;
}
}}}
