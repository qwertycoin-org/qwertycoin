// Copyright (c) 2026, Qwertycoin
//
// SPDX-License-Identifier: BSD-3-Clause

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "cryptonote_config.h"
#include "epose/service_epoch.h"
#include "epose/service_node.h"

namespace
{
  struct simulation_case
  {
    uint64_t total_nodes = 0;
    uint64_t attacker_nodes = 0;
    uint64_t epochs = 0;
    uint64_t min_attestations = 0;
    size_t committee_size = qwertycoin::epose::EPOSE_VERIFIER_COMMITTEE_SIZE;
  };

  struct simulation_result
  {
    uint64_t attacker_subject_trials = 0;
    uint64_t attacker_subjects_with_threshold = 0;
    uint64_t honest_subject_trials = 0;
    uint64_t honest_subjects_with_threshold = 0;
    uint64_t total_committees = 0;
    uint64_t total_committee_seats = 0;
    uint64_t controlled_committee_seats = 0;
    uint64_t full_attacker_committees = 0;
  };

  uint64_t parse_u64(const char *value, const char *name)
  {
    char *end = nullptr;
    const unsigned long long parsed = std::strtoull(value, &end, 10);
    if (!value[0] || (end && *end))
      throw std::invalid_argument(std::string("invalid ") + name + ": " + value);
    return static_cast<uint64_t>(parsed);
  }

  crypto::hash hash_text(const std::string &value)
  {
    return crypto::cn_fast_hash(value.data(), value.size());
  }

  crypto::public_key make_public_key(const std::string &domain, uint64_t index)
  {
    const crypto::hash hash = hash_text(domain + ":" + std::to_string(index));
    crypto::public_key key{};
    static_assert(sizeof(key) == sizeof(hash), "public key and hash size mismatch");
    std::memcpy(&key, &hash, sizeof(key));
    return key;
  }

  qwertycoin::epose::service_node_identity make_identity(uint64_t index)
  {
    qwertycoin::epose::service_node_identity identity{};
    identity.service_public_key = make_public_key("service", index);
    identity.reward_address.m_spend_public_key = make_public_key("reward-spend", index);
    identity.reward_address.m_view_public_key = make_public_key("reward-view", index);
    identity.endpoint_commitment = qwertycoin::epose::make_endpoint_commitment("sim-node-" + std::to_string(index) + ".example:8196");
    identity.registration_epoch = 1;
    identity.expiry_epoch = identity.registration_epoch + qwertycoin::epose::EPOSE_REGISTRATION_TTL_EPOCHS;
    return identity;
  }

  std::string key_string(const crypto::public_key &key)
  {
    return std::string(reinterpret_cast<const char *>(&key), sizeof(key));
  }

  bool is_controlled(const std::set<std::string> &controlled_keys, const crypto::public_key &key)
  {
    return controlled_keys.find(key_string(key)) != controlled_keys.end();
  }

  uint64_t rate_bps(uint64_t count, uint64_t total)
  {
    return total == 0 ? 0 : (count * 10000 + total / 2) / total;
  }

  simulation_result run_case(const simulation_case &scenario)
  {
    if (scenario.attacker_nodes > scenario.total_nodes)
      throw std::invalid_argument("attacker node count cannot exceed total node count");
    if (scenario.total_nodes < 2)
      throw std::invalid_argument("at least two total nodes are required");
    if (scenario.committee_size == 0)
      throw std::invalid_argument("committee size must be greater than zero");

    std::vector<qwertycoin::epose::service_node_identity> nodes;
    nodes.reserve(scenario.total_nodes);
    std::set<std::string> controlled_keys;
    for (uint64_t i = 0; i < scenario.total_nodes; ++i)
    {
      nodes.push_back(make_identity(i));
      if (i < scenario.attacker_nodes)
        controlled_keys.insert(key_string(nodes.back().service_public_key));
    }

    simulation_result result{};
    for (uint64_t epoch = 1; epoch <= scenario.epochs; ++epoch)
    {
      const crypto::hash seed_block_hash = hash_text("seed-block:" + std::to_string(epoch));
      const crypto::hash epoch_seed = qwertycoin::epose::calculate_epoch_seed(cryptonote::TESTNET, epoch, seed_block_hash);

      for (uint64_t subject_index = 0; subject_index < scenario.total_nodes; ++subject_index)
      {
        const bool attacker_subject = subject_index < scenario.attacker_nodes;
        const auto committee = qwertycoin::epose::select_verifiers(
            nodes,
            nodes[subject_index].service_public_key,
            cryptonote::TESTNET,
            epoch,
            epoch_seed,
            scenario.committee_size);

        uint64_t controlled_seats = 0;
        for (const auto &assignment : committee)
        {
          if (is_controlled(controlled_keys, assignment.verifier_public_key))
            ++controlled_seats;
        }
        const uint64_t required_attestations = scenario.min_attestations == 0
            ? qwertycoin::epose::required_attestations_for_committee_size(committee.size())
            : scenario.min_attestations;

        ++result.total_committees;
        result.total_committee_seats += committee.size();
        result.controlled_committee_seats += controlled_seats;
        if (controlled_seats == committee.size() && !committee.empty())
          ++result.full_attacker_committees;
        if (required_attestations == 0)
          continue;

        if (attacker_subject)
        {
          ++result.attacker_subject_trials;
          if (controlled_seats >= required_attestations)
            ++result.attacker_subjects_with_threshold;
        }
        else
        {
          ++result.honest_subject_trials;
          if (controlled_seats >= required_attestations)
            ++result.honest_subjects_with_threshold;
        }
      }
    }

    return result;
  }

