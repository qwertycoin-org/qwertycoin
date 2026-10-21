// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause
#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include "../epose/prototype/release_policy.h"
#include "epose/lifecycle_v2.h"
#include "epose/reward_v2.h"
#include "epose/canonical_service_v2.h"
#include "ringct/rctOps.h"

namespace {
using namespace qwertycoin::epose;
namespace rp = release_policy_prototype;
crypto::hash hash(const std::string &s) { return crypto::cn_fast_hash(s.data(),s.size()); }
struct keys {
  crypto::public_key pub{}; crypto::secret_key secret{};
  keys() { crypto::generate_keys(pub,secret); } // TEST ONLY, ephemeral; never persisted
};
using authorities = std::array<keys,3>;
rp::key_set public_keys(const authorities &a) { return {{a[0].pub,a[1].pub,a[2].pub}}; }
rp::configuration config(const authorities &a) {
  rp::configuration c;
  c.context={cryptonote::TESTNET,hash("TEST ONLY genesis"),hash("TEST ONLY parameters")};
  c.timing={0,20,2}; c.h0=5; c.first_enforced_epoch=4;
  c.minimum_notice_blocks=20; c.authority=public_keys(a); return c;
}
struct driver {
  authorities auth;
  rp::configuration c=config(auth);
  rp::chain state{c};
  uint64_t next=0;
  std::vector<std::vector<std::string>> blocks;
  void empty_to(uint64_t height) {
    while (next<=height) { ASSERT_EQ(rp::status::accepted,state.apply_block(next,{})); blocks.push_back({}); ++next; }
  }
  rp::record make(rp::operation op) {
    rp::record r; r.op=op; r.sequence=state.next_sequence(); r.previous=state.tip_policy_hash(); r.signer_mask=3; return r;
  }
  void authorize(rp::record &r) {
    rp::sign(r,0,auth[0].secret,c.context); rp::sign(r,1,auth[1].secret,c.context);
  }
  void push(rp::record r) {
    authorize(r); auto bytes=rp::encode(r);
    ASSERT_EQ(rp::status::accepted,state.apply_block(next,{bytes})); blocks.push_back({bytes}); ++next;
  }
  void publish(uint64_t id,uint64_t prev,const std::string &label) {
    auto r=make(rp::operation::publish); r.release_id=id; r.predecessor=prev; r.label=label; push(r);
  }
  void schedule(uint64_t head,uint64_t epoch) {
    auto r=make(rp::operation::schedule); r.release_id=head; r.target_epoch=epoch; push(r);
  }
  void bootstrap() {
    empty_to(4); publish(1,0,"2.0.4"); publish(2,1,"2.0.5"); schedule(2,4);
  }
  rp::status try_record(rp::record r) { authorize(r); return state.apply_block(next,{rp::encode(r)}); }
};
TEST(epose_release_policy_prototype, explicit_disabled_mainnet_and_h0_boundaries) {
  authorities a; auto c=config(a); c.h0=boost::none; rp::chain off(c);
  EXPECT_EQ(rp::status::disabled,off.apply_block(0,{}));
  c=config(a); c.context.nettype=cryptonote::MAINNET; EXPECT_FALSE(rp::chain(c).valid());
  c=config(a); c.h0=0; EXPECT_FALSE(rp::chain(c).valid());
  c=config(a); c.h0=std::numeric_limits<uint64_t>::max(); EXPECT_FALSE(rp::chain(c).valid());
  driver d; d.empty_to(3); auto r=d.make(rp::operation::publish); r.release_id=1; r.label="2.0.4";
  EXPECT_EQ(rp::status::before_h0,d.try_record(r)); // H0-1
  d.empty_to(4); d.push(r); // H0
  d.publish(2,1,"2.0.5"); // H0+1
}
TEST(epose_release_policy_prototype, stable_order_skipped_semver_and_early_updaters) {
  driver d; d.bootstrap(); d.publish(9,2,"2.1.0"); d.schedule(9,6); d.empty_to(119);
  EXPECT_EQ(rp::status::accepted,d.state.eligible(1,4));
  EXPECT_EQ(rp::status::accepted,d.state.eligible(9,4)); // registered stable early updater
  EXPECT_EQ(rp::status::below_minimum,d.state.eligible(1,6));
  EXPECT_EQ(rp::status::accepted,d.state.eligible(2,6));
  EXPECT_EQ(rp::status::unknown_release,d.state.eligible(3,6)); // skipped ordinal is not registered
  EXPECT_EQ(rp::status::unknown_release,d.state.eligible(999999,6));
  EXPECT_EQ(rp::status::unknown_release,d.state.eligible(0,6));
  rp::epoch_view v; ASSERT_TRUE(d.state.view(6,v)); EXPECT_EQ(2u,v.minimum); EXPECT_EQ(9u,v.head);
}
TEST(epose_release_policy_prototype, rejects_prereleases_duplicates_bad_predecessors_and_overlong_labels) {
  driver d; d.bootstrap();
  for (const std::string label:{"2.1.0-rc1","2.1.0-beta","nightly","2.1.0+canary","2.0.5","2.1","2..1"}) {
    auto r=d.make(rp::operation::publish); r.release_id=3; r.predecessor=2; r.label=label;
    EXPECT_EQ(rp::status::invalid_release,d.try_record(r)) << label;
  }
  auto r=d.make(rp::operation::publish); r.release_id=2; r.predecessor=2; r.label="2.1.0";
  EXPECT_EQ(rp::status::invalid_release,d.try_record(r));
  r.release_id=3; r.predecessor=17; EXPECT_EQ(rp::status::invalid_release,d.try_record(r));
  r.label=std::string(33,'1'); EXPECT_TRUE(rp::encode(r).empty());
}
TEST(epose_release_policy_prototype, rejects_insufficient_wrong_duplicate_and_unused_signatures) {
  driver d; d.empty_to(4); auto r=d.make(rp::operation::publish); r.release_id=1; r.label="2.0.4";
  auto check=[&](rp::record bad) { EXPECT_EQ(rp::status::invalid_authorization,d.state.apply_block(d.next,{rp::encode(bad)})); };
  r.signer_mask=1; d.authorize(r); check(r);
  r.signer_mask=3; d.authorize(r); auto bad=r; bad.signatures[1]=bad.signatures[0]; check(bad);
  bad=r; bad.signatures[1]={}; check(bad);
  bad=r; bad.signatures[2]=bad.signatures[0]; check(bad);
  bad=r; bad.signer_mask=11; check(bad);
  bad=r; keys stranger; rp::sign(bad,1,stranger.secret,d.c.context); check(bad);
  EXPECT_EQ(rp::status::accepted,d.state.apply_block(d.next,{rp::encode(r)}));
}
TEST(epose_release_policy_prototype, network_genesis_parameters_and_policy_replay_are_bound) {
  driver d; d.empty_to(4); auto r=d.make(rp::operation::publish); r.release_id=1; r.label="2.0.4";
  for (unsigned i=0;i<3;++i) {
    auto ctx=d.c.context; if (i==0) ctx.nettype=cryptonote::STAGENET;
    if (i==1) ctx.genesis_hash=hash("other");
    if (i==2) ctx.parameter_set_hash=hash("other");
    rp::sign(r,0,d.auth[0].secret,ctx); rp::sign(r,1,d.auth[1].secret,ctx);
    EXPECT_EQ(rp::status::invalid_authorization,d.state.apply_block(d.next,{rp::encode(r)}));
  }
  d.push(r); EXPECT_EQ(rp::status::bad_continuation,d.try_record(r));
}
TEST(epose_release_policy_prototype, first_canonical_continuation_wins_and_failures_are_atomic) {
  driver d; d.bootstrap(); auto a=d.make(rp::operation::publish); a.release_id=3; a.predecessor=2; a.label="2.1.0";
  auto b=a; b.release_id=4; b.label="2.2.0"; d.push(a);
  const auto before=d.state.state_hash(); EXPECT_EQ(rp::status::bad_continuation,d.try_record(b));
  EXPECT_EQ(before,d.state.state_hash());
  EXPECT_EQ(rp::status::resource_limit,d.state.apply_block(d.next,{rp::encode(a),rp::encode(b)}));
  EXPECT_EQ(before,d.state.state_hash());
}
TEST(epose_release_policy_prototype, notice_is_checked_against_admission_lock_not_service_height) {
  driver d; d.bootstrap(); d.publish(3,2,"2.1.0"); d.empty_to(78);
  auto r=d.make(rp::operation::schedule); r.release_id=3; r.target_epoch=6; // lock 99
  d.push(r); // Hp=79; exact minimum 20 before lock accepted
  driver late; late.bootstrap(); late.publish(3,2,"2.1.0"); late.empty_to(79);
  r=late.make(rp::operation::schedule); r.release_id=3; r.target_epoch=6;
  EXPECT_EQ(rp::status::too_late,late.try_record(r)); // Hp=80
  r.target_epoch=std::numeric_limits<uint64_t>::max(); EXPECT_EQ(rp::status::too_late,late.try_record(r));
}
TEST(epose_release_policy_prototype, epochs_are_frozen_before_admission_and_not_reinterpreted) {
  driver d; d.bootstrap(); d.empty_to(59); rp::epoch_view original; ASSERT_TRUE(d.state.view(4,original));
  d.publish(3,2,"2.1.0"); d.schedule(3,6); d.empty_to(120);
  rp::epoch_view later; ASSERT_TRUE(d.state.view(4,later)); EXPECT_EQ(original.policy_hash,later.policy_hash);
  EXPECT_EQ(rp::status::policy_unavailable,d.state.eligible(1,4,58)); // never use a future lock
  EXPECT_EQ(rp::status::unknown_release,d.state.eligible(3,4,59)); // unavailable before publication
  EXPECT_EQ(rp::status::accepted,d.state.eligible(3,4,80)); // early updater after publication
  EXPECT_EQ(rp::status::accepted,d.state.eligible(3,5));
  EXPECT_EQ(rp::status::accepted,d.state.eligible(1,5));
  EXPECT_EQ(rp::status::below_minimum,d.state.eligible(1,6));
}
TEST(epose_release_policy_prototype, pending_cancel_is_bounded_and_cannot_rewrite_history) {
  driver d; d.bootstrap(); d.publish(3,2,"2.1.0"); d.schedule(3,6); const auto seq=d.state.next_sequence()-1;
  auto cancel=d.make(rp::operation::cancel); cancel.reference_sequence=seq; d.push(cancel);
  d.empty_to(120); EXPECT_EQ(rp::status::accepted,d.state.eligible(1,6));
  driver late; late.bootstrap(); late.publish(3,2,"2.1.0"); late.schedule(3,6); late.empty_to(79);
  cancel=late.make(rp::operation::cancel); cancel.reference_sequence=5;
  EXPECT_EQ(rp::status::too_late,late.try_record(cancel));
  driver initial; initial.bootstrap(); cancel=initial.make(rp::operation::cancel); cancel.reference_sequence=3;
  EXPECT_EQ(rp::status::invalid_schedule,initial.try_record(cancel));
}
TEST(epose_release_policy_prototype, rotation_needs_old_and_new_quorums_and_rejects_old_generation) {
  driver d; d.bootstrap(); authorities fresh;
  auto r=d.make(rp::operation::rotate); r.next_keys=public_keys(fresh); r.next_signer_mask=3;
  EXPECT_EQ(rp::status::invalid_authorization,d.try_record(r));
  rp::sign(r,0,fresh[0].secret,d.c.context,true); rp::sign(r,1,fresh[1].secret,d.c.context,true); d.push(r);
  auto next=d.make(rp::operation::publish); next.release_id=3; next.predecessor=2; next.label="2.1.0";
  EXPECT_EQ(rp::status::invalid_authorization,d.try_record(next));
  next.generation=1; rp::sign(next,0,fresh[0].secret,d.c.context); rp::sign(next,1,fresh[1].secret,d.c.context);
  EXPECT_EQ(rp::status::accepted,d.state.apply_block(d.next,{rp::encode(next)}));
}
std::vector<crypto::public_key> weak_authority_keys() {
  const auto identity=rct::rct2pk(rct::identity());
  const crypto::public_key order_four{};
  keys ordinary;
  const auto mixed=rct::rct2pk(rct::addKeys(rct::pk2rct(ordinary.pub),rct::pk2rct(order_four)));
  return {identity,order_four,mixed};
}
TEST(epose_release_policy_prototype, authority_configuration_rejects_identity_torsion_and_mixed_order_keys) {
  authorities a;
  for (const auto &bad:weak_authority_keys()) {
    ASSERT_TRUE(crypto::check_key(bad)); // curve decoding alone is insufficient
    for (size_t slot=0;slot<3;++slot) {
      auto c=config(a); c.authority[slot]=bad;
      rp::chain state(c);
      EXPECT_FALSE(state.valid());
      EXPECT_EQ(rp::status::invalid_configuration,state.apply_block(0,{}));
    }
  }
}
TEST(epose_release_policy_prototype, authority_configuration_rejects_malformed_and_duplicate_keys) {
  authorities a;
  crypto::public_key malformed;
  std::memset(&malformed,0xff,sizeof(malformed));
  ASSERT_FALSE(crypto::check_key(malformed));
  for (size_t slot=0;slot<3;++slot) {
    auto c=config(a); c.authority[slot]=malformed;
    EXPECT_FALSE(rp::chain(c).valid());
    c=config(a); c.authority[slot]=c.authority[(slot+1)%3];
    EXPECT_FALSE(rp::chain(c).valid());
  }
}
TEST(epose_release_policy_prototype, identity_authority_signature_needs_no_secret_and_must_not_count) {
  authorities a; auto c=config(a);
  const crypto::secret_key public_zero_scalar{};
  ASSERT_TRUE(crypto::secret_key_to_public_key(public_zero_scalar,c.authority[0]));
  ASSERT_EQ(rct::rct2pk(rct::identity()),c.authority[0]);
  rp::record r; r.sequence=1; r.release_id=1; r.label="2.0.4"; r.signer_mask=3;
  // An identity slot contributes a valid signature using only the public zero
  // scalar: accepting this key would reduce 2-of-3 to one real signing secret.
  rp::sign(r,0,public_zero_scalar,c.context); rp::sign(r,1,a[1].secret,c.context);
  ASSERT_TRUE(crypto::check_signature(rp::signing_hash(r,c.context),c.authority[0],r.signatures[0]));
  ASSERT_TRUE(crypto::check_signature(rp::signing_hash(r,c.context),c.authority[1],r.signatures[1]));
  rp::chain state(c);
  EXPECT_FALSE(state.valid());
  EXPECT_EQ(rp::status::invalid_configuration,state.apply_block(0,{}));
}
TEST(epose_release_policy_prototype, rotation_rejects_weak_keys_including_the_unused_slot_atomically) {
  for (const auto &bad:weak_authority_keys()) {
    for (size_t slot=0;slot<3;++slot) {
      driver d; d.bootstrap(); authorities fresh;
      auto r=d.make(rp::operation::rotate); r.next_keys=public_keys(fresh);
      r.next_keys[slot]=bad;
      r.next_signer_mask=static_cast<uint8_t>(7u ^ (1u<<slot));
      for (size_t i=0;i<3;++i) if (i!=slot)
        rp::sign(r,i,fresh[i].secret,d.c.context,true);
      const auto before=d.state.state_hash();
      EXPECT_EQ(rp::status::invalid_authorization,d.try_record(r));
      EXPECT_EQ(before,d.state.state_hash());
      // Rejection must not advance the key generation or policy sequence.
      d.publish(3,2,"2.1.0");
    }
  }
}
TEST(epose_release_policy_prototype, parser_is_bounded_canonical_and_failure_atomic) {
  driver d; d.bootstrap(); auto r=d.make(rp::operation::publish); r.release_id=3; r.predecessor=2; r.label="2.1.0"; d.authorize(r);
  const auto bytes=rp::encode(r); EXPECT_EQ(573u+r.label.size(),bytes.size()); rp::record out;
  ASSERT_TRUE(rp::decode(bytes,out)); EXPECT_EQ(bytes,rp::encode(out));
  for (size_t n=0;n<bytes.size();++n) { EXPECT_FALSE(rp::decode(bytes.substr(0,n),out)); EXPECT_EQ(0u,out.sequence); }
  EXPECT_FALSE(rp::decode(bytes+"x",out)); EXPECT_FALSE(rp::decode(std::string(769,'x'),out));
  auto unknown=bytes; unknown[8]=2; EXPECT_FALSE(rp::decode(unknown,out));
}
TEST(epose_release_policy_prototype, replay_restart_and_reorg_reconstruct_policy_state) {
  driver d; d.bootstrap(); d.publish(3,2,"2.1.0"); d.schedule(3,6); d.empty_to(120);
  rp::chain restart(d.c), other_validator(d.c);
  for (size_t h=0;h<d.blocks.size();++h) {
    ASSERT_EQ(rp::status::accepted,restart.apply_block(h,d.blocks[h]));
    ASSERT_EQ(rp::status::accepted,other_validator.apply_block(h,d.blocks[h]));
  }
  EXPECT_EQ(d.state.state_hash(),restart.state_hash()); EXPECT_EQ(restart.state_hash(),other_validator.state_hash());
  for (uint64_t h=120;h>=9;--h) ASSERT_TRUE(d.state.disconnect_tip()); // undo schedule across activation
  EXPECT_EQ(rp::status::accepted,d.state.apply_block(9,{}));
  for (uint64_t h=10;h<=120;++h) ASSERT_EQ(rp::status::accepted,d.state.apply_block(h,{}));
  EXPECT_EQ(rp::status::accepted,d.state.eligible(1,6));
  EXPECT_EQ(rp::status::below_minimum,restart.eligible(1,6));
  for (uint64_t h=121;h<=200;++h) ASSERT_EQ(rp::status::accepted,restart.apply_block(h,{}));
  for (unsigned n=0;n<128;++n) ASSERT_TRUE(restart.disconnect_tip());
  EXPECT_FALSE(restart.disconnect_tip());
}

struct node {
  keys service, operator_key;
  frozen_member_v2 member{};
  explicit node(const receipt_context_v2 &ctx) {
    member.service_public_key=service.pub; member.operator_authorization_public_key=operator_key.pub;
    member.identity_id=derive_identity_id_v2(ctx.nettype,ctx.genesis_hash,ctx.parameter_set_hash,operator_key.pub);
    member.descriptor_hash=hash("descriptor"+std::string(reinterpret_cast<const char *>(&service.pub),32));
    member.reward_binding_hash=hash("reward"); member.endpoint_descriptor_hash=hash("endpoint"); member.sequence=1;
  }
};
committee_policy_v2 committee() { return {3,2,3,2,1,{0,5,10},100}; }
membership_pipeline_v2 pipeline(const rp::configuration &c) {
  return {c.context.nettype,c.context.genesis_hash,c.context.parameter_set_hash,c.timing,
      {admission_work_algorithm_v2::randomx,1,1},committee()};
}
void enroll(membership_pipeline_v2 &p,const rp::configuration &c,const std::vector<node> &nodes,uint64_t epoch) {
  admission_context_v2 ctx{c.context.nettype,c.context.genesis_hash,c.context.parameter_set_hash,
      (epoch-1)*20,hash("context"+std::to_string(epoch))};
  for (const auto &n:nodes) {
    admission_lease_v2 lease; lease.member=n.member; lease.target_epoch=epoch; lease.work_algorithm=1;
    lease.leading_zero_bits=1; lease.admission_context_height=ctx.height; lease.admission_context_hash=ctx.block_hash;
    do { lease.work_hash=calculate_admission_work_v2(lease,ctx); if (admission_work_meets_target_v2(lease.work_hash,1)) break; ++lease.nonce; } while (true);
    lease.lease_hash=calculate_admission_lease_hash_v2(lease,ctx);
    ASSERT_EQ(pipeline_status_v2::accepted,p.apply_admission(lease,ctx,ctx.height+1));
  }
  ASSERT_EQ(pipeline_status_v2::accepted,p.freeze_membership(epoch,epoch*20-2,hash("snapshot"+std::to_string(epoch))));
}
rp::receipt proof(const driver &d,const membership_pipeline_v2 &p,const node &subject,const node &verifier,
    uint64_t epoch,uint64_t round,uint64_t subject_id,uint64_t verifier_id) {
  rp::receipt r; auto &s=r.service; auto &c=s.challenge;
  c.epoch=epoch; c.round=round; c.subject_public_key=subject.service.pub; c.verifier_public_key=verifier.service.pub;
  c.snapshot_hash=p.snapshot(epoch)->snapshot_hash;
  c.anchor_hash=round==0 ? p.snapshot(epoch)->anchor_hash : hash("round"+std::to_string(epoch)+std::to_string(round));
  c.endpoint_descriptor_hash=subject.member.endpoint_descriptor_hash; c.nonce=hash("nonce");
  c.requested_object_hash=hash("canonical object"); s.response_object_hash=c.requested_object_hash;
  sign_subject_response_v2(s,subject.service.secret,d.c.context); sign_verifier_receipt_v2(s,verifier.service.secret,d.c.context);
  r.subject_release=subject_id; r.verifier_release=verifier_id; rp::epoch_view v;
  EXPECT_TRUE(d.state.view(epoch,v)); r.policy_hash=v.policy_hash;
  rp::sign_subject(r,subject.service.secret,d.c.context); rp::sign_verifier(r,verifier.service.secret,d.c.context); return r;
}
const node &find_node(const std::vector<node> &nodes,const crypto::public_key &key) {
  return *std::find_if(nodes.begin(),nodes.end(),[&](const node &n){return n.service.pub==key;});
}
void fulfill(const driver &d,membership_pipeline_v2 &p,const std::vector<node> &nodes,const node &subject,
    uint64_t epoch,uint64_t subject_id,uint64_t verifier_id,rp::status expected=rp::status::accepted) {
  for (uint64_t round=0;round<2;++round) {
    const auto anchor=round==0 ? p.snapshot(epoch)->anchor_hash : hash("round"+std::to_string(epoch)+std::to_string(round));
    for (const auto &assignment:p.committee(epoch,round,subject.service.pub,anchor)) {
      const auto r=proof(d,p,subject,find_node(nodes,assignment.verifier_public_key),epoch,round,subject_id,verifier_id);
      EXPECT_EQ(expected,rp::apply_receipt(r,d.state,p,epoch*20+round*5+(round ? 1 : 0),anchor));
    }
  }
}
TEST(epose_release_policy_prototype, real_admission_qualification_payout_loss_and_same_identity_return) {
  driver d; d.bootstrap(); d.publish(3,2,"2.1.0"); d.schedule(3,6); d.empty_to(160);
  std::vector<node> nodes; for (unsigned i=0;i<4;++i) nodes.emplace_back(d.c.context);
  auto p=pipeline(d.c);
  for (uint64_t epoch:{4,5,6,7}) {
    enroll(p,d.c,nodes,epoch);
    fulfill(d,p,nodes,nodes[0],epoch,epoch==7 ? 2 : 1,2,
        epoch==6 ? rp::status::below_minimum : rp::status::accepted);
    ASSERT_EQ(pipeline_status_v2::accepted,p.close_qualification(epoch,epoch*20+17));
    EXPECT_EQ(epoch==6 ? 0u : 1u,p.qualification(epoch)->qualified_nodes.size());
  }
  crypto::public_key payee;
  EXPECT_EQ(reward_status_v2::accepted,select_service_payee_v2(*p.qualification(5),hash("payout seed"),
      d.c.context.genesis_hash,d.c.context.parameter_set_hash,d.c.timing,6,120,payee));
  EXPECT_EQ(nodes[0].service.pub,payee); // old entitlement still payable, even after policy activation
  EXPECT_EQ(nodes[0].service.pub,p.qualification(7)->qualified_nodes.front());
  reward_allocation_v2 allocation;
  ASSERT_EQ(reward_status_v2::accepted,calculate_reward_allocation_v2(1000,7,false,
      empty_qualification_policy_v2::miner_fallback,allocation));
  EXPECT_EQ(0u,allocation.service_reward); EXPECT_EQ(1007u,allocation.coinbase_total);
}
TEST(epose_release_policy_prototype, claims_bind_both_roles_policy_epoch_and_original_transcript) {
  driver d; d.bootstrap(); d.empty_to(100); std::vector<node> nodes;
  for (unsigned i=0;i<4;++i) nodes.emplace_back(d.c.context);
  auto p=pipeline(d.c); enroll(p,d.c,nodes,4);
  auto r=proof(d,p,nodes[0],nodes[1],4,0,1,2); const auto anchor=r.service.challenge.anchor_hash;
  auto reject=[&](rp::receipt bad) { EXPECT_NE(rp::status::accepted,rp::apply_receipt(bad,d.state,p,80,anchor)); };
  auto bad=r; bad.subject_release=2; rp::sign_verifier(bad,nodes[1].service.secret,d.c.context); reject(bad);
  bad=r; bad.subject_claim={}; rp::sign_verifier(bad,nodes[1].service.secret,d.c.context); reject(bad);
  bad=r; bad.verifier_release=1; reject(bad);
  bad=r; bad.policy_hash=hash("other"); reject(bad);
  bad=r; bad.service.challenge.epoch=5; reject(bad);
  bad=r; std::swap(bad.service.challenge.subject_public_key,bad.service.challenge.verifier_public_key); reject(bad);
  bad=r; bad.subject_claim=r.verifier_claim; reject(bad);
  bad=r; bad.service.response_object_hash=hash("wrong object"); reject(bad);
  bad=r; bad.version=2; reject(bad);
  EXPECT_EQ(0u,p.receipt_count());
  EXPECT_EQ(rp::status::accepted,rp::apply_receipt(r,d.state,p,80,anchor));
}
TEST(epose_release_policy_prototype, receipt_wire_roundtrip_and_no_trailing_or_unknown_version) {
  driver d; d.bootstrap(); d.empty_to(100); std::vector<node> nodes;
  for (unsigned i=0;i<4;++i) nodes.emplace_back(d.c.context);
  auto p=pipeline(d.c); enroll(p,d.c,nodes,4); const auto r=proof(d,p,nodes[0],nodes[1],4,0,1,2);
  const auto bytes=rp::encode_receipt(r); EXPECT_EQ(587u,bytes.size()); rp::receipt out;
  ASSERT_TRUE(rp::decode_receipt(bytes,d.c.context,out)); EXPECT_EQ(bytes,rp::encode_receipt(out));
  for (size_t n=0;n<bytes.size();++n) EXPECT_FALSE(rp::decode_receipt(bytes.substr(0,n),d.c.context,out));
  EXPECT_FALSE(rp::decode_receipt(bytes+"x",d.c.context,out));
  auto unknown=bytes; unknown[8]=2; EXPECT_FALSE(rp::decode_receipt(unknown,d.c.context,out));
}
TEST(epose_release_policy_prototype, old_verifiers_missing_rounds_and_small_populations_cannot_qualify) {
  driver d; d.bootstrap(); d.publish(3,2,"2.1.0"); d.schedule(3,6); d.empty_to(140);
  std::vector<node> nodes; for (unsigned i=0;i<4;++i) nodes.emplace_back(d.c.context);
  auto p=pipeline(d.c); enroll(p,d.c,nodes,6);
  fulfill(d,p,nodes,nodes[0],6,2,1,rp::status::below_minimum);
  ASSERT_EQ(pipeline_status_v2::accepted,p.close_qualification(6,137));
  EXPECT_TRUE(p.qualification(6)->qualified_nodes.empty());
  auto alone=pipeline(d.c); std::vector<node> one{nodes.front()}; enroll(alone,d.c,one,6);
  ASSERT_EQ(pipeline_status_v2::accepted,alone.close_qualification(6,137));
  EXPECT_TRUE(alone.qualification(6)->qualified_nodes.empty());
}
TEST(epose_release_policy_prototype, legacy_official_evidence_has_no_authenticated_release_claim) {
  driver d; d.bootstrap(); d.empty_to(100); std::vector<node> nodes;
  for (unsigned i=0;i<4;++i) nodes.emplace_back(d.c.context);
  auto p=pipeline(d.c); enroll(p,d.c,nodes,4);
  const auto r=proof(d,p,nodes[0],nodes[1],4,0,1,2);
  envelope_record_v2 legacy;
  ASSERT_EQ(record_codec_status_v2::accepted,encode_service_receipt_record_v2(r.service,d.c.context,legacy));
  rp::receipt missing;
  EXPECT_FALSE(rp::decode_receipt(legacy.payload,d.c.context,missing));
  missing.service=r.service; missing.policy_hash=r.policy_hash;
  EXPECT_EQ(rp::status::unknown_release,rp::apply_receipt(missing,d.state,p,80,r.service.challenge.anchor_hash));
  EXPECT_EQ(0u,p.receipt_count());
  // Existing raw v2 historical verification remains supported by production code.
  EXPECT_TRUE(validate_authenticated_service_receipt_v2(r.service,d.c.context));
}
TEST(epose_release_policy_prototype, real_registered_id_spoof_is_a_demonstrated_bypass_not_binary_attestation) {
  driver d; d.bootstrap(); d.publish(3,2,"2.1.0"); d.schedule(3,6); d.empty_to(140);
  std::vector<node> nodes; for (unsigned i=0;i<4;++i) nodes.emplace_back(d.c.context);
  auto p=pipeline(d.c); enroll(p,d.c,nodes,6);
  fulfill(d,p,nodes,nodes[0],6,2,2); // same service implementation; only claimed release changes
  ASSERT_EQ(pipeline_status_v2::accepted,p.close_qualification(6,137));
  ASSERT_EQ(1u,p.qualification(6)->qualified_nodes.size());
  const auto r=proof(d,p,nodes[0],nodes[1],6,0,9999,2);
  EXPECT_EQ(rp::status::unknown_release,rp::apply_receipt(r,d.state,p,120,r.service.challenge.anchor_hash));
}
TEST(epose_release_policy_prototype, a_forged_version_cannot_make_missing_service_bytes_available) {
  driver d; d.bootstrap(); d.empty_to(100); std::vector<node> nodes;
  for (unsigned i=0;i<4;++i) nodes.emplace_back(d.c.context);
  auto p=pipeline(d.c); enroll(p,d.c,nodes,4); auto r=proof(d,p,nodes[0],nodes[1],4,0,2,2);
  canonical_service_response_v2 response;
  EXPECT_EQ(canonical_service_status_v2::object_not_found,answer_canonical_block_challenge_v2(
      r.service.challenge,d.c.context,{1024},nodes[0].service.secret,
      [](const service_challenge_v2 &,const receipt_context_v2 &){return true;},
      [](const crypto::hash &,cryptonote::blobdata &){return false;},response));
  // This is a verifier-side availability check, NOT a new consensus proof of remote execution.
}
TEST(epose_release_policy_prototype, replica_replay_and_qualification_undo_do_not_depend_on_endpoint_cache) {
  driver d; d.bootstrap(); d.publish(3,2,"2.1.0"); d.schedule(3,6); d.empty_to(140);
  rp::chain replay(d.c); for (size_t h=0;h<d.blocks.size();++h) ASSERT_EQ(rp::status::accepted,replay.apply_block(h,d.blocks[h]));
  std::vector<node> nodes; for (unsigned i=0;i<4;++i) nodes.emplace_back(d.c.context);
  auto a=pipeline(d.c),b=pipeline(d.c); enroll(a,d.c,nodes,6); enroll(b,d.c,nodes,6);
  const auto before=a.checkpoint(); auto undo=before;
  // Replay the same complete receipt bytes independently; no descriptor cache or DNS/HTTP input exists.
  for (uint64_t round=0;round<2;++round) {
    const auto anchor=round==0 ? a.snapshot(6)->anchor_hash : hash("round61");
    for (const auto &assignment:a.committee(6,round,nodes[0].service.pub,anchor)) {
      const auto r=proof(d,a,nodes[0],find_node(nodes,assignment.verifier_public_key),6,round,2,2);
      rp::receipt decoded; ASSERT_TRUE(rp::decode_receipt(rp::encode_receipt(r),d.c.context,decoded));
      ASSERT_EQ(rp::status::accepted,rp::apply_receipt(decoded,d.state,a,120+round*5+(round?1:0),anchor));
      ASSERT_EQ(rp::status::accepted,rp::apply_receipt(decoded,replay,b,120+round*5+(round?1:0),anchor));
    }
  }
  ASSERT_EQ(pipeline_status_v2::accepted,a.close_qualification(6,137,&undo));
  ASSERT_EQ(pipeline_status_v2::accepted,b.close_qualification(6,137)); EXPECT_EQ(a.state_hash(),b.state_hash());
  a.rollback(undo); EXPECT_EQ(nullptr,a.qualification(6)); EXPECT_EQ(0u,a.receipt_count());
}
TEST(epose_release_policy_prototype, activation_round_and_deadline_boundary_blocks) {
  driver d; d.bootstrap(); d.publish(3,2,"2.1.0"); d.schedule(3,6);
  for (uint64_t h:{119,120,121}) {
    d.empty_to(h);
    EXPECT_EQ(h==119 ? rp::status::accepted : rp::status::below_minimum,
        d.state.eligible(1,h/20)); // H-1 / H / H+1
  }
  d.empty_to(138); // canonical prefix must exist before testing later receipt heights
  std::vector<node> nodes; for (unsigned i=0;i<4;++i) nodes.emplace_back(d.c.context);
  auto p=pipeline(d.c); enroll(p,d.c,nodes,6);
  auto r=proof(d,p,nodes[0],nodes[1],6,0,2,2); const auto a=r.service.challenge.anchor_hash;
  EXPECT_EQ(rp::status::invalid_receipt,rp::apply_receipt(r,d.state,p,119,a));
  EXPECT_EQ(rp::status::accepted,rp::apply_receipt(r,d.state,p,120,a));
  EXPECT_EQ(rp::status::accepted,rp::apply_receipt(r,d.state,p,121,a));
  r=proof(d,p,nodes[0],nodes[1],6,1,2,2);
  EXPECT_EQ(rp::status::invalid_receipt,rp::apply_receipt(r,d.state,p,125,r.service.challenge.anchor_hash));
  EXPECT_EQ(rp::status::accepted,rp::apply_receipt(r,d.state,p,126,r.service.challenge.anchor_hash));
  r=proof(d,p,nodes[0],nodes[1],6,2,2,2);
  EXPECT_EQ(rp::status::accepted,rp::apply_receipt(r,d.state,p,137,r.service.challenge.anchor_hash));
  EXPECT_EQ(rp::status::invalid_receipt,rp::apply_receipt(r,d.state,p,138,r.service.challenge.anchor_hash));
  EXPECT_EQ(pipeline_status_v2::wrong_boundary,p.close_qualification(6,136));
  EXPECT_EQ(pipeline_status_v2::accepted,p.close_qualification(6,137));
  EXPECT_EQ(pipeline_status_v2::qualification_already_closed,p.close_qualification(6,138));
}
TEST(epose_release_policy_prototype, policy_archive_limit_and_uint64_ids_fail_closed) {
  driver d; d.empty_to(4);
  for (uint64_t id=1;id<=rp::max_policy_records;++id)
    d.publish(id,id-1,"0.0."+std::to_string(id));
  const auto before=d.state.state_hash(); auto r=d.make(rp::operation::publish);
  r.release_id=rp::max_policy_records+1; r.predecessor=rp::max_policy_records; r.label="1.0.0";
  EXPECT_EQ(rp::status::resource_limit,d.try_record(r)); EXPECT_EQ(before,d.state.state_hash());
  driver exhausted; exhausted.empty_to(4);
  exhausted.publish(std::numeric_limits<uint64_t>::max(),0,"1.0.0");
  r=exhausted.make(rp::operation::publish); r.release_id=0;
  r.predecessor=std::numeric_limits<uint64_t>::max(); r.label="2.0.0";
  EXPECT_EQ(rp::status::invalid_release,exhausted.try_record(r));
}
TEST(epose_release_policy_prototype, post_lock_publication_admits_early_updater_without_retroactive_rescue) {
  driver d; d.bootstrap(); d.empty_to(80); rp::epoch_view before;
  ASSERT_TRUE(d.state.view(4,before));
  d.publish(3,2,"2.1.0"); // publication at 81, well after E4 lock at 59
  d.empty_to(100); rp::epoch_view after; ASSERT_TRUE(d.state.view(4,after));
  EXPECT_EQ(before.policy_hash,after.policy_hash); EXPECT_EQ(before.minimum,after.minimum);
  EXPECT_EQ(rp::status::unknown_release,d.state.eligible(3,4,80));
  EXPECT_EQ(rp::status::accepted,d.state.eligible(3,4,81));
  std::vector<node> nodes; for (unsigned i=0;i<4;++i) nodes.emplace_back(d.c.context);
  auto p=pipeline(d.c); enroll(p,d.c,nodes,4);
  auto r=proof(d,p,nodes[0],nodes[1],4,0,3,2);
  EXPECT_EQ(rp::status::unknown_release,rp::apply_receipt(r,d.state,p,80,r.service.challenge.anchor_hash));
  EXPECT_EQ(rp::status::accepted,rp::apply_receipt(r,d.state,p,81,r.service.challenge.anchor_hash));
}
} // namespace
