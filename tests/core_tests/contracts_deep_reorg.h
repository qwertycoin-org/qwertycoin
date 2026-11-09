#pragma once
#include "contracts.h"

struct gen_contracts_deep_reorg_replay : public gen_contracts_base
{
  static const uint64_t reorg_depth = QWC_VM_UNDO_RETENTION_BLOCKS + 1;

  gen_contracts_deep_reorg_replay()
  {
    REGISTER_CALLBACK_METHOD(gen_contracts_deep_reorg_replay, check_old_undo_expired);
    REGISTER_CALLBACK_METHOD(gen_contracts_deep_reorg_replay, check_replayed_alt_chain);
  }

  bool generate(std::vector<test_event_entry>& events) const;
  bool push_empty_blocks(std::vector<test_event_entry>& events, ring_chain& chain, cryptonote::block& tip,
      const cryptonote::account_base& miner, uint64_t count) const;

  bool check_old_undo_expired(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events);
  bool check_replayed_alt_chain(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events);
};
template<> struct get_test_options<gen_contracts_deep_reorg_replay>: public get_height_window_test_options<HF_VERSION_QWC_CONTRACTS> {};
