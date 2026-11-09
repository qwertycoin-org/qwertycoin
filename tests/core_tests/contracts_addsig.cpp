#include "contracts.h"
#include "../unit_tests/vm_test_utils.h"

#include "cryptonote_basic/cryptonote_format_utils.h"
#include "wallet/wallet2.h"

using namespace cryptonote;

namespace
{
  std::string key_bytes(const crypto::public_key& key)
  {
    return std::string(reinterpret_cast<const char*>(&key), sizeof(key));
  }

}

gen_contracts_stored_signer_changes::gen_contracts_stored_signer_changes()
{
  REGISTER_CALLBACK_METHOD(gen_contracts_stored_signer_changes, check_original_call);
  REGISTER_CALLBACK_METHOD(gen_contracts_stored_signer_changes, check_changed_call);
}

bool gen_contracts_stored_signer_changes::generate(std::vector<test_event_entry>& events) const
{
  ring_chain chain;
  CHECK_AND_ASSERT_MES(generate_ring_chain(events, chain), false, "ring chain");
  const std::string code = vm_test::stored_signer_contract();
  const crypto::hash contract_id = vm::contract_id_for(code, {});
  tx_extra_vm_call deployment = deploy_call();
  deployment.code = code;
  transaction deploy;
  CHECK_AND_ASSERT_MES(construct_call_tx(chain, deployment, deploy, 0), false, "deployment");
  push_kept_txs(events, {deploy});
  block deployed;
  CHECK_AND_ASSERT_MES(construct_next_block(chain, chain.tip, chain.miner, fork_version, {deploy}, deployed), false, "deployment block");
  events.push_back(deployed);

  const auto signer_for = [&](size_t index) {
    crypto::public_key public_key{};
    crypto::secret_key secret_key{};
    CHECK_AND_ASSERT_MES(derive_contract_key(chain.ring_miners[index].get_keys(), contract_id, crypto::null_hash, public_key, secret_key), crypto::null_pkey, "derived key");
    return public_key;
  };
  const auto call_for = [&](uint32_t method, const std::string& args) {
    tx_extra_vm_call call = invoke_call(method, args);
    call.contract_id = contract_id;
    return call;
  };
  transaction first_owner;
  CHECK_AND_ASSERT_MES(construct_call_tx(chain, call_for(2, vm_test::stored_owner_args(signer_for(2))), first_owner, 1), false, "first owner");
  push_kept_txs(events, {first_owner});
  block initialized;
  CHECK_AND_ASSERT_MES(construct_next_block(chain, deployed, chain.miner, fork_version, {first_owner}, initialized), false, "initial owner block");
  events.push_back(initialized);

  tx_extra_vm_call original_call = call_for(3, vm_test::stored_owner_lookup_args());
  original_call.signers.push_back(signer_for(2));
  original_call.signatures.emplace_back();
  transaction original;
  CHECK_AND_ASSERT_MES(construct_call_tx(chain, original_call, original, 2), false, "original signed call");
  events.push_back(original);
  DO_CALLBACK(events, "check_original_call");

  transaction next_owner;
  CHECK_AND_ASSERT_MES(construct_call_tx(chain, call_for(2, vm_test::stored_owner_args(signer_for(3))), next_owner, 3), false, "next owner");
  push_kept_txs(events, {next_owner});
  block changed;
  CHECK_AND_ASSERT_MES(construct_next_block(chain, initialized, chain.miner, fork_version, {next_owner}, changed), false, "new owner block");
  events.push_back(changed);
  DO_CALLBACK(events, "check_changed_call");
  return true;
}

bool gen_contracts_stored_signer_changes::check_original_call(cryptonote::core& c, size_t,
    const std::vector<test_event_entry>& events)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_contracts_stored_signer_changes::check_original_call");
  const transaction* original = find_contract_tx(events, 3);
  CHECK_TEST_CONDITION(original != nullptr);
  contract_effects effects{};
  uint64_t fee = 0;
  CHECK_TEST_CONDITION(get_tx_fee(*original, fee));
  const contract_call_verdict verdict = c.get_blockchain_storage().dry_run_contract_tx(*original, fee, effects);
  CHECK_AND_ASSERT_MES(verdict == contract_call_verdict::executed, false,
      "[" << perr_context << "] original call dry-run is " << contract_call_verdict_name(verdict));
  CHECK_TEST_CONDITION(c.pool_has_tx(get_transaction_hash(*original)));
  return true;
}

