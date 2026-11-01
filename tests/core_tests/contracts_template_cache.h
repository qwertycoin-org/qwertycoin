#pragma once

#include "contracts.h"

struct gen_contracts_cached_template : gen_contracts_base
{
  gen_contracts_cached_template();
  bool generate(std::vector<test_event_entry>& events) const;
  bool check_cached_template(cryptonote::core& core, size_t event_index, const std::vector<test_event_entry>& events);
};

template<> struct get_test_options<gen_contracts_cached_template> : public get_height_window_test_options<HF_VERSION_QWC_CONTRACTS> {};
