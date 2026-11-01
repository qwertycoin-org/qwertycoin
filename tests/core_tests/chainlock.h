#pragma once
#include <functional>
#include <vector>

#include <boost/optional.hpp>

#include "height_window.h"
#include "epose/chainlock_v1.h"

struct gen_chainlock_base : public gen_height_window_base
{
  static const uint64_t lock_epoch_length = 20;
  static const uint64_t lock_anchor_depth = 5;
  static const size_t signer_count = 10;

  struct signer_key
  {
    crypto::public_key public_key{};
    crypto::secret_key secret_key{};
  };

  enum class branch { main, rival };

  struct expectation
  {
    uint64_t tip_height = 0;
    branch tip_branch = branch::main;
    boost::optional<uint64_t> highest_lock;
    boost::optional<uint64_t> carrier_lock;
    boost::optional<uint64_t> tombstoned;
  };

  struct lock_spec
  {
    uint64_t height = 0;
    crypto::hash block_hash{};
    uint64_t epoch = 0;
    size_t signatures = 0;
    boost::optional<crypto::hash> anchor_hash;
  };

  using blob_maker = std::function<bool(const std::vector<test_event_entry>&, const cryptonote::block&, cryptonote::blobdata&)>;

  gen_chainlock_base()
  {
    REGISTER_CALLBACK_METHOD(gen_chainlock_base, install_quorum);
    REGISTER_CALLBACK_METHOD(gen_chainlock_base, install_checkpoint);
    REGISTER_CALLBACK_METHOD(gen_chainlock_base, check_expectation);
    REGISTER_CALLBACK_METHOD(gen_chainlock_base, check_last_block_invalid);
  }
  virtual ~gen_chainlock_base() = default;

  virtual std::vector<expectation> expectations() const = 0;

  static qwertycoin::epose::epoch_timing_v2 timing();
  static std::vector<signer_key> signers();
  static std::vector<expectation> rejected_expectations();
  static const cryptonote::block* main_block_at(const std::vector<test_event_entry>& events, uint64_t height);

  bool start_chain(std::vector<test_event_entry>& events, ring_chain& chain) const;
  bool extend(std::vector<test_event_entry>& events, ring_chain& chain, const cryptonote::block& prev, const cryptonote::account_base& miner,
      cryptonote::block& blk, const cryptonote::blobdata& lock = cryptonote::blobdata(),
      uint8_t version = fork_version) const;
  bool make_lock(const std::vector<test_event_entry>& events, const lock_spec& spec, qwertycoin::epose::chainlock_v1& lock) const;
  bool quorum_for(const std::vector<test_event_entry>& events, const lock_spec& spec,
      qwertycoin::epose::chainlock_quorum_v1& quorum, qwertycoin::epose::chainlock_context_v1& context) const;
  bool make_lock_blob(const std::vector<test_event_entry>& events, const lock_spec& spec, cryptonote::blobdata& blob) const;
  lock_spec full_lock(const cryptonote::block& locked) const;
  bool generate_rejected_lock(std::vector<test_event_entry>& events, const blob_maker& make_blob) const;

  bool install_quorum(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events);
  bool install_checkpoint(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events);
  bool check_expectation(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events);
  bool check_last_block_invalid(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events);

private:
  size_t m_checked = 0;
};

#define CHAINLOCK_CASE(NAME)                                                                       \
  struct NAME : public gen_chainlock_base                                                          \
  {                                                                                                \
    bool generate(std::vector<test_event_entry>& events) const;                                    \
    std::vector<expectation> expectations() const override;                                        \
  };                                                                                               \
  template<> struct get_test_options<NAME>: public get_height_window_test_options<HF_VERSION_QWC_CONTRACTS> {};

CHAINLOCK_CASE(gen_chainlock_carried_lock_accepted)
CHAINLOCK_CASE(gen_chainlock_below_threshold_rejected)
CHAINLOCK_CASE(gen_chainlock_wrong_epoch_rejected)
CHAINLOCK_CASE(gen_chainlock_non_ancestor_rejected)
CHAINLOCK_CASE(gen_chainlock_bad_signature_rejected)
CHAINLOCK_CASE(gen_chainlock_not_newer_rejected)
CHAINLOCK_CASE(gen_chainlock_fork_blocked)
CHAINLOCK_CASE(gen_chainlock_higher_lock_wins)
CHAINLOCK_CASE(gen_chainlock_reorg_above_lock)
CHAINLOCK_CASE(gen_chainlock_checkpoint_overrides_lock)

