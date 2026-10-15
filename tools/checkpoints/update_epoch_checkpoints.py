#!/usr/bin/env python3
"""Propose reviewed Reset-1 mainnet checkpoints at finalized EPoSE boundaries.

The collector is deliberately not a consensus authority.  It requires every
configured Core RPC endpoint to report the same block hash and cumulative
difficulty, cross-checks the hash through the public Explorer, and only edits a
review branch after the boundary is at least 60 blocks deep.  A human-reviewed
merge is still required before any checkpoint becomes part of Core.
"""

from __future__ import annotations

import argparse
import dataclasses
import datetime as dt
import json
import pathlib
import re
import sys
import time
import urllib.error
import urllib.request


EPOCH_LENGTH = 720
MINIMUM_FINALITY_DEPTH = 60
EXPECTED_MAJOR_VERSION = 17
DEFAULT_ENDPOINTS = tuple(
    f"http://seed-{index:02d}.qwertycoin.org:8198" for index in range(6)
)
DEFAULT_EXPLORER_TEMPLATE = "https://explorer.qwertycoin.org/api/block/{height}"
MAX_RESPONSE_BYTES = 1024 * 1024
REQUEST_ATTEMPTS = 3
HASH_RE = re.compile(r"^[0-9a-f]{64}$")
DIFFICULTY_RE = re.compile(r"^0x[0-9a-f]+$")
CHECKPOINT_RE = re.compile(
    r'ADD_CHECKPOINT2\(\s*(\d+),\s*"([0-9a-f]{64})",\s*"(0x[0-9a-f]+)"\s*\);'
)
TEST_COUNT_RE = re.compile(
    r"(std::array<expected_checkpoint,\s*)(\d+)(>\s+expected_mainnet_checkpoints)"
)
TEST_ENTRY_RE = re.compile(
    r'\{\s*(\d+),\s*"([0-9a-f]{64})",\s*"(0x[0-9a-f]+)"\s*\},'
)
DOC_ENTRY_RE = re.compile(
    r'^\|\s*(\d+)\s*\|[^|]*\|\s*`([0-9a-f]{64})`\s*\|\s*`(0x[0-9a-f]+)`\s*\|$',
    re.MULTILINE,
)


class SnapshotError(RuntimeError):
    """Fail-closed snapshot collection or rendering error."""


@dataclasses.dataclass(frozen=True)
class TipObservation:
    endpoint: str
    block_count: int
    tip_height: int
    top_block_hash: str


@dataclasses.dataclass(frozen=True)
class HeaderObservation:
    endpoint: str
    height: int
    block_hash: str
    cumulative_difficulty: str
    major_version: int


@dataclasses.dataclass(frozen=True)
class CheckpointCandidate:
    epoch: int
    height: int
    block_hash: str
    cumulative_difficulty: str
    major_version: int
    minimum_depth: int
    headers: tuple[HeaderObservation, ...]
    explorer_current_height: int


def _require_int(value: object, field: str) -> int:
    if isinstance(value, bool) or not isinstance(value, int):
        raise SnapshotError(f"{field} must be an integer")
    return value


def _require_hash(value: object, field: str) -> str:
    if not isinstance(value, str) or not HASH_RE.fullmatch(value):
        raise SnapshotError(f"{field} must be a lowercase 64-character hash")
    return value