bool gen_contracts_stored_signer_changes::check_changed_call(cryptonote::core& c, size_t,
    const std::vector<test_event_entry>& events)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_contracts_stored_signer_changes::check_changed_call");
  const transaction* original = find_contract_tx(events, 3);
  CHECK_TEST_CONDITION(original != nullptr);
  const crypto::hash txid = get_transaction_hash(*original);
  CHECK_TEST_CONDITION(c.get_blockchain_storage().flush_txes_from_pool({txid}));
  tx_verification_context tvc{};
  c.handle_incoming_tx(t_serializable_object_to_blob(*original), tvc, relay_method::fluff, false);
  CHECK_TEST_CONDITION(tvc.m_verifivation_failed && tvc.m_contract_call_rejected && tvc.m_no_drop_offense);
  CHECK_TEST_CONDITION(!c.pool_has_tx(txid));
  return true;
}

gen_contracts_external_addsig::gen_contracts_external_addsig()
{
  REGISTER_CALLBACK_METHOD(gen_contracts_external_addsig, check_mined_replay);
}

bool gen_contracts_external_addsig::generate(std::vector<test_event_entry>& events) const
{
  ring_chain chain;
  CHECK_AND_ASSERT_MES(generate_ring_chain(events, chain), false, "ring chain");
  transaction deployment;
  CHECK_AND_ASSERT_MES(construct_call_tx(chain, deploy_call(), deployment, 0), false, "deployment");
  push_kept_txs(events, {deployment});
  block deployed;
  CHECK_AND_ASSERT_MES(construct_next_block(chain, chain.tip, chain.miner, fork_version, {deployment}, deployed), false, "deployment block");
  events.push_back(deployed);

  const keypair external = keypair::generate(hw::get_device("default"));
  const keypair wrong = keypair::generate(hw::get_device("default"));
  tx_extra_vm_call call = invoke_call(vm_test::m_addsig, key_bytes(external.pub));
  call.signers.push_back(external.pub);
  call.signatures.emplace_back();
  transaction unsigned_tx;
  CHECK_AND_ASSERT_MES(construct_call_tx(chain, call, unsigned_tx, 1, {external.pub}), false, "unsigned external call");
  CHECK_AND_ASSERT_MES(!contract_call_signed(unsigned_tx), false, "external slot is pending");
  tools::wallet2::pending_tx ptx{};
  ptx.tx = unsigned_tx;
  const std::string pending_hex = tools::wallet2::pending_tx_to_hex(ptx);
  tools::wallet2::pending_tx_decode_error decode_error{};
  CHECK_AND_ASSERT_MES(tools::wallet2::pending_tx_from_hex(pending_hex, ptx, false, decode_error), false, "pending metadata");
  const std::string digest_hex = tools::wallet2::contract_signature_hash_hex(ptx.tx, FAKECHAIN);
  crypto::hash digest{};
  CHECK_AND_ASSERT_MES(epee::string_tools::hex_to_pod(digest_hex, digest), false, "external digest");
  const crypto::hash unsigned_hash = get_transaction_hash(unsigned_tx);
  const size_t unsigned_size = tx_to_blob(unsigned_tx).size();
  crypto::hash ring_hash{};
  CHECK_AND_ASSERT_MES(get_contract_ring_hash(unsigned_tx, ring_hash, fork_version), false, "external ring hash");
  crypto::signature wrong_signature{};
  crypto::generate_signature(digest, wrong.pub, wrong.sec, wrong_signature);
  bool wrong_rejected = false;
  try
  {
    tools::wallet2::attach_contract_signatures_hex(ptx, FAKECHAIN, {{epee::string_tools::pod_to_hex(external.pub), epee::string_tools::pod_to_hex(wrong_signature)}}, true);
  }
  catch (const tools::error::wallet_internal_error&)
  {
    wrong_rejected = true;
  }
  CHECK_AND_ASSERT_MES(wrong_rejected && ptx.tx.extra == unsigned_tx.extra, false, "wrong external signature rejected");
  crypto::signature signature{};
  crypto::generate_signature(digest, external.pub, external.sec, signature);
  tools::wallet2::attach_contract_signatures_hex(ptx, FAKECHAIN, {{epee::string_tools::pod_to_hex(external.pub), epee::string_tools::pod_to_hex(signature)}}, true);
  CHECK_AND_ASSERT_MES(contract_call_signed(ptx.tx) && get_transaction_hash(ptx.tx) != unsigned_hash
      && tx_to_blob(ptx.tx).size() == unsigned_size, false, "external signature attached");
  crypto::hash after{};
  CHECK_AND_ASSERT_MES(contract_call_signature_hash(ptx.tx, FAKECHAIN, after) && after == digest, false, "digest unchanged");
  CHECK_AND_ASSERT_MES(get_contract_ring_hash(ptx.tx, after, fork_version) && after == ring_hash, false, "ring hash unchanged");
  DO_CALLBACK(events, "mark_invalid_tx");
  events.push_back(std::vector<transaction>{unsigned_tx});
  DO_CALLBACK(events, "check_pool_rejects_signature");
  events.push_back(std::vector<transaction>{ptx.tx});
  block accepted;
  CHECK_AND_ASSERT_MES(construct_next_block(chain, deployed, chain.miner, fork_version, {ptx.tx}, accepted), false, "external signature block");
  events.push_back(accepted);
  DO_CALLBACK(events, "check_mined_replay");
  DO_CALLBACK(events, "check_vm_mirror");
  return true;
}

