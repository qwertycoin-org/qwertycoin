#!/usr/bin/env python3
"""Publish verified Core/GUI release hashes as DNSSEC-backed TXT metadata.

The Cloudflare token is accepted only through ``CLOUDFLARE_API_TOKEN``.  It is
never accepted on the command line and is never printed.  GitHub release
metadata is read with the workflow-provided ``GITHUB_TOKEN`` when available.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
from dataclasses import dataclass


GITHUB_API = "https://api.github.com"
CLOUDFLARE_API = "https://api.cloudflare.com/client/v4"
CORE_REPOSITORY = "qwertycoin-org/qwertycoin"
GUI_REPOSITORY = "qwertycoin-org/qwertycoin-gui"
ZONE_NAME = "qwertycoin.org"
RECORD_NAME = "updates.qwertycoin.org"
RECORD_TTL = 300
TAG_RE = re.compile(r"^v(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$")
SHA256_RE = re.compile(r"^[0-9a-f]{64}$")


class _NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, request, file_pointer, code, message, headers, new_url):
        return None


NO_REDIRECT_OPENER = urllib.request.build_opener(_NoRedirect)


@dataclass(frozen=True)
class Channel:
    software: str
    build_tag: str
    repository: str
    filename: str

    @property
    def key(self) -> tuple[str, str]:
        return self.software, self.build_tag


def channels_for(tag: str) -> tuple[Channel, ...]:
    version = version_from_tag(tag)
    return (
        Channel("qwertycoin", "linux-x64", CORE_REPOSITORY,
                f"qwertycoin-v{version}-linux-x86_64.tar.gz"),
        Channel("qwertycoin", "mac-armv8", CORE_REPOSITORY,
                f"qwertycoin-v{version}-macos-arm64.tar.gz"),
        Channel("qwertycoin", "win-x64", CORE_REPOSITORY,
                f"qwertycoin-v{version}-windows-x86_64.zip"),
        Channel("qwertycoin-gui", "linux-x64", GUI_REPOSITORY,
                f"qwertycoin-gui-v{version}-linux-x86_64.tar.gz"),
        Channel("qwertycoin-gui", "mac-armv8", GUI_REPOSITORY,
                f"qwertycoin-gui-v{version}-macos-arm64.dmg"),
        Channel("qwertycoin-gui", "install-win-x64", GUI_REPOSITORY,
                f"qwertycoin-gui-v{version}-windows-x86_64-setup.exe"),
        Channel("qwertycoin-gui", "win-x64", GUI_REPOSITORY,
                f"qwertycoin-gui-v{version}-windows-x86_64.zip"),
    )


def expected_assets(tag: str, repository: str) -> set[str]:
    version = version_from_tag(tag)
    if repository == CORE_REPOSITORY:
        return {
            f"qwertycoin-v{version}-linux-x86_64.tar.gz",
            f"qwertycoin-v{version}-macos-arm64.tar.gz",
            f"qwertycoin-v{version}-windows-x86_64.zip",
            "SHA256SUMS",
        }
    if repository == GUI_REPOSITORY:
        return {
            f"qwertycoin-gui-v{version}-linux-x86_64.tar.gz",
            f"qwertycoin-gui-v{version}-macos-arm64.dmg",
            f"qwertycoin-gui-v{version}-macos-arm64.tar.gz",
            f"qwertycoin-gui-v{version}-windows-x86_64-setup.exe",
            f"qwertycoin-gui-v{version}-windows-x86_64.zip",
            "SHA256SUMS",
        }
    raise RuntimeError(f"unsupported release repository: {repository}")


def version_from_tag(tag: str) -> str:
    match = TAG_RE.fullmatch(tag)
    if not match:
        raise RuntimeError(f"stable release tag must be vMAJOR.MINOR.PATCH: {tag!r}")
    return ".".join(match.groups())


def version_tuple(tag_or_version: str) -> tuple[int, int, int]:
    tag = tag_or_version if tag_or_version.startswith("v") else "v" + tag_or_version
    match = TAG_RE.fullmatch(tag)
    if not match:
        raise RuntimeError(f"invalid stable version: {tag_or_version!r}")
    return tuple(int(part) for part in match.groups())


def request_json(url: str, *, token: str | None = None, method: str = "GET",
                 payload: dict | None = None,
                 accept: str = "application/vnd.github+json") -> dict | list:
    data = None if payload is None else json.dumps(payload).encode("utf-8")
    headers = {"Accept": accept, "User-Agent": "qwc-release-dns/1"}
    if token:
        headers["Authorization"] = "Bearer " + token
    if payload is not None:
        headers["Content-Type"] = "application/json"
    request = urllib.request.Request(url, data=data, method=method, headers=headers)
    try:
        opener = NO_REDIRECT_OPENER.open if token else urllib.request.urlopen
        with opener(request, timeout=30) as response:
            return json.load(response)
    except urllib.error.HTTPError as exc:
        raise RuntimeError(f"HTTP {exc.code} from {urllib.parse.urlsplit(url).netloc}") from exc


def request_bytes(url: str, *, token: str | None = None) -> bytes:
    headers = {"Accept": "application/octet-stream", "User-Agent": "qwc-release-dns/1"}
    if token:
        headers["Authorization"] = "Bearer " + token
    request = urllib.request.Request(url, headers=headers)
    try:
        with urllib.request.urlopen(request, timeout=30) as response:
            return response.read()
    except urllib.error.HTTPError as exc:
        raise RuntimeError(f"HTTP {exc.code} from {urllib.parse.urlsplit(url).netloc}") from exc


def stable_release_tags(repository: str, github_token: str | None) -> set[str]:
    releases = request_json(
        f"{GITHUB_API}/repos/{repository}/releases?per_page=100", token=github_token)
    if not isinstance(releases, list):
        raise RuntimeError(f"unexpected GitHub release response for {repository}")
    tags = set()
    for release in releases:
        tag = release.get("tag_name", "")
        if not release.get("draft") and not release.get("prerelease") and TAG_RE.fullmatch(tag):
            tags.add(tag)
    return tags


def latest_common_stable_tag(github_token: str | None) -> str:
    common = stable_release_tags(CORE_REPOSITORY, github_token) & stable_release_tags(
        GUI_REPOSITORY, github_token)
    if not common:
        raise RuntimeError("Core and GUI have no common stable release tag")
    return max(common, key=version_tuple)


def parse_sha256sums(data: bytes) -> dict[str, str]:
    try:
        text = data.decode("ascii")
    except UnicodeDecodeError as exc:
        raise RuntimeError("SHA256SUMS is not ASCII") from exc
    result: dict[str, str] = {}
    for line in text.splitlines():
        match = re.fullmatch(r"([0-9a-f]{64}) [ *]([A-Za-z0-9][A-Za-z0-9._-]*)", line)
        if not match:
            raise RuntimeError(f"malformed SHA256SUMS line: {line!r}")
        digest, filename = match.groups()
        if filename in result:
            raise RuntimeError(f"duplicate SHA256SUMS filename: {filename}")
        result[filename] = digest
    if not result:
        raise RuntimeError("SHA256SUMS is empty")
    return result


def verified_release_hashes(repository: str, tag: str, github_token: str | None) -> dict[str, str]:
    release = request_json(
        f"{GITHUB_API}/repos/{repository}/releases/tags/{urllib.parse.quote(tag)}",
        token=github_token,
    )
    if not isinstance(release, dict):
        raise RuntimeError(f"unexpected GitHub release response for {repository} {tag}")
    if release.get("tag_name") != tag or release.get("draft") or release.get("prerelease"):
        raise RuntimeError(f"{repository} {tag} is not an exact public stable release")
    assets = release.get("assets")
    if not isinstance(assets, list):
        raise RuntimeError(f"{repository} {tag} has no asset list")
    by_name: dict[str, dict] = {}
    for asset in assets:
        name = asset.get("name")
        if not isinstance(name, str) or name in by_name:
            raise RuntimeError(f"{repository} {tag} has invalid or duplicate assets")
        by_name[name] = asset
    expected = expected_assets(tag, repository)
    if set(by_name) != expected:
        raise RuntimeError(
            f"{repository} {tag} asset set differs: expected={sorted(expected)}, actual={sorted(by_name)}")

    sums_asset = by_name["SHA256SUMS"]
    sums_url = sums_asset.get("browser_download_url")
    if not isinstance(sums_url, str) or not sums_url.startswith(
            f"https://github.com/{repository}/releases/download/{tag}/"):
        raise RuntimeError(f"{repository} {tag} has an unexpected SHA256SUMS URL")
    # Release assets are public.  Downloading without Authorization prevents a
    # credential from being forwarded to GitHub's object-storage redirect.
    sums_bytes = request_bytes(sums_url)
    import hashlib
    sums_digest = hashlib.sha256(sums_bytes).hexdigest()
    if sums_asset.get("digest") != "sha256:" + sums_digest:
        raise RuntimeError(f"{repository} {tag} SHA256SUMS GitHub digest mismatch")
    sums = parse_sha256sums(sums_bytes)
    if set(sums) != expected - {"SHA256SUMS"}:
        raise RuntimeError(f"{repository} {tag} SHA256SUMS asset set differs")
    for filename, digest in sums.items():
        if by_name[filename].get("state") != "uploaded":
            raise RuntimeError(f"{repository} {tag} asset is not uploaded: {filename}")
        if by_name[filename].get("digest") != "sha256:" + digest:
            raise RuntimeError(f"{repository} {tag} asset digest mismatch: {filename}")
    return sums


def desired_records(tag: str, github_token: str | None) -> dict[tuple[str, str], str]:
    version = version_from_tag(tag)
    hashes = {
        CORE_REPOSITORY: verified_release_hashes(CORE_REPOSITORY, tag, github_token),
        GUI_REPOSITORY: verified_release_hashes(GUI_REPOSITORY, tag, github_token),
    }
    return {
        channel.key: f"{channel.software}:{channel.build_tag}:{version}:{hashes[channel.repository][channel.filename]}"
        for channel in channels_for(tag)
    }


def canonical_txt(content: str) -> str:
    if len(content) >= 2 and content[0] == content[-1] == '"':
        content = content[1:-1]
    if '"' in content or "\\" in content:
        raise RuntimeError(f"unexpected TXT quoting: {content!r}")
    return content


def record_key(content: str) -> tuple[str, str]:
    fields = canonical_txt(content).split(":")
    if len(fields) != 4 or not SHA256_RE.fullmatch(fields[3]):
        raise RuntimeError(f"unexpected updater TXT record: {content!r}")
    version_tuple(fields[2])
    return fields[0], fields[1]


def cloudflare_request(token: str, method: str, path: str,
                       payload: dict | None = None) -> dict:
    data = None if payload is None else json.dumps(payload).encode("utf-8")
    headers = {"Authorization": "Bearer " + token, "Content-Type": "application/json"}
    request = urllib.request.Request(
        CLOUDFLARE_API + path, data=data, method=method, headers=headers)
    try:
        with NO_REDIRECT_OPENER.open(request, timeout=30) as response:
            result = json.load(response)
    except urllib.error.HTTPError as exc:
        raise RuntimeError(f"Cloudflare API HTTP {exc.code}") from exc
    if not result.get("success"):
        raise RuntimeError("Cloudflare API rejected the request")
    return result


def load_cloudflare_state(token: str, desired: dict[tuple[str, str], str]) -> tuple[str, list[dict]]:
    query = urllib.parse.urlencode({"name": ZONE_NAME, "status": "active"})
    zones = cloudflare_request(token, "GET", "/zones?" + query)["result"]
    if len(zones) != 1:
        raise RuntimeError(f"expected exactly one active {ZONE_NAME} zone")
    zone_id = zones[0]["id"]
    query = urllib.parse.urlencode({"type": "TXT", "name": RECORD_NAME, "per_page": 100})
    records = cloudflare_request(token, "GET", f"/zones/{zone_id}/dns_records?{query}")["result"]
    if len(records) != len(desired):
        raise RuntimeError(f"expected exactly {len(desired)} updater TXT records, got {len(records)}")
    keyed: dict[tuple[str, str], dict] = {}
    for record in records:
        if record.get("type") != "TXT" or record.get("name") != RECORD_NAME:
            raise RuntimeError("Cloudflare returned an unexpected updater record")
        key = record_key(record["content"])
        if key in keyed:
            raise RuntimeError(f"duplicate updater channel: {key}")
        keyed[key] = record
    if set(keyed) != set(desired):
        raise RuntimeError("Cloudflare updater channels differ from the compiled allowlist")
    return zone_id, records


def current_rrset(records: list[dict]) -> set[str]:
    return {canonical_txt(record["content"]) for record in records}


def validate_transition(records: list[dict], desired: dict[tuple[str, str], str]) -> str:
    current_versions = {canonical_txt(record["content"]).split(":")[2] for record in records}
    if len(current_versions) != 1:
        raise RuntimeError(f"current updater RRset mixes versions: {sorted(current_versions)}")
    current_version = current_versions.pop()
    desired_version = next(iter(desired.values())).split(":")[2]
    if version_tuple(desired_version) < version_tuple(current_version):
        raise RuntimeError(f"refusing updater downgrade from {current_version} to {desired_version}")
    if version_tuple(desired_version) == version_tuple(current_version):
        if current_rrset(records) != set(desired.values()):
            raise RuntimeError(
                f"release assets changed after DNS publication for version {desired_version}")
        return "unchanged"
    return "upgrade"


def doh_rrset(url: str) -> tuple[bool, set[str]]:
    result = request_json(url, accept="application/dns-json")
    if not isinstance(result, dict) or result.get("Status") != 0:
        return False, set()
    answers = result.get("Answer", [])
    rrset = {
        canonical_txt(answer["data"])
        for answer in answers
        if answer.get("type") == 16 and answer.get("name", "").rstrip(".") == RECORD_NAME
    }
    return result.get("AD") is True, rrset


def verify_public_dns(desired: set[str], attempts: int = 24, delay: int = 15) -> None:
    query = urllib.parse.urlencode({"name": RECORD_NAME, "type": "TXT", "do": "1"})
    resolvers = (
        "https://cloudflare-dns.com/dns-query?" + query,
        "https://dns.google/resolve?" + query,
    )
    for attempt in range(attempts):
        states = [doh_rrset(url) for url in resolvers]
        if all(authenticated and rrset == desired for authenticated, rrset in states):
            return
        if attempt + 1 < attempts:
            time.sleep(delay)
    raise RuntimeError("public DNSSEC resolver verification timed out")


def sanitized(records: list[dict]) -> list[dict]:
    return sorted(
        ({"content": canonical_txt(record["content"]), "ttl": record.get("ttl")}
         for record in records),
        key=lambda row: row["content"],
    )


def publish(token: str, desired: dict[tuple[str, str], str]) -> dict:
    zone_id, before = load_cloudflare_state(token, desired)
    transition = validate_transition(before, desired)
    if transition == "unchanged" and all(record.get("ttl") == RECORD_TTL for record in before):
        verify_public_dns(set(desired.values()))
        return {"status": "unchanged", "records": sanitized(before)}

    changed: list[dict] = []
    try:
        for record in before:
            wanted = desired[record_key(record["content"])]
            if canonical_txt(record["content"]) == wanted and record.get("ttl") == RECORD_TTL:
                continue
            cloudflare_request(
                token,
                "PATCH",
                f"/zones/{zone_id}/dns_records/{record['id']}",
                {"content": wanted, "ttl": RECORD_TTL},
            )
            changed.append(record)
        _, after = load_cloudflare_state(token, desired)
        if current_rrset(after) != set(desired.values()) or any(
                record.get("ttl") != RECORD_TTL for record in after):
            raise RuntimeError("Cloudflare post-update RRset verification failed")
        verify_public_dns(set(desired.values()))
        return {"status": "updated", "changed": len(changed), "records": sanitized(after)}
    except Exception as update_error:
        rollback_errors = []
        for record in reversed(changed):
            try:
                cloudflare_request(
                    token,
                    "PATCH",
                    f"/zones/{zone_id}/dns_records/{record['id']}",
                    {"content": record["content"], "ttl": record.get("ttl", RECORD_TTL)},
                )
            except Exception as exc:  # pragma: no cover - emergency reporting only
                rollback_errors.append(str(exc))
        if rollback_errors:
            raise RuntimeError(
                "DNS update failed and rollback was incomplete: " + "; ".join(rollback_errors)) \
                from update_error
        raise RuntimeError("DNS update failed; the original RRset was restored") from update_error


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("mode", choices=("plan", "apply"))
    parser.add_argument("--tag", help="exact stable tag; default: latest common Core/GUI stable tag")
    args = parser.parse_args()

    github_token = os.environ.get("GITHUB_TOKEN")
    tag = args.tag or latest_common_stable_tag(github_token)
    version_from_tag(tag)
    desired = desired_records(tag, github_token)
    print(json.dumps({"mode": args.mode, "release_tag": tag,
                      "desired": sorted(desired.values())}, indent=2))
    if args.mode == "plan":
        return 0

    cloudflare_token = os.environ.get("CLOUDFLARE_API_TOKEN")
    if not cloudflare_token:
        raise RuntimeError("CLOUDFLARE_API_TOKEN is unavailable")
    result = publish(cloudflare_token, desired)
    print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        raise SystemExit(1)