struct gen_chainlock_message_order_base : gen_chainlock_base
{
  gen_chainlock_message_order_base()
  {
    REGISTER_CALLBACK_METHOD(gen_chainlock_message_order_base, deliver_lock);
    REGISTER_CALLBACK_METHOD(gen_chainlock_message_order_base, deliver_rival_lock);
    REGISTER_CALLBACK_METHOD(gen_chainlock_message_order_base, check_convergence);
  }
  std::vector<expectation> expectations() const override;
  bool generate_case(std::vector<test_event_entry>& events, bool lock_first, bool competing = false) const;
  bool deliver_lock(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events);
  bool deliver_rival_lock(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events);
  bool check_convergence(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events);
  mutable bool lock_first_ = false;
  mutable bool competing_ = false;
};

struct gen_chainlock_message_before_reorg : gen_chainlock_message_order_base
{
  bool generate(std::vector<test_event_entry>& events) const { return generate_case(events, true); }
};
template<> struct get_test_options<gen_chainlock_message_before_reorg>: public get_height_window_test_options<HF_VERSION_QWC_CONTRACTS> {};

struct gen_chainlock_message_after_reorg : gen_chainlock_message_order_base
{
  bool generate(std::vector<test_event_entry>& events) const { return generate_case(events, false); }
};
template<> struct get_test_options<gen_chainlock_message_after_reorg>: public get_height_window_test_options<HF_VERSION_QWC_CONTRACTS> {};

struct gen_chainlock_competing_base : gen_chainlock_message_order_base
{
  std::vector<expectation> expectations() const override;
};

struct gen_chainlock_competing_main_first : gen_chainlock_competing_base
{
  bool generate(std::vector<test_event_entry>& events) const { return generate_case(events, true, true); }
};
template<> struct get_test_options<gen_chainlock_competing_main_first>: public get_height_window_test_options<HF_VERSION_QWC_CONTRACTS> {};

struct gen_chainlock_competing_rival_first : gen_chainlock_competing_base
{
  bool generate(std::vector<test_event_entry>& events) const { return generate_case(events, false, true); }
};
template<> struct get_test_options<gen_chainlock_competing_rival_first>: public get_height_window_test_options<HF_VERSION_QWC_CONTRACTS> {};

struct gen_chainlock_divergent_order_base : gen_chainlock_base
{
  gen_chainlock_divergent_order_base()
  {
    REGISTER_CALLBACK_METHOD(gen_chainlock_divergent_order_base, deliver_main_lock);
    REGISTER_CALLBACK_METHOD(gen_chainlock_divergent_order_base, deliver_rival_lock);
    REGISTER_CALLBACK_METHOD(gen_chainlock_divergent_order_base, check_convergence);
  }
  bool generate_case(std::vector<test_event_entry>& events, bool main_first) const;
  std::vector<expectation> expectations() const override;
  bool deliver_main_lock(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events);
  bool deliver_rival_lock(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events);
  bool check_convergence(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events);
  mutable bool main_first_ = false;
};

struct gen_chainlock_divergent_main_first : gen_chainlock_divergent_order_base
{
  bool generate(std::vector<test_event_entry>& events) const { return generate_case(events, true); }
};
template<> struct get_test_options<gen_chainlock_divergent_main_first>: public get_height_window_test_options<HF_VERSION_QWC_CONTRACTS> {};

struct gen_chainlock_divergent_rival_first : gen_chainlock_divergent_order_base
{
  bool generate(std::vector<test_event_entry>& events) const { return generate_case(events, false); }
};
template<> struct get_test_options<gen_chainlock_divergent_rival_first>: public get_height_window_test_options<HF_VERSION_QWC_CONTRACTS> {};

struct gen_chainlock_divergent_higher : gen_chainlock_divergent_order_base
{
  gen_chainlock_divergent_higher()
  {
    REGISTER_CALLBACK_METHOD(gen_chainlock_divergent_higher, deliver_older_main_lock);
  }
  bool generate(std::vector<test_event_entry>& events) const;
  std::vector<expectation> expectations() const override;
  bool deliver_older_main_lock(cryptonote::core& c, size_t ev_index,
      const std::vector<test_event_entry>& events);
};
template<> struct get_test_options<gen_chainlock_divergent_higher>: public get_height_window_test_options<HF_VERSION_QWC_CONTRACTS> {};

struct gen_chainlock_during_batch : public gen_chainlock_base
{
  gen_chainlock_during_batch()
  {
    REGISTER_CALLBACK_METHOD(gen_chainlock_during_batch, check_concurrent_lock);
  }
  bool generate(std::vector<test_event_entry>& events) const;
  std::vector<expectation> expectations() const override;
  bool check_concurrent_lock(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events);
};
template<> struct get_test_options<gen_chainlock_during_batch>: public get_height_window_test_options<HF_VERSION_QWC_CONTRACTS> {};
