#pragma once

#include "contracts.h"

struct gen_contracts_hdr_info_reorg : gen_contracts_base
{
  gen_contracts_hdr_info_reorg();
  bool generate(std::vector<test_event_entry>& events) const;
  bool check_main(cryptonote::core& c, size_t index, const std::vector<test_event_entry>& events);
  bool check_alt(cryptonote::core& c, size_t index, const std::vector<test_event_entry>& events);
};

template<> struct get_test_options<gen_contracts_hdr_info_reorg> : get_height_window_test_options<HF_VERSION_QWC_CONTRACTS> {};

struct gen_contracts_hdr_info_retry : gen_contracts_base
{
  gen_contracts_hdr_info_retry();
  bool generate(std::vector<test_event_entry>& events) const;
  bool inject_failure(cryptonote::core& c, size_t index, const std::vector<test_event_entry>& events);
  bool retry_block(cryptonote::core& c, size_t index, const std::vector<test_event_entry>& events);
  bool m_provider_called = false;
};

template<> struct get_test_options<gen_contracts_hdr_info_retry> : get_height_window_test_options<HF_VERSION_QWC_CONTRACTS> {};

struct gen_contracts_hdr_info_before_fork_rejected : gen_contracts_base
{
  bool generate(std::vector<test_event_entry>& events) const;
};

template<> struct get_test_options<gen_contracts_hdr_info_before_fork_rejected> : get_height_window_test_options<HF_VERSION_QWC_EPOSE> {};
