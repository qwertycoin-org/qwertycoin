#pragma once

#include "epose/membership_v2.h"

struct membership_pipeline_test_access
{
  using pipeline = qwertycoin::epose::membership_pipeline_v2;
  using member = qwertycoin::epose::frozen_member_v2;
  using qualification = qwertycoin::epose::qualification_set_v2;

  static void remove_selected_leases(pipeline &membership, uint64_t epoch)
  {
    membership.epochs_.at(epoch)->snapshot_lease_hashes.clear();
  }

  static void prune_snapshot(pipeline &membership, uint64_t epoch)
  {
    auto &snapshot = *membership.epochs_.at(epoch)->snapshot;
    snapshot.members.clear();
    snapshot.pruned = true;
  }

  static void add_member(pipeline &membership, pipeline::undo_journal &undo,
      uint64_t epoch, const member &value, const crypto::hash &lease_hash)
  {
    qwertycoin::epose::admission_lease_v2 lease{};
    lease.member = value;
    lease.target_epoch = epoch;
    lease.lease_hash = lease_hash;
    membership.mutable_bucket(epoch, &undo).leases.push_back({lease, 0});
  }

  static void qualify(pipeline &membership, pipeline::undo_journal &undo,
      uint64_t epoch, const crypto::public_key &key)
  {
    std::string index(reinterpret_cast<const char *>(&key), sizeof(key));
    membership.mutable_bucket(epoch, &undo).receipt_coverage[{0, index}] = 1;
  }

  static void set_qualification(pipeline &membership, pipeline::undo_journal &undo,
      uint64_t epoch, qualification value)
  {
    membership.mutable_bucket(epoch, &undo).qualification = std::move(value);
  }
};
