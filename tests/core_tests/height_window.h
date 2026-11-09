#pragma once
#include "chaingen.h"
#include "bulletproof_plus.h"

struct gen_height_window_base : public gen_bpp_tx_validation_base
{
  static const uint8_t fork_version = HF_VERSION_QWC_CONTRACTS;
  static const size_t ring_size = 16;
  static const uint64_t mined_height = 1 + ring_size + CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW;

  struct ring_chain
  {
    cryptonote::block tip;
    cryptonote::account_base miner;
    std::vector<cryptonote::account_base> ring_miners;
    std::vector<cryptonote::block> ring_blocks;
    uint64_t spent_amount = 0;
    uint64_t outputs_before_ring = 0;
    test_generator generator;
  };

  gen_height_window_base()
  {
    REGISTER_CALLBACK_METHOD(gen_height_window_base, check_unlock);
    REGISTER_CALLBACK_METHOD(gen_height_window_base, check_block_rejected_with_supplement);
    REGISTER_CALLBACK_METHOD(gen_height_window_base, check_pool_rejects_without_drop_offense);
    REGISTER_CALLBACK_METHOD(gen_height_window_base, check_last_tx_in_pool);
    REGISTER_CALLBACK_METHOD(gen_height_window_base, check_last_tx_not_in_pool);
  }

  bool generate_ring_chain(std::vector<test_event_entry>& events, ring_chain& chain) const;
  cryptonote::tx_source_entry ring_source(const ring_chain& chain, size_t real_block) const;
  bool construct_window_tx(const ring_chain& chain, const std::vector<uint8_t>& extra, cryptonote::transaction& tx, size_t spender = 0, const std::vector<crypto::public_key>& external_contract_signers = {}) const;
  bool construct_window_tx(const ring_chain& chain, uint64_t from, uint64_t until, cryptonote::transaction& tx) const;
  bool construct_next_block(ring_chain& chain, const cryptonote::block& prev, const cryptonote::account_base& miner, uint8_t hf_version,
      const std::vector<cryptonote::transaction>& txs, cryptonote::block& blk, const cryptonote::blobdata& chainlock = cryptonote::blobdata()) const;

  bool resubmit_last_tx(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events, cryptonote::tx_verification_context& tvc) const;
  static bool handle_block_with_supplement(cryptonote::core& c, const cryptonote::block& blk,
      const cryptonote::transaction& tx, bool preverified, cryptonote::block_verification_context& bvc);

  bool generate_case(std::vector<test_event_entry>& events, uint8_t hf_version, const std::vector<uint8_t>& extra,
      bool tx_valid, bool keep_by_block, bool block_valid) const;

  bool generate_case_with_window(std::vector<test_event_entry>& events, uint8_t hf_version,
      uint64_t from, uint64_t until, bool tx_valid, bool keep_by_block, bool block_valid) const;

  bool check_unlock(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events);
  bool check_block_rejected_with_supplement(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events);
  bool check_pool_rejects_without_drop_offense(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events);
  bool check_last_tx_in_pool(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events);
  bool check_last_tx_not_in_pool(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events);
};

template<uint8_t test_version>
struct get_height_window_test_options {
  const std::pair<uint8_t, uint64_t> hard_forks[4] = {std::make_pair(1, 0), std::make_pair(2, 1), std::make_pair(test_version, gen_height_window_base::mined_height), std::make_pair(0, 0)};
  const cryptonote::test_options test_options = { hard_forks, 0 };
};

#define HEIGHT_WINDOW_CASE(NAME, VERSION)                                                        \
  struct NAME : public gen_height_window_base { bool generate(std::vector<test_event_entry>& events) const; }; \
  template<> struct get_test_options<NAME>: public get_height_window_test_options<VERSION> {};

HEIGHT_WINDOW_CASE(gen_height_window_absent_ok, HF_VERSION_QWC_CONTRACTS)
HEIGHT_WINDOW_CASE(gen_height_window_in_range_ok, HF_VERSION_QWC_CONTRACTS)
HEIGHT_WINDOW_CASE(gen_height_window_far_until_unlock_unchanged, HF_VERSION_QWC_CONTRACTS)
HEIGHT_WINDOW_CASE(gen_height_window_until_boundary_ok, HF_VERSION_QWC_CONTRACTS)
HEIGHT_WINDOW_CASE(gen_height_window_from_boundary_ok, HF_VERSION_QWC_CONTRACTS)
HEIGHT_WINDOW_CASE(gen_height_window_until_plus_one_rejected_in_block, HF_VERSION_QWC_CONTRACTS)
HEIGHT_WINDOW_CASE(gen_height_window_expired_rejected_by_pool, HF_VERSION_QWC_CONTRACTS)
HEIGHT_WINDOW_CASE(gen_height_window_from_after_block_rejected_in_block, HF_VERSION_QWC_CONTRACTS)
HEIGHT_WINDOW_CASE(gen_height_window_pool_lead_boundary, HF_VERSION_QWC_CONTRACTS)
HEIGHT_WINDOW_CASE(gen_height_window_pool_too_early, HF_VERSION_QWC_CONTRACTS)
HEIGHT_WINDOW_CASE(gen_height_window_before_fork_rejected, HF_VERSION_QWC_EPOSE)
HEIGHT_WINDOW_CASE(gen_height_window_before_fork_block_rejected, HF_VERSION_QWC_EPOSE)
HEIGHT_WINDOW_CASE(gen_height_window_duplicate_rejected, HF_VERSION_QWC_CONTRACTS)
HEIGHT_WINDOW_CASE(gen_height_window_timestamp_unlock_rejected, HF_VERSION_QWC_CONTRACTS)
HEIGHT_WINDOW_CASE(gen_height_window_pool_drops_on_expiry, HF_VERSION_QWC_CONTRACTS)

struct gen_height_window_reorg : public gen_height_window_base
{
  gen_height_window_reorg()
  {
    REGISTER_CALLBACK_METHOD(gen_height_window_reorg, check_on_b);
    REGISTER_CALLBACK_METHOD(gen_height_window_reorg, check_back_on_a);
  }
  bool generate(std::vector<test_event_entry>& events) const;
  bool check_on_b(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events);
  bool check_back_on_a(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events);
};
template<> struct get_test_options<gen_height_window_reorg>: public get_height_window_test_options<HF_VERSION_QWC_CONTRACTS> {};

struct gen_height_window_reorg_after_expiry : public gen_height_window_base
{
  gen_height_window_reorg_after_expiry()
  {
    REGISTER_CALLBACK_METHOD(gen_height_window_reorg_after_expiry, check_dropped_into_cache);
    REGISTER_CALLBACK_METHOD(gen_height_window_reorg_after_expiry, check_switched_with_cached_tx);
  }
  bool generate(std::vector<test_event_entry>& events) const;
  bool check_dropped_into_cache(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events);
  bool check_switched_with_cached_tx(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events);
};
template<> struct get_test_options<gen_height_window_reorg_after_expiry>: public get_height_window_test_options<HF_VERSION_QWC_CONTRACTS> {};

struct gen_height_window_coinbase_rejected : public gen_bpp_tx_validation_base
{
  bool generate(std::vector<test_event_entry>& events) const;
};
template<> struct get_test_options<gen_height_window_coinbase_rejected> {
  const std::pair<uint8_t, uint64_t> hard_forks[4] = {std::make_pair(1, 0), std::make_pair(2, 1), std::make_pair(HF_VERSION_QWC_CONTRACTS, 2), std::make_pair(0, 0)};
  const cryptonote::test_options test_options = { hard_forks, 0 };
};