def request_json(url: str, *, payload: dict | None = None, timeout: float = 15.0) -> dict:
    body = None if payload is None else json.dumps(payload).encode("utf-8")
    headers = {"Accept": "application/json", "User-Agent": "qwc-checkpoint-proposer/1"}
    if body is not None:
        headers["Content-Type"] = "application/json"
    last_error: Exception | None = None
    for attempt in range(REQUEST_ATTEMPTS):
        request = urllib.request.Request(url, data=body, headers=headers)
        try:
            with urllib.request.urlopen(request, timeout=timeout) as response:
                response_bytes = response.read(MAX_RESPONSE_BYTES + 1)
            if len(response_bytes) > MAX_RESPONSE_BYTES:
                raise SnapshotError(
                    f"response from {url!r} exceeded {MAX_RESPONSE_BYTES} bytes"
                )
            result = json.loads(response_bytes)
            break
        except (urllib.error.URLError, TimeoutError, json.JSONDecodeError, UnicodeDecodeError) as exc:
            last_error = exc
            if attempt + 1 < REQUEST_ATTEMPTS:
                time.sleep(2 ** attempt)
    else:
        assert last_error is not None
        raise SnapshotError(
            f"request failed for {url!r} after {REQUEST_ATTEMPTS} attempts: "
            f"{type(last_error).__name__}"
        ) from last_error
    if not isinstance(result, dict):
        raise SnapshotError(f"{url!r} returned a non-object JSON response")
    return result


def rpc_result(endpoint: str, method: str, params: dict, timeout: float) -> dict:
    response = request_json(
        endpoint.rstrip("/") + "/json_rpc",
        payload={"jsonrpc": "2.0", "id": "checkpoint-proposer", "method": method, "params": params},
        timeout=timeout,
    )
    if response.get("error") not in (None, {}):
        raise SnapshotError(f"{endpoint} rejected {method}: {response['error']!r}")
    result = response.get("result")
    if not isinstance(result, dict):
        raise SnapshotError(f"{endpoint} returned no object result for {method}")
    return result


def read_tip(endpoint: str, timeout: float) -> TipObservation:
    result = rpc_result(endpoint, "get_info", {}, timeout)
    if result.get("mainnet") is not True:
        raise SnapshotError(f"{endpoint} is not reporting mainnet")
    if result.get("synchronized") is not True or result.get("offline") is True:
        raise SnapshotError(f"{endpoint} is not synchronized and online")
    block_count = _require_int(result.get("height"), f"{endpoint} height")
    if block_count <= 0:
        raise SnapshotError(f"{endpoint} returned an empty chain")
    return TipObservation(
        endpoint=endpoint,
        block_count=block_count,
        tip_height=block_count - 1,
        top_block_hash=_require_hash(result.get("top_block_hash"), f"{endpoint} top_block_hash"),
    )


def read_header(endpoint: str, height: int, timeout: float) -> HeaderObservation:
    result = rpc_result(endpoint, "get_block_header_by_height", {"height": height}, timeout)
    header = result.get("block_header")
    if not isinstance(header, dict):
        raise SnapshotError(f"{endpoint} returned no block header at height {height}")
    reported_height = _require_int(header.get("height"), f"{endpoint} header height")
    if reported_height != height:
        raise SnapshotError(
            f"{endpoint} returned height {reported_height} for requested height {height}"
        )
    if header.get("orphan_status") is not False:
        raise SnapshotError(f"{endpoint} reported an orphan at height {height}")
    difficulty = header.get("wide_cumulative_difficulty")
    if not isinstance(difficulty, str) or not DIFFICULTY_RE.fullmatch(difficulty):
        raise SnapshotError(f"{endpoint} returned invalid cumulative difficulty at height {height}")
    major_version = _require_int(header.get("major_version"), f"{endpoint} major_version")
    return HeaderObservation(
        endpoint=endpoint,
        height=height,
        block_hash=_require_hash(header.get("hash"), f"{endpoint} block hash"),
        cumulative_difficulty=difficulty,
        major_version=major_version,
    )


