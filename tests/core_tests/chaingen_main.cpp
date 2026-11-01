// Copyright (c) 2014-2022, The Monero Project
// 
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
// 
// 1. Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the following disclaimer.
// 
// 2. Redistributions in binary form must reproduce the above copyright notice, this list
//    of conditions and the following disclaimer in the documentation and/or other
//    materials provided with the distribution.
// 
// 3. Neither the name of the copyright holder nor the names of its contributors may be
//    used to endorse or promote products derived from this software without specific
//    prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// 
// Parts of this file are originally copyright (c) 2012-2013 The Cryptonote developers

#include "chaingen.h"
#include "chaingen_tests_list.h"
#include "common/util.h"
#include "common/command_line.h"
#include "tx_pool.h"
#include "transaction_tests.h"
#include "contracts_template_cache.h"

#include <boost/regex.hpp>

namespace po = boost::program_options;

namespace
{
  const command_line::arg_descriptor<std::string> arg_test_data_path              = {"test_data_path", "", ""};
  const command_line::arg_descriptor<bool>        arg_generate_test_data          = {"generate_test_data", ""};
  const command_line::arg_descriptor<bool>        arg_play_test_data              = {"play_test_data", ""};
  const command_line::arg_descriptor<bool>        arg_generate_and_play_test_data = {"generate_and_play_test_data", ""};
  const command_line::arg_descriptor<bool>        arg_test_transactions           = {"test_transactions", ""};
  const command_line::arg_descriptor<std::string> arg_filter                      = { "filter", "Regular expression filter for which tests to run" };
  const command_line::arg_descriptor<bool>        arg_list_tests                  = {"list_tests", ""};
}

