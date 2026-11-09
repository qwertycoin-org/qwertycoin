#pragma once
#include "chainlock.h"

struct gen_chainlock_rpc : public gen_chainlock_base
{
  gen_chainlock_rpc()
  {
    REGISTER_CALLBACK_METHOD(gen_chainlock_rpc, check_unlocked);
    REGISTER_CALLBACK_METHOD(gen_chainlock_rpc, check_locked);
    REGISTER_CALLBACK_METHOD(gen_chainlock_rpc, write_vector);
  }
  bool generate(std::vector<test_event_entry>& events) const;
  std::vector<expectation> expectations() const override { return {}; }
  bool check_unlocked(cryptonote::core& c, size_t, const std::vector<test_event_entry>&);
  bool check_locked(cryptonote::core& c, size_t, const std::vector<test_event_entry>&);
  bool write_vector(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events);
};
template<> struct get_test_options<gen_chainlock_rpc>: public get_height_window_test_options<HF_VERSION_QWC_CONTRACTS> {};

struct gen_chainlock_quorum_before_epoch_start : public gen_chainlock_base
{
  gen_chainlock_quorum_before_epoch_start()
  {
    REGISTER_CALLBACK_METHOD(gen_chainlock_quorum_before_epoch_start, check_quorum);
  }
  bool generate(std::vector<test_event_entry>& events) const;
  std::vector<expectation> expectations() const override { return {}; }
  bool check_quorum(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events);
};
template<> struct get_test_options<gen_chainlock_quorum_before_epoch_start>: public get_height_window_test_options<HF_VERSION_QWC_CONTRACTS> {};

struct gen_chainlock_alt_epoch_boundary : public gen_chainlock_base
{
  gen_chainlock_alt_epoch_boundary()
  {
    REGISTER_CALLBACK_METHOD(gen_chainlock_alt_epoch_boundary, check_verdict);
    REGISTER_CALLBACK_METHOD(gen_chainlock_alt_epoch_boundary, check_reorg);
  }
  bool generate(std::vector<test_event_entry>& events) const;
  std::vector<expectation> expectations() const override { return {}; }
  bool check_verdict(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events);
  bool check_reorg(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events);
};
template<> struct get_test_options<gen_chainlock_alt_epoch_boundary>: public get_height_window_test_options<HF_VERSION_QWC_CONTRACTS> {};

struct gen_chainlock_rpc_late_carrier : public gen_chainlock_base
{
  gen_chainlock_rpc_late_carrier()
  {
    REGISTER_CALLBACK_METHOD(gen_chainlock_rpc_late_carrier, check_proof);
  }
  bool generate(std::vector<test_event_entry>& events) const;
  std::vector<expectation> expectations() const override { return {}; }
  bool check_proof(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events);
};
template<> struct get_test_options<gen_chainlock_rpc_late_carrier>: public get_height_window_test_options<HF_VERSION_QWC_CONTRACTS> {};

struct gen_chainlock_rpc_old_carrier : public gen_chainlock_base
{
  gen_chainlock_rpc_old_carrier()
  {
    REGISTER_CALLBACK_METHOD(gen_chainlock_rpc_old_carrier, check_proof);
    REGISTER_CALLBACK_METHOD(gen_chainlock_rpc_old_carrier, check_reorg);
  }
  bool generate(std::vector<test_event_entry>& events) const;
  std::vector<expectation> expectations() const override { return {}; }
  bool check_proof(cryptonote::core& c, size_t, const std::vector<test_event_entry>&);
  bool check_reorg(cryptonote::core& c, size_t, const std::vector<test_event_entry>&);
};
template<> struct get_test_options<gen_chainlock_rpc_old_carrier>: public get_height_window_test_options<HF_VERSION_QWC_CONTRACTS> {};

struct gen_chainlock_rpc_pages : public gen_chainlock_base
{
  gen_chainlock_rpc_pages()
  {
    REGISTER_CALLBACK_METHOD(gen_chainlock_rpc_pages, check_page);
  }
  bool generate(std::vector<test_event_entry>& events) const;
  std::vector<expectation> expectations() const override { return {}; }
  bool check_page(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events);
};
template<> struct get_test_options<gen_chainlock_rpc_pages>: public get_height_window_test_options<HF_VERSION_QWC_CONTRACTS> {};