def latest_mature_epoch_end(tip_height: int, minimum_depth: int) -> int | None:
    mature_height = tip_height - minimum_depth
    if mature_height < EPOCH_LENGTH - 1:
        return None
    return ((mature_height + 1) // EPOCH_LENGTH) * EPOCH_LENGTH - 1


def parse_source_checkpoints(text: str) -> dict[int, tuple[str, str]]:
    boundary_marker = text.find(
        "// Reset-1 mainnet: completed 720-block EPoSE epoch boundaries."
    )
    if boundary_marker < 0:
        raise SnapshotError("mainnet checkpoint section marker is missing")
    start = text.rfind("    ADD_CHECKPOINT2(0,", 0, boundary_marker)
    if start < 0:
        raise SnapshotError("mainnet genesis checkpoint is missing")
    end = text.find("    return true;", start)
    if end < 0:
        raise SnapshotError("mainnet checkpoint section terminator is missing")
    return {
        int(height): (block_hash, difficulty)
        for height, block_hash, difficulty in CHECKPOINT_RE.findall(text[start:end])
    }


def parse_test_checkpoints(text: str) -> dict[int, tuple[str, str]]:
    return {
        int(height): (block_hash, difficulty)
        for height, block_hash, difficulty in TEST_ENTRY_RE.findall(text)
    }


def parse_doc_checkpoints(text: str) -> dict[int, tuple[str, str]]:
    return {
        int(height): (block_hash, difficulty)
        for height, block_hash, difficulty in DOC_ENTRY_RE.findall(text)
    }


def verify_repository_consistency(root: pathlib.Path) -> dict[int, tuple[str, str]]:
    source = parse_source_checkpoints((root / "src/checkpoints/checkpoints.cpp").read_text())
    tests = parse_test_checkpoints((root / "tests/unit_tests/checkpoints.cpp").read_text())
    docs = parse_doc_checkpoints((root / "docs/MAINNET_CHECKPOINTS.md").read_text())
    if source != tests or source != docs:
        raise SnapshotError("source, unit-test, and documentation checkpoint tables differ")
    epoch_heights = [height for height in source if height % EPOCH_LENGTH == EPOCH_LENGTH - 1]
    if not epoch_heights:
        raise SnapshotError("repository contains no EPoSE epoch-boundary checkpoint")
    if sorted(epoch_heights) != list(
        range(min(epoch_heights), max(epoch_heights) + 1, EPOCH_LENGTH)
    ):
        raise SnapshotError("repository EPoSE epoch-boundary checkpoints are not contiguous")
    return source


def verify_explorer(height: int, expected_hash: str, template: str, timeout: float) -> int:
    url = template.format(height=height)
    if not url.startswith("https://"):
        raise SnapshotError("Explorer cross-check URL must use HTTPS")
    response = request_json(url, timeout=timeout)
    if response.get("status") != "success" or not isinstance(response.get("data"), dict):
        raise SnapshotError(f"Explorer returned an unsuccessful response for height {height}")
    data = response["data"]
    if _require_int(data.get("block_height"), "Explorer block_height") != height:
        raise SnapshotError(f"Explorer returned a different height for {height}")
    if _require_hash(data.get("hash"), "Explorer hash") != expected_hash:
        raise SnapshotError(f"Explorer hash disagrees at height {height}")
    return _require_int(data.get("current_height"), "Explorer current_height")


def collect_candidates(
    root: pathlib.Path,
    endpoints: tuple[str, ...],
    explorer_template: str,
    minimum_depth: int,
    expected_major_version: int,
    timeout: float,
) -> tuple[tuple[TipObservation, ...], tuple[CheckpointCandidate, ...]]:
    if len(endpoints) < 3 or len(set(endpoints)) != len(endpoints):
        raise SnapshotError("at least three distinct Core RPC endpoints are required")
    if minimum_depth < MINIMUM_FINALITY_DEPTH:
        raise SnapshotError(f"minimum depth cannot be below {MINIMUM_FINALITY_DEPTH}")

    existing = verify_repository_consistency(root)
    tips = tuple(read_tip(endpoint, timeout) for endpoint in endpoints)
    minimum_tip = min(tip.tip_height for tip in tips)
    latest_boundary = latest_mature_epoch_end(minimum_tip, minimum_depth)
    if latest_boundary is None:
        return tips, ()

    existing_epoch_heights = sorted(
        height for height in existing if height % EPOCH_LENGTH == EPOCH_LENGTH - 1
    )
    first_missing = existing_epoch_heights[-1] + EPOCH_LENGTH
    candidates: list[CheckpointCandidate] = []
    for height in range(first_missing, latest_boundary + 1, EPOCH_LENGTH):
        headers = tuple(read_header(endpoint, height, timeout) for endpoint in endpoints)
        identities = {
            (header.block_hash, header.cumulative_difficulty, header.major_version)
            for header in headers
        }
        if len(identities) != 1:
            raise SnapshotError(f"Core RPC endpoints disagree at height {height}")
        block_hash, difficulty, major_version = identities.pop()
        if major_version != expected_major_version:
            raise SnapshotError(
                f"height {height} reports major version {major_version}, expected {expected_major_version}"
            )
        minimum_observed_depth = min(tip.tip_height - height for tip in tips)
        if minimum_observed_depth < minimum_depth:
            raise SnapshotError(f"height {height} is only {minimum_observed_depth} blocks deep")
        explorer_current_height = verify_explorer(
            height, block_hash, explorer_template, timeout
        )
        if explorer_current_height - 1 - height < minimum_depth:
            raise SnapshotError(f"Explorer has not observed finality depth for height {height}")
        candidates.append(
            CheckpointCandidate(
                epoch=(height + 1) // EPOCH_LENGTH - 1,
                height=height,
                block_hash=block_hash,
                cumulative_difficulty=difficulty,
                major_version=major_version,
                minimum_depth=minimum_observed_depth,
                headers=headers,
                explorer_current_height=explorer_current_height,
            )
        )
    return tips, tuple(candidates)


def _append_source_entries(text: str, candidates: tuple[CheckpointCandidate, ...]) -> str:
    if not candidates:
        return text
    start = text.index("// Reset-1 mainnet: completed 720-block EPoSE epoch boundaries.")
    insert_at = text.index("    return true;", start)
    entries = "".join(
        f'    ADD_CHECKPOINT2({item.height}, "{item.block_hash}", '
        f'"{item.cumulative_difficulty}");\n'
        for item in candidates
    )
    return text[:insert_at] + entries + text[insert_at:]


def _append_test_entries(text: str, candidates: tuple[CheckpointCandidate, ...]) -> str:
    if not candidates:
        return text
    match = TEST_COUNT_RE.search(text)
    if match is None:
        raise SnapshotError("unit-test checkpoint array declaration is missing")
    new_count = int(match.group(2)) + len(candidates)
    text = text[:match.start(2)] + str(new_count) + text[match.end(2):]
    insert_at = text.index("  }};", match.end())
    entries = "".join(
        f'    {{{item.height}, "{item.block_hash}", '
        f'"{item.cumulative_difficulty}"}},\n'
        for item in candidates
    )
    return text[:insert_at] + entries + text[insert_at:]


def _append_doc_entries(text: str, candidates: tuple[CheckpointCandidate, ...]) -> str:
    if not candidates:
        return text
    matches = list(DOC_ENTRY_RE.finditer(text))
    if not matches:
        raise SnapshotError("documentation checkpoint table is missing")
    insert_at = matches[-1].end()
    entries = "".join(
        f"\n| {item.height} | Epoch {item.epoch} end | `{item.block_hash}` | "
        f"`{item.cumulative_difficulty}` |"
        for item in candidates
    )
    return text[:insert_at] + entries + text[insert_at:]


def apply_candidates(root: pathlib.Path, candidates: tuple[CheckpointCandidate, ...]) -> None:
    paths_and_renderers = (
        (root / "src/checkpoints/checkpoints.cpp", _append_source_entries),
        (root / "tests/unit_tests/checkpoints.cpp", _append_test_entries),
        (root / "docs/MAINNET_CHECKPOINTS.md", _append_doc_entries),
    )
    for path, renderer in paths_and_renderers:
        before = path.read_text()
        after = renderer(before, candidates)
        if after != before:
            path.write_text(after)
    verify_repository_consistency(root)


def evidence_document(
    observed_at: str,
    tips: tuple[TipObservation, ...],
    candidates: tuple[CheckpointCandidate, ...],
    minimum_depth: int,
    explorer_template: str,
) -> dict:
    return {
        "schema": "qwc-mainnet-epoch-checkpoint-evidence-v1",
        "observed_at": observed_at,
        "epoch_length": EPOCH_LENGTH,
        "minimum_finality_depth": minimum_depth,
        "explorer_template": explorer_template,
        "tips": [dataclasses.asdict(tip) for tip in tips],
        "candidates": [dataclasses.asdict(candidate) for candidate in candidates],
    }


def markdown_summary(evidence: dict) -> str:
    candidates = evidence["candidates"]
    lines = [
        "# Automated mainnet epoch-checkpoint proposal",
        "",
        f"Observed at `{evidence['observed_at']}` from {len(evidence['tips'])} Core RPC endpoints.",
        "The public Explorer supplied an HTTPS block-hash cross-check. This evidence is a",
        "review input, not authority to merge a consensus checkpoint.",
        "",
    ]
    if not candidates:
        lines.append("No new EPoSE epoch boundary has reached the required finality depth.")
        return "\n".join(lines) + "\n"
    lines.extend(
        [
            "| Epoch | Height | Block hash | Cumulative difficulty | Minimum observed depth |",
            "| ---: | ---: | --- | ---: | ---: |",
        ]
    )
    for candidate in candidates:
        lines.append(
            f"| {candidate['epoch']} | {candidate['height']} | `{candidate['block_hash']}` | "
            f"`{candidate['cumulative_difficulty']}` | {candidate['minimum_depth']} |"
        )
    lines.extend(
        [
            "",
            "Before merge, a maintainer must independently inspect the candidate, CI results,",
            "and an additional public chain view. DNS checkpoint publication is out of scope.",
        ]
    )
    return "\n".join(lines) + "\n"


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("plan", "apply"))
    parser.add_argument("--source-root", type=pathlib.Path, default=pathlib.Path(__file__).resolve().parents[2])
    parser.add_argument("--endpoint", action="append", dest="endpoints")
    parser.add_argument("--explorer-template", default=DEFAULT_EXPLORER_TEMPLATE)
    parser.add_argument("--minimum-depth", type=int, default=MINIMUM_FINALITY_DEPTH)
    parser.add_argument("--expected-major-version", type=int, default=EXPECTED_MAJOR_VERSION)
    parser.add_argument("--timeout", type=float, default=15.0)
    parser.add_argument("--evidence", type=pathlib.Path)
    parser.add_argument("--summary", type=pathlib.Path)
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(sys.argv[1:] if argv is None else argv)
    root = args.source_root.resolve()
    endpoints = tuple(args.endpoints or DEFAULT_ENDPOINTS)
    observed_at = dt.datetime.now(dt.timezone.utc).replace(microsecond=0).isoformat()
    try:
        tips, candidates = collect_candidates(
            root=root,
            endpoints=endpoints,
            explorer_template=args.explorer_template,
            minimum_depth=args.minimum_depth,
            expected_major_version=args.expected_major_version,
            timeout=args.timeout,
        )
        if args.mode == "apply":
            apply_candidates(root, candidates)
        evidence = evidence_document(
            observed_at, tips, candidates, args.minimum_depth, args.explorer_template
        )
        if args.evidence:
            args.evidence.parent.mkdir(parents=True, exist_ok=True)
            args.evidence.write_text(json.dumps(evidence, indent=2, sort_keys=True) + "\n")
        if args.summary:
            args.summary.parent.mkdir(parents=True, exist_ok=True)
            args.summary.write_text(markdown_summary(evidence))
        print(
            json.dumps(
                {
                    "status": "updated" if args.mode == "apply" and candidates else "unchanged",
                    "candidate_count": len(candidates),
                    "candidate_heights": [candidate.height for candidate in candidates],
                },
                sort_keys=True,
            )
        )
        return 0
    except SnapshotError as exc:
        print(f"checkpoint proposal failed closed: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