bool gen_contracts_external_addsig::check_mined_replay(cryptonote::core& c, size_t ev_index,
    const std::vector<test_event_entry>& events)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_contracts_external_addsig::check_mined_replay");
  CHECK_TEST_CONDITION(ev_index >= 2);
  const auto* transactions = boost::get<std::vector<transaction>>(&events[ev_index - 2]);
  CHECK_TEST_CONDITION(transactions && transactions->size() == 1);
  const transaction& signed_tx = transactions->front();
  CHECK_TEST_CONDITION(c.get_blockchain_storage().have_tx(get_transaction_hash(signed_tx)));
  tx_verification_context tvc{};
  const bool accepted = c.handle_incoming_tx(t_serializable_object_to_blob(signed_tx), tvc,
      relay_method::local, false, true);
  CHECK_TEST_CONDITION(!accepted && tvc.m_verifivation_failed && tvc.m_double_spend && tvc.m_no_drop_offense);
  return true;
}

bool gen_contracts_addsig_validation::generate(std::vector<test_event_entry>& events) const
{
  ring_chain chain;
  CHECK_AND_ASSERT_MES(generate_ring_chain(events, chain), false, "ring chain");

  transaction deployment;
  CHECK_AND_ASSERT_MES(construct_call_tx(chain, deploy_call(), deployment, 0), false, "deployment");
  push_kept_txs(events, {deployment});
  block deployed;
  CHECK_AND_ASSERT_MES(construct_next_block(chain, chain.tip, chain.miner, fork_version, {deployment}, deployed), false, "deployment block");
  events.push_back(deployed);

  const auto public_key_for = [&](size_t index) {
    crypto::public_key public_key{};
    crypto::secret_key secret_key{};
    CHECK_AND_ASSERT_MES(derive_contract_key(chain.ring_miners[index].get_keys(), contract_id(), crypto::null_hash, public_key, secret_key), crypto::null_pkey, "derived key");
    return public_key;
  };
  const auto reject_from_pool = [&](const transaction& tx) {
    DO_CALLBACK(events, "mark_invalid_tx");
    events.push_back(std::vector<transaction>{tx});
    DO_CALLBACK(events, "check_pool_rejects_signer_mismatch");
  };

  tx_extra_vm_call missing_call = invoke_call(vm_test::m_addsig, key_bytes(public_key_for(1)));
  transaction missing;
  CHECK_AND_ASSERT_MES(construct_call_tx(chain, missing_call, missing, 1), false, "missing signature transaction");
  reject_from_pool(missing);

  tx_extra_vm_call wrong_call = invoke_call(vm_test::m_addsig, key_bytes(public_key_for(1)));
  wrong_call.signers.push_back(public_key_for(2));
  wrong_call.signatures.emplace_back();
  transaction wrong;
  CHECK_AND_ASSERT_MES(construct_call_tx(chain, wrong_call, wrong, 2), false, "wrong signer transaction");
  reject_from_pool(wrong);

  tx_extra_vm_call extra_call = invoke_call(vm_test::m_addsig, key_bytes(public_key_for(3)));
  extra_call.signers.push_back(public_key_for(3));
  crypto::hash alternate_key_id = crypto::cn_fast_hash("alternate", 9);
  crypto::public_key alternate_public_key{};
  crypto::secret_key alternate_secret_key{};
  CHECK_AND_ASSERT_MES(derive_contract_key(chain.ring_miners[3].get_keys(), contract_id(), alternate_key_id, alternate_public_key, alternate_secret_key), false, "alternate signer");
  extra_call.signers.push_back(alternate_public_key);
  extra_call.signer_key_ids = {crypto::null_hash, alternate_key_id};
  extra_call.signatures.resize(2);
  transaction extra;
  CHECK_AND_ASSERT_MES(construct_call_tx(chain, extra_call, extra, 3), false, "extra signer transaction");
  reject_from_pool(extra);

  transaction block_missing;
  CHECK_AND_ASSERT_MES(construct_call_tx(chain, invoke_call(vm_test::m_addsig, key_bytes(public_key_for(4))), block_missing, 4), false, "block missing signature transaction");
  push_kept_txs(events, {block_missing});
  block rejected;
  CHECK_AND_ASSERT_MES(construct_next_block(chain, deployed, chain.miner, fork_version, {block_missing}, rejected), false, "rejected block");
  DO_CALLBACK(events, "mark_invalid_block");
  events.push_back(rejected);
  DO_CALLBACK(events, "check_height_unchanged");

  tx_extra_vm_call valid_call = invoke_call(vm_test::m_addsig, key_bytes(public_key_for(5)));
  valid_call.signers.push_back(public_key_for(5));
  valid_call.signatures.emplace_back();
  transaction valid;
  CHECK_AND_ASSERT_MES(construct_call_tx(chain, valid_call, valid, 5), false, "signed transaction");
  transaction reordered = valid;
  tx_extra_vm_call reordered_call{};
  CHECK_AND_ASSERT_MES(get_tx_vm_call(reordered.extra, reordered_call) == 1, false, "signed call for reordering");
  std::vector<uint8_t> reordered_extra;
  CHECK_AND_ASSERT_MES(add_vm_call_to_tx_extra(reordered_extra, reordered_call)
      && remove_field_from_tx_extra(reordered.extra, typeid(tx_extra_vm_call)), false, "reordered call extra");
  reordered_extra.insert(reordered_extra.end(), reordered.extra.begin(), reordered.extra.end());
  reordered.extra = std::move(reordered_extra);
  reordered.invalidate_hashes();
  DO_CALLBACK(events, "mark_invalid_tx");
  events.push_back(std::vector<transaction>{reordered});
  DO_CALLBACK(events, "check_pool_rejects_malformed_extra");

  transaction tampered = valid;
  tx_extra_vm_call tampered_call{};
  CHECK_AND_ASSERT_MES(get_tx_vm_call(tampered.extra, tampered_call) == 1, false, "signed call");
  tampered_call.signatures.front().value.c.data[0] ^= 1;
  CHECK_AND_ASSERT_MES(remove_field_from_tx_extra(tampered.extra, typeid(tx_extra_vm_call))
      && add_vm_call_to_tx_extra(tampered.extra, tampered_call)
      && sort_tx_extra(tampered.extra, tampered.extra, false, true, true), false, "tampered signature");
  tampered.invalidate_hashes();
  DO_CALLBACK(events, "mark_invalid_tx");
  events.push_back(std::vector<transaction>{tampered});
  DO_CALLBACK(events, "check_pool_rejects_signature");
  events.push_back(std::vector<transaction>{valid});
  block accepted;
  CHECK_AND_ASSERT_MES(construct_next_block(chain, deployed, chain.miner, fork_version, {valid}, accepted), false, "signed block");
  events.push_back(accepted);
  DO_CALLBACK(events, "check_vm_mirror");
  return true;
}