int main(int argc, char* argv[])
{
  TRY_ENTRY();
  tools::on_startup();
  epee::string_tools::set_module_name_and_folder(argv[0]);

  //set up logging options
  mlog_configure(mlog_get_default_log_path("core_tests.log"), true);
  mlog_set_log_level(2);
  
  po::options_description desc_options("Allowed options");
  command_line::add_arg(desc_options, command_line::arg_help);
  command_line::add_arg(desc_options, arg_test_data_path);
  command_line::add_arg(desc_options, arg_generate_test_data);
  command_line::add_arg(desc_options, arg_play_test_data);
  command_line::add_arg(desc_options, arg_generate_and_play_test_data);
  command_line::add_arg(desc_options, arg_test_transactions);
  command_line::add_arg(desc_options, arg_filter);
  command_line::add_arg(desc_options, arg_list_tests);

  po::variables_map vm;
  bool r = command_line::handle_error_helper(desc_options, [&]()
  {
    po::store(po::parse_command_line(argc, argv, desc_options), vm);
    po::notify(vm);
    return true;
  });
  if (!r)
    return 1;

  if (command_line::get_arg(vm, command_line::arg_help))
  {
    std::cout << desc_options << std::endl;
    return 0;
  }

  const std::string filter = tools::glob_to_regex(command_line::get_arg(vm, arg_filter));
  boost::smatch match;

  size_t tests_count = 0;
  std::vector<std::string> failed_tests;
  std::string tests_folder = command_line::get_arg(vm, arg_test_data_path);
  bool list_tests = false;
  if (command_line::get_arg(vm, arg_generate_test_data))
  {
    GENERATE("chain001.dat", gen_simple_chain_001);
  }
  else if (command_line::get_arg(vm, arg_play_test_data))
  {
    PLAY("chain001.dat", gen_simple_chain_001);
  }
  else if (command_line::get_arg(vm, arg_generate_and_play_test_data) || (list_tests = command_line::get_arg(vm, arg_list_tests)))
  {
    GENERATE_AND_PLAY(gen_simple_chain_001);
    GENERATE_AND_PLAY(gen_simple_chain_split_1);
    GENERATE_AND_PLAY(one_block);
    GENERATE_AND_PLAY(gen_chain_switch_1);
    GENERATE_AND_PLAY(gen_ring_signature_1);
    GENERATE_AND_PLAY(gen_ring_signature_2);
    //GENERATE_AND_PLAY(gen_ring_signature_big); // Takes up to XXX hours (if CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW == 10)

    // Block verification tests
    GENERATE_AND_PLAY(gen_block_big_major_version);
    GENERATE_AND_PLAY(gen_block_big_minor_version);
    GENERATE_AND_PLAY(gen_block_ts_not_checked);
    GENERATE_AND_PLAY(gen_block_ts_in_past);
    GENERATE_AND_PLAY(gen_block_ts_in_future);
    GENERATE_AND_PLAY(gen_block_invalid_prev_id);
    GENERATE_AND_PLAY(gen_block_invalid_nonce);
    GENERATE_AND_PLAY(gen_block_no_miner_tx);
    GENERATE_AND_PLAY(gen_block_unlock_time_is_low);
    GENERATE_AND_PLAY(gen_block_unlock_time_is_high);
    GENERATE_AND_PLAY(gen_block_unlock_time_is_timestamp_in_past);
    GENERATE_AND_PLAY(gen_block_unlock_time_is_timestamp_in_future);
    GENERATE_AND_PLAY(gen_block_height_is_low);
    GENERATE_AND_PLAY(gen_block_height_is_high);
    GENERATE_AND_PLAY(gen_block_miner_tx_has_2_tx_gen_in);
    GENERATE_AND_PLAY(gen_block_miner_tx_has_2_in);
    GENERATE_AND_PLAY(gen_block_miner_tx_with_txin_to_key);
    GENERATE_AND_PLAY(gen_block_miner_tx_out_is_small);
    GENERATE_AND_PLAY(gen_block_miner_tx_out_is_big);
    GENERATE_AND_PLAY(gen_block_miner_tx_has_no_out);
    GENERATE_AND_PLAY(gen_block_miner_tx_has_out_to_alice);
    GENERATE_AND_PLAY(gen_block_miner_tx_out_has_no_view_tag_before_hf_view_tags);
    GENERATE_AND_PLAY(gen_block_miner_tx_out_has_no_view_tag_from_hf_view_tags);
    GENERATE_AND_PLAY(gen_block_miner_tx_out_has_view_tag_before_hf_view_tags);
    GENERATE_AND_PLAY(gen_block_miner_tx_out_has_view_tag_from_hf_view_tags);
    GENERATE_AND_PLAY(gen_block_has_invalid_tx);
    GENERATE_AND_PLAY(gen_block_is_too_big);
    GENERATE_AND_PLAY(gen_block_invalid_binary_format); // Takes up to 3 hours, if CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW == 500, up to 30 minutes, if CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW == 10
    GENERATE_AND_PLAY(gen_block_late_v1_coinbase_tx);

    // Transaction verification tests
    GENERATE_AND_PLAY(gen_tx_big_version);
    GENERATE_AND_PLAY(gen_tx_unlock_time);
    GENERATE_AND_PLAY(gen_tx_input_is_not_txin_to_key);
    GENERATE_AND_PLAY(gen_tx_no_inputs_no_outputs);
    GENERATE_AND_PLAY(gen_tx_no_inputs_has_outputs);
    GENERATE_AND_PLAY(gen_tx_has_inputs_no_outputs);
    GENERATE_AND_PLAY(gen_tx_invalid_input_amount);
    GENERATE_AND_PLAY(gen_tx_input_wo_key_offsets);
    GENERATE_AND_PLAY(gen_tx_sender_key_offest_not_exist);
    GENERATE_AND_PLAY(gen_tx_key_offest_points_to_foreign_key);
    GENERATE_AND_PLAY(gen_tx_mixed_key_offest_not_exist);
    GENERATE_AND_PLAY(gen_tx_key_image_not_derive_from_tx_key);
    GENERATE_AND_PLAY(gen_tx_key_image_is_invalid);
    GENERATE_AND_PLAY(gen_tx_check_input_unlock_time);
    GENERATE_AND_PLAY(gen_tx_txout_to_key_has_invalid_key);
    GENERATE_AND_PLAY(gen_tx_output_with_zero_amount);
    GENERATE_AND_PLAY(gen_tx_output_is_not_txout_to_key);
    GENERATE_AND_PLAY(gen_tx_signatures_are_invalid);

    // Mempool
    GENERATE_AND_PLAY(txpool_spend_key_public);
    GENERATE_AND_PLAY(txpool_spend_key_all);
    GENERATE_AND_PLAY(txpool_double_spend_norelay);
    GENERATE_AND_PLAY(txpool_double_spend_local);
    GENERATE_AND_PLAY(txpool_double_spend_keyimage);
    GENERATE_AND_PLAY(txpool_stem_loop);

    // Double spend
    GENERATE_AND_PLAY(gen_double_spend_in_tx<false>);
    GENERATE_AND_PLAY(gen_double_spend_in_tx<true>);
    GENERATE_AND_PLAY(gen_double_spend_in_the_same_block<false>);
    GENERATE_AND_PLAY(gen_double_spend_in_the_same_block<true>);
    GENERATE_AND_PLAY(gen_double_spend_in_different_blocks<false>);
    GENERATE_AND_PLAY(gen_double_spend_in_different_blocks<true>);
    GENERATE_AND_PLAY(gen_double_spend_in_different_chains);
    GENERATE_AND_PLAY(gen_double_spend_in_alt_chain_in_the_same_block<false>);
    GENERATE_AND_PLAY(gen_double_spend_in_alt_chain_in_the_same_block<true>);
    GENERATE_AND_PLAY(gen_double_spend_in_alt_chain_in_different_blocks<false>);
    GENERATE_AND_PLAY(gen_double_spend_in_alt_chain_in_different_blocks<true>);

    GENERATE_AND_PLAY(gen_uint_overflow_1);
    GENERATE_AND_PLAY(gen_uint_overflow_2);

    GENERATE_AND_PLAY(gen_block_reward);

    GENERATE_AND_PLAY(gen_v2_tx_mixable_0_mixin);
    GENERATE_AND_PLAY(gen_v2_tx_mixable_low_mixin);
//    GENERATE_AND_PLAY(gen_v2_tx_unmixable_only);
//    GENERATE_AND_PLAY(gen_v2_tx_unmixable_one);
//    GENERATE_AND_PLAY(gen_v2_tx_unmixable_two);
    GENERATE_AND_PLAY(gen_v2_tx_dust);

    GENERATE_AND_PLAY(gen_rct_tx_valid_from_pre_rct);
    GENERATE_AND_PLAY(gen_rct_tx_valid_from_rct);
    GENERATE_AND_PLAY(gen_rct_tx_valid_from_mixed);
    GENERATE_AND_PLAY(gen_rct_tx_pre_rct_bad_real_dest);
    GENERATE_AND_PLAY(gen_rct_tx_pre_rct_bad_real_mask);
    GENERATE_AND_PLAY(gen_rct_tx_pre_rct_bad_fake_dest);
    GENERATE_AND_PLAY(gen_rct_tx_pre_rct_bad_fake_mask);
    GENERATE_AND_PLAY(gen_rct_tx_rct_bad_real_dest);
    GENERATE_AND_PLAY(gen_rct_tx_rct_bad_real_mask);
    GENERATE_AND_PLAY(gen_rct_tx_rct_bad_fake_dest);
    GENERATE_AND_PLAY(gen_rct_tx_rct_bad_fake_mask);
    GENERATE_AND_PLAY(gen_rct_tx_rct_spend_with_zero_commit);
    GENERATE_AND_PLAY(gen_rct_tx_pre_rct_zero_vin_amount);
    GENERATE_AND_PLAY(gen_rct_tx_rct_non_zero_vin_amount);
    GENERATE_AND_PLAY(gen_rct_tx_non_zero_vout_amount);
    GENERATE_AND_PLAY(gen_rct_tx_pre_rct_duplicate_key_image);
    GENERATE_AND_PLAY(gen_rct_tx_rct_duplicate_key_image);
    GENERATE_AND_PLAY(gen_rct_tx_pre_rct_wrong_key_image);
    GENERATE_AND_PLAY(gen_rct_tx_rct_wrong_key_image);
    GENERATE_AND_PLAY(gen_rct_tx_pre_rct_wrong_fee);
    GENERATE_AND_PLAY(gen_rct_tx_rct_wrong_fee);
    GENERATE_AND_PLAY(gen_rct_tx_pre_rct_remove_vin);
    GENERATE_AND_PLAY(gen_rct_tx_rct_remove_vin);
    GENERATE_AND_PLAY(gen_rct_tx_pre_rct_add_vout);
    GENERATE_AND_PLAY(gen_rct_tx_rct_add_vout);
    GENERATE_AND_PLAY(gen_rct_tx_pre_rct_increase_vin_and_fee);
    GENERATE_AND_PLAY(gen_rct_tx_pre_rct_altered_extra);
    GENERATE_AND_PLAY(gen_rct_tx_rct_altered_extra);
    GENERATE_AND_PLAY(gen_rct_tx_pre_rct_has_no_view_tag_before_hf_view_tags);
    // TODO: base test needs to be restructured to handle pre rct outputs after HF v12
    // GENERATE_AND_PLAY(gen_rct_tx_pre_rct_has_no_view_tag_from_hf_view_tags);
    GENERATE_AND_PLAY(gen_rct_tx_pre_rct_has_view_tag_before_hf_view_tags);
    // GENERATE_AND_PLAY(gen_rct_tx_pre_rct_has_view_tag_from_hf_view_tags);
    GENERATE_AND_PLAY(gen_rct_tx_rct_has_no_view_tag_before_hf_view_tags);
    GENERATE_AND_PLAY(gen_rct_tx_rct_has_no_view_tag_from_hf_view_tags);
    GENERATE_AND_PLAY(gen_rct_tx_rct_has_view_tag_before_hf_view_tags);
    GENERATE_AND_PLAY(gen_rct_tx_rct_has_view_tag_from_hf_view_tags);
    GENERATE_AND_PLAY(gen_rct_tx_uses_output_too_early);

    GENERATE_AND_PLAY(gen_multisig_tx_valid_22_1_2);
    GENERATE_AND_PLAY(gen_multisig_tx_valid_22_1_2_many_inputs);
    GENERATE_AND_PLAY(gen_multisig_tx_valid_22_2_1);
    GENERATE_AND_PLAY(gen_multisig_tx_valid_33_1_23);
    GENERATE_AND_PLAY(gen_multisig_tx_valid_33_3_21);
    GENERATE_AND_PLAY(gen_multisig_tx_valid_23_1_2);
    GENERATE_AND_PLAY(gen_multisig_tx_valid_23_1_3);
    GENERATE_AND_PLAY(gen_multisig_tx_valid_23_2_1);
    GENERATE_AND_PLAY(gen_multisig_tx_valid_23_2_3);
    GENERATE_AND_PLAY(gen_multisig_tx_valid_45_1_234);
    GENERATE_AND_PLAY(gen_multisig_tx_valid_45_4_135_many_inputs);
    GENERATE_AND_PLAY(gen_multisig_tx_valid_89_3_1245789);
    GENERATE_AND_PLAY(gen_multisig_tx_invalid_23_1__no_threshold);
    GENERATE_AND_PLAY(gen_multisig_tx_invalid_45_5_23_no_threshold);
    GENERATE_AND_PLAY(gen_multisig_tx_invalid_22_1__no_threshold);
    GENERATE_AND_PLAY(gen_multisig_tx_invalid_33_1__no_threshold);
    GENERATE_AND_PLAY(gen_multisig_tx_invalid_33_1_2_no_threshold);
    GENERATE_AND_PLAY(gen_multisig_tx_invalid_33_1_3_no_threshold);
    GENERATE_AND_PLAY(gen_multisig_tx_valid_24_1_2);
    GENERATE_AND_PLAY(gen_multisig_tx_valid_24_1_2_many_inputs);
    GENERATE_AND_PLAY(gen_multisig_tx_valid_25_1_2);
    GENERATE_AND_PLAY(gen_multisig_tx_valid_25_1_2_many_inputs);
    GENERATE_AND_PLAY(gen_multisig_tx_valid_48_1_234);
    GENERATE_AND_PLAY(gen_multisig_tx_valid_48_1_234_many_inputs);
    GENERATE_AND_PLAY(gen_multisig_tx_invalid_24_1_no_signers);
    GENERATE_AND_PLAY(gen_multisig_tx_invalid_25_1_no_signers);
    GENERATE_AND_PLAY(gen_multisig_tx_invalid_48_1_no_signers);
    GENERATE_AND_PLAY(gen_multisig_tx_invalid_48_1_23_no_threshold);

    GENERATE_AND_PLAY(gen_bp_tx_valid_1_before_12);
    GENERATE_AND_PLAY(gen_bp_tx_invalid_1_from_12);
    GENERATE_AND_PLAY(gen_bp_tx_invalid_1_1);
    GENERATE_AND_PLAY(gen_bp_tx_valid_2);
    GENERATE_AND_PLAY(gen_bp_tx_valid_3);
    GENERATE_AND_PLAY(gen_bp_tx_valid_16);
    GENERATE_AND_PLAY(gen_bp_tx_invalid_4_2_1);
    GENERATE_AND_PLAY(gen_bp_tx_invalid_16_16);
    GENERATE_AND_PLAY(gen_bp_txs_valid_2_and_2);
    GENERATE_AND_PLAY(gen_bp_txs_invalid_2_and_8_2_and_16_16_1);
    GENERATE_AND_PLAY(gen_bp_txs_valid_2_and_3_and_2_and_4);
    GENERATE_AND_PLAY(gen_bp_tx_invalid_not_enough_proofs);
    GENERATE_AND_PLAY(gen_bp_tx_invalid_empty_proofs);
    GENERATE_AND_PLAY(gen_bp_tx_invalid_too_many_proofs);
    GENERATE_AND_PLAY(gen_bp_tx_invalid_wrong_amount);
    GENERATE_AND_PLAY(gen_bp_tx_invalid_borromean_type);
    GENERATE_AND_PLAY(gen_bp_tx_invalid_bulletproof2_type);
    GENERATE_AND_PLAY(gen_bp_tx_invalid_clsag_type);

    GENERATE_AND_PLAY(gen_bpp_tx_invalid_before_fork);
    GENERATE_AND_PLAY(gen_bpp_tx_valid_at_fork);
    GENERATE_AND_PLAY(gen_bpp_tx_invalid_1_1);
    GENERATE_AND_PLAY(gen_bpp_tx_valid_2);
    GENERATE_AND_PLAY(gen_bpp_tx_valid_3);
    GENERATE_AND_PLAY(gen_height_window_absent_ok);
    GENERATE_AND_PLAY(gen_height_window_in_range_ok);
    GENERATE_AND_PLAY(gen_height_window_far_until_unlock_unchanged);
    GENERATE_AND_PLAY(gen_height_window_until_boundary_ok);
    GENERATE_AND_PLAY(gen_height_window_from_boundary_ok);
    GENERATE_AND_PLAY(gen_height_window_until_plus_one_rejected_in_block);
    GENERATE_AND_PLAY(gen_height_window_expired_rejected_by_pool);
    GENERATE_AND_PLAY(gen_height_window_from_after_block_rejected_in_block);
    GENERATE_AND_PLAY(gen_height_window_pool_lead_boundary);
    GENERATE_AND_PLAY(gen_height_window_pool_too_early);
    GENERATE_AND_PLAY(gen_height_window_before_fork_rejected);
    GENERATE_AND_PLAY(gen_height_window_before_fork_block_rejected);
    GENERATE_AND_PLAY(gen_height_window_duplicate_rejected);
    GENERATE_AND_PLAY(gen_height_window_timestamp_unlock_rejected);
    GENERATE_AND_PLAY(gen_height_window_pool_drops_on_expiry);
    GENERATE_AND_PLAY(gen_height_window_reorg);
    GENERATE_AND_PLAY(gen_height_window_reorg_after_expiry);
    GENERATE_AND_PLAY(gen_height_window_coinbase_rejected);
    GENERATE_AND_PLAY(gen_contracts_deploy_and_invoke);
    GENERATE_AND_PLAY(gen_contracts_zero_output_call);
    GENERATE_AND_PLAY(gen_contracts_zero_output_rejected);
    GENERATE_AND_PLAY(gen_contracts_zero_output_before_fork_rejected);
    GENERATE_AND_PLAY(gen_contracts_failing_call_rejected);
    GENERATE_AND_PLAY(gen_contracts_underpaid_rejected);
    GENERATE_AND_PLAY(gen_contracts_tx_gas_cap_rejected);
    GENERATE_AND_PLAY(gen_contracts_root_mismatch_rejected);
    GENERATE_AND_PLAY(gen_contracts_root_missing_rejected);
    GENERATE_AND_PLAY(gen_contracts_root_before_fork_rejected);
    GENERATE_AND_PLAY(gen_contracts_call_before_fork_rejected);
    GENERATE_AND_PLAY(gen_contracts_reorg_undo);
    GENERATE_AND_PLAY(gen_contracts_template_drops_failing_call);
    GENERATE_AND_PLAY(gen_contracts_cached_template);
    GENERATE_AND_PLAY(gen_contracts_failing_alt_chain_keeps_main);
    GENERATE_AND_PLAY(gen_contracts_failing_reorg_shared_tx);
    GENERATE_AND_PLAY(gen_contracts_pool_rules);
    GENERATE_AND_PLAY(gen_contracts_mint_supply_invariant);
    GENERATE_AND_PLAY(gen_contracts_addsig_validation);
    GENERATE_AND_PLAY(gen_contracts_external_addsig);
    GENERATE_AND_PLAY(gen_contracts_stored_signer_changes);
    GENERATE_AND_PLAY(gen_contracts_forged_input_skips_vm);
    GENERATE_AND_PLAY(gen_contracts_fork_boundary);
    GENERATE_AND_PLAY(gen_contracts_block_rules);
    GENERATE_AND_PLAY(gen_chainlock_carried_lock_accepted);
    GENERATE_AND_PLAY(gen_chainlock_rpc);
    GENERATE_AND_PLAY(gen_chainlock_production_inputs);
    GENERATE_AND_PLAY(gen_chainlock_quorum_before_epoch_start);
    GENERATE_AND_PLAY(gen_chainlock_alt_epoch_boundary);
    GENERATE_AND_PLAY(gen_chainlock_rpc_late_carrier);
    GENERATE_AND_PLAY(gen_chainlock_rpc_old_carrier);
    GENERATE_AND_PLAY(gen_chainlock_rpc_pages);
    GENERATE_AND_PLAY(gen_vm_state_proof_rpc);
    GENERATE_AND_PLAY(gen_vm_state_proof_retention);
    GENERATE_AND_PLAY(gen_vm_state_proof_restore);
    GENERATE_AND_PLAY(gen_chainlock_below_threshold_rejected);
    GENERATE_AND_PLAY(gen_chainlock_wrong_epoch_rejected);
    GENERATE_AND_PLAY(gen_chainlock_non_ancestor_rejected);
    GENERATE_AND_PLAY(gen_chainlock_bad_signature_rejected);
    GENERATE_AND_PLAY(gen_chainlock_not_newer_rejected);
    GENERATE_AND_PLAY(gen_chainlock_fork_blocked);
    GENERATE_AND_PLAY(gen_chainlock_higher_lock_wins);
    GENERATE_AND_PLAY(gen_chainlock_message_before_reorg);
    GENERATE_AND_PLAY(gen_chainlock_message_after_reorg);
    GENERATE_AND_PLAY(gen_chainlock_competing_main_first);
    GENERATE_AND_PLAY(gen_chainlock_competing_rival_first);
    GENERATE_AND_PLAY(gen_chainlock_divergent_main_first);
    GENERATE_AND_PLAY(gen_chainlock_divergent_rival_first);
    GENERATE_AND_PLAY(gen_chainlock_divergent_higher);
    GENERATE_AND_PLAY(gen_chainlock_reorg_above_lock);
    GENERATE_AND_PLAY(gen_chainlock_checkpoint_overrides_lock);
    GENERATE_AND_PLAY(gen_chainlock_during_batch);
    GENERATE_AND_PLAY(gen_chainlock_reorg_below_lock_then_above);
    GENERATE_AND_PLAY(gen_chainlock_age_limit);
    GENERATE_AND_PLAY(gen_chainlock_speculative_switch_verified);
    GENERATE_AND_PLAY(gen_chainlock_speculative_switch_rolled_back);
    GENERATE_AND_PLAY(gen_contracts_deep_reorg_replay);
    GENERATE_AND_PLAY(gen_contracts_hdr_info_reorg);
    GENERATE_AND_PLAY(gen_contracts_hdr_info_retry);
    GENERATE_AND_PLAY(gen_contracts_hdr_info_before_fork_rejected);
    GENERATE_AND_PLAY(gen_bpp_tx_valid_16);
    GENERATE_AND_PLAY(gen_bpp_tx_invalid_4_2_1);
    GENERATE_AND_PLAY(gen_bpp_tx_invalid_16_16);
    GENERATE_AND_PLAY(gen_bpp_txs_valid_2_and_2);
    GENERATE_AND_PLAY(gen_bpp_txs_invalid_2_and_8_2_and_16_16_1);
    GENERATE_AND_PLAY(gen_bpp_txs_valid_2_and_3_and_2_and_4);
    GENERATE_AND_PLAY(gen_bpp_tx_invalid_not_enough_proofs);
    GENERATE_AND_PLAY(gen_bpp_tx_invalid_empty_proofs);
    GENERATE_AND_PLAY(gen_bpp_tx_invalid_too_many_proofs);
    GENERATE_AND_PLAY(gen_bpp_tx_invalid_wrong_amount);
    GENERATE_AND_PLAY(gen_bpp_tx_invalid_clsag_type);

    GENERATE_AND_PLAY(gen_rct2_tx_clsag_malleability);

    GENERATE_AND_PLAY(gen_block_low_coinbase);

    el::Level level = (failed_tests.empty() ? el::Level::Info : el::Level::Error);
    if (!list_tests)
    {
      MLOG(level, "\nREPORT:");
      MLOG(level, "  Test run: " << tests_count);
      MLOG(level, "  Failures: " << failed_tests.size());
    }
    if (!failed_tests.empty())
    {
      MLOG(level, "FAILED TESTS:");
      BOOST_FOREACH(auto test_name, failed_tests)
      {
        MLOG(level, "  " << test_name);
      }
    }
  }
  else if (command_line::get_arg(vm, arg_test_transactions))
  {
    CALL_TEST("TRANSACTIONS TESTS", test_transactions);
  }
  else
  {
    MERROR("Wrong arguments");
    return 2;
  }

  return failed_tests.empty() ? 0 : 1;

  CATCH_ENTRY_L0("main", 1);
}
