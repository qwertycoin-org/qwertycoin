#pragma once
#include "chainlock.h"

namespace cryptonote { class Blockchain; }

struct gen_chainlock_production_inputs : public gen_chainlock_base
{
  gen_chainlock_production_inputs()
  {
    REGISTER_CALLBACK_METHOD(gen_chainlock_production_inputs, check_inputs);
  }
  bool generate(std::vector<test_event_entry>& events) const;
  std::vector<expectation> expectations() const override { return {}; }
  bool check_inputs(cryptonote::core& c, size_t, const std::vector<test_event_entry>&);
  static bool install_production_inputs(cryptonote::Blockchain& chain, const qwertycoin::epose::chainlock_v1& lock);
  static void mark_test_state_at_tip(cryptonote::Blockchain& chain);
  static bool restore_test_quorum(cryptonote::Blockchain& chain);
};
template<> struct get_test_options<gen_chainlock_production_inputs>: public get_height_window_test_options<HF_VERSION_QWC_CONTRACTS> {};
