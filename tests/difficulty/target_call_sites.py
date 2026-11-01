from pathlib import Path
import re

root = Path(__file__).resolve().parents[2]
required = {
    "src/cryptonote_core/blockchain.cpp": [
        "size_t target = get_difficulty_target();",
        "block_target(m_nettype, get_ideal_hard_fork_version(height))",
        "block_target(m_nettype, get_ideal_hard_fork_version(bei.height))",
        "locked_tx_allowed_delta_seconds(m_nettype, get_current_hard_fork_version())",
        "(BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW + 1) * block_target(m_nettype) / 2",
        "timestamps.back() + block_target(m_nettype)",
        "return block_target(m_nettype, get_current_hard_fork_version());",
    ],
    "src/cryptonote_core/cryptonote_core.cpp": [
        "seconds[n] / block_target(m_nettype)",
        "block_rate_probability(b, seconds[n] / block_target(m_nettype))",
        "blocks_for_duration(m_nettype, seconds[0])",
    ],
    "src/daemon/rpc_command_executor.cpp": [
        "boost::multiprecision::uint128_t(backlog) * ires.target / 60",
        "full_reward_zone == 0",
        "divide_round_up(res.pool_stats.bytes_total, full_reward_zone)",
        "100.f * ires.target * chain.length / dt",
    ],
    "src/cryptonote_protocol/cryptonote_protocol_handler.inl": [
        "m_core.get_nettype() == DEVNET ? 0",
        "diff_v2 * block_target(m_core.get_nettype())",
    ],
    "src/rpc/core_rpc_server.cpp": ["res.block_target = m_core.get_blockchain_storage().get_difficulty_target();"],
    "src/wallet/wallet2.cpp": [
        "target_seconds * blocks_to_consider",
        "const int seconds_per_block = block_target(m_nettype);",
        "locked_tx_allowed_delta_seconds(m_nettype, block_height < v2height ? 1 : 2)",
        "new gamma_picker(rct_offsets, m_nettype)",
        "RECENT_OUTPUT_BLOCKS(m_nettype)",
    ],
    "src/wallet/wallet_rpc_server.cpp": [
        "const uint64_t target_seconds = block_target(nettype);",
        "cryptonote::divide_round_up(unlock_time - now, target_seconds)",
        "entry.unlock_time, m_wallet->nettype()",
    ],
    "src/simplewallet/simplewallet.cpp": ["locked_tx_allowed_delta_seconds(m_wallet->nettype(), m_wallet->use_fork_rules(2, 0) ? 2 : 1)"],
}
for name, snippets in required.items():
    source = (root / name).read_text()
    source = re.sub(r'(?m)^#line [0-9]+ "[^"]+"\n', '', source)
    for snippet in snippets:
        assert snippet in source, f"{name} lost network-aware target: {snippet}"
    assert "DIFFICULTY_TARGET_V2" not in source, f"{name} uses the fixed v2 target"
    assert not re.search(r"\b120\b", source), f"{name} has a literal 120-second target"

wallet = (root / "src/wallet/wallet2.cpp").read_text()
assert wallet.count("const int seconds_per_block = block_target(m_nettype);") == 2
simplewallet = (root / "src/simplewallet/simplewallet.cpp").read_text()
assert "nblocks_low * block_target(m_wallet->nettype()) / 60" in simplewallet
assert "nblocks_high * block_target(m_wallet->nettype()) / 60" in simplewallet
assert simplewallet.count("locked_tx_allowed_delta_seconds(m_wallet->nettype()") == 2
rpc = (root / "src/wallet/wallet_rpc_server.cpp").read_text()
for line in rpc.splitlines():
    if "set_confirmations(entry, m_wallet->" in line:
        assert "m_wallet->nettype()" in line, "wallet RPC confirmation call lost network type"

assert "cryptonote::divide_round_up(entry.amount, block_reward)" in rpc

daemon = (root / "src/daemon/rpc_command_executor.cpp").read_text()
alt_chain = daemon.split("bool t_rpc_command_executor::alt_chain_info(", 1)[1].split("bool t_rpc_command_executor::print_blockchain_dynamic_stats(", 1)[0]
alt_chain = re.sub(r"/\*.*?\*/|//[^\n]*", "", alt_chain, flags=re.S)
assert "return chain.length != 0 && chain.length <= chain.height;" in daemon
assert alt_chain.count("if (!valid_alt_chain_length(chain))") == 2
assert re.search(r"if \(!valid_alt_chain_length\(chain\)\)\s*continue;\s*if \(chain.length <= above\)", alt_chain)
assert "return height > start_height ? height - start_height - 1 : 0;" in daemon
assert alt_chain.count("alt_chain_depth(ires.height, start_height)") == 3
assert "if (bhres.block_headers.empty() || bhres.block_headers.size() - 1 != chain.length)" in alt_chain
assert alt_chain.count("if (chain.length > 1)") == 1
assert not re.search(r"\bif\s*\(\s*(?:false|0)\s*\)", alt_chain)
expected_diagnostic_branch = (
    'tools::msg_writer() << "Age: " << tools::get_human_readable_timespan(age);',
    "if (chain.length > 1)",
    "{",
    'tools::msg_writer() << "Time span: " << tools::get_human_readable_timespan(dt);',
    "cryptonote::difficulty_type start_difficulty(bhres.block_headers.back().wide_difficulty);",
    "if (start_difficulty > 0 && dt > 0)",
    'tools::msg_writer() << "Approximated " << 100.f * ires.target * chain.length / dt << "% of network hash rate";',
    "else if (dt == 0)",
    'tools::fail_msg_writer() << "Cannot estimate hash rate from a zero time span";',
    "else",
    'tools::fail_msg_writer() << "Bad block difficulty reported by daemon";',
    "}",
)
assert re.search(r"\s*".join(map(re.escape, expected_diagnostic_branch)), alt_chain)