  void print_result(const simulation_case &scenario, const simulation_result &result)
  {
    std::cout << "total_nodes=" << scenario.total_nodes
              << " attacker_nodes=" << scenario.attacker_nodes
              << " attacker_share_bps=" << rate_bps(scenario.attacker_nodes, scenario.total_nodes)
              << " epochs=" << scenario.epochs
              << " committee_size=" << scenario.committee_size
              << " min_attestations=" << (scenario.min_attestations == 0 ? "dynamic_2_of_3" : std::to_string(scenario.min_attestations))
              << " attacker_subject_trials=" << result.attacker_subject_trials
              << " attacker_subjects_with_threshold=" << result.attacker_subjects_with_threshold
              << " attacker_subject_threshold_rate_bps=" << rate_bps(result.attacker_subjects_with_threshold, result.attacker_subject_trials)
              << " honest_subject_trials=" << result.honest_subject_trials
              << " honest_subjects_with_attacker_threshold=" << result.honest_subjects_with_threshold
              << " honest_subject_threshold_rate_bps=" << rate_bps(result.honest_subjects_with_threshold, result.honest_subject_trials)
              << " avg_controlled_seats_bps=" << rate_bps(result.controlled_committee_seats, result.total_committee_seats)
              << " full_attacker_committee_rate_bps=" << rate_bps(result.full_attacker_committees, result.total_committees)
              << '\n';
  }

  std::vector<simulation_case> default_cases()
  {
    std::vector<simulation_case> cases;
    const uint64_t epochs = 64;
    const uint64_t totals[] = {10, 25, 50, 100};
    const uint64_t shares_bps[] = {1000, 2000, 2500, 3300, 4000, 5000};
    for (const uint64_t total : totals)
    {
      for (const uint64_t share : shares_bps)
      {
        uint64_t attacker_nodes = std::max<uint64_t>(1, (total * share + 9999) / 10000);
        if (attacker_nodes > total)
          attacker_nodes = total;
        cases.push_back({total, attacker_nodes, epochs, 2, 5});
        cases.push_back({total, attacker_nodes, epochs, 0, 5});
        cases.push_back({total, attacker_nodes, epochs, 0, 7});
        cases.push_back({total, attacker_nodes, epochs, 0, 9});
        cases.push_back({total, attacker_nodes, epochs, 0, 11});
      }
    }
    return cases;
  }
}

int main(int argc, char **argv)
{
  try
  {
    std::vector<simulation_case> cases;
    if (argc == 1)
    {
      cases = default_cases();
    }
    else
    {
      simulation_case scenario{};
      scenario.total_nodes = parse_u64(argv[1], "total nodes");
      scenario.attacker_nodes = argc > 2 ? parse_u64(argv[2], "attacker nodes") : std::max<uint64_t>(1, scenario.total_nodes / 10);
      scenario.epochs = argc > 3 ? parse_u64(argv[3], "epochs") : 64;
      scenario.min_attestations = argc > 4 ? parse_u64(argv[4], "minimum attestations, or 0 for dynamic") : 0;
      scenario.committee_size = argc > 5 ? static_cast<size_t>(parse_u64(argv[5], "committee size")) : qwertycoin::epose::EPOSE_VERIFIER_COMMITTEE_SIZE;
      cases.push_back(scenario);
    }

    for (const auto &scenario : cases)
      print_result(scenario, run_case(scenario));

    return 0;
  }
  catch (const std::exception &e)
  {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
