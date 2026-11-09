#pragma once
#include "chainlock.h"

CHAINLOCK_CASE(gen_chainlock_speculative_switch_verified)
struct gen_chainlock_speculative_switch_rolled_back : gen_chainlock_base
{
  gen_chainlock_speculative_switch_rolled_back()
  {
    REGISTER_CALLBACK_METHOD(gen_chainlock_speculative_switch_rolled_back, store_wrong_anchor_lock);
  }
  bool generate(std::vector<test_event_entry>& events) const;
  std::vector<expectation> expectations() const override;
  bool store_wrong_anchor_lock(cryptonote::core& c, size_t ev_index,
      const std::vector<test_event_entry>& events);
};
template<> struct get_test_options<gen_chainlock_speculative_switch_rolled_back>: public get_height_window_test_options<HF_VERSION_QWC_CONTRACTS> {};
