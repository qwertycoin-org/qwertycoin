#!/usr/bin/env python3

from __future__ import annotations

import hashlib
import importlib.util
import pathlib
import sys
import unittest
from unittest import mock


MODULE_PATH = pathlib.Path(__file__).resolve().parents[1] / "update_release_dns.py"
SPEC = importlib.util.spec_from_file_location("update_release_dns", MODULE_PATH)
dns = importlib.util.module_from_spec(SPEC)
assert SPEC and SPEC.loader
sys.modules[SPEC.name] = dns
SPEC.loader.exec_module(dns)


def record(software: str, build_tag: str, version: str, digest: str, record_id: str = "id") -> dict:
    return {
        "id": record_id,
        "type": "TXT",
        "name": dns.RECORD_NAME,
        "content": f"{software}:{build_tag}:{version}:{digest}",
        "ttl": dns.RECORD_TTL,
    }


class ReleaseDnsTests(unittest.TestCase):
    def test_stable_versions_are_strict(self):
        self.assertEqual(dns.version_from_tag("v2.0.2"), "2.0.2")
        self.assertEqual(dns.version_tuple("2.10.3"), (2, 10, 3))
        for invalid in ("2.0.2", "v2.0", "v2.0.2-rc1", "v02.0.2", "v2.0.2.0"):
            with self.subTest(invalid=invalid), self.assertRaises(RuntimeError):
                dns.version_from_tag(invalid)

    def test_channels_match_updater_allowlist(self):
        channels = dns.channels_for("v2.0.2")
        self.assertEqual(len(channels), 7)
        self.assertEqual(
            {channel.key for channel in channels},
            {
                ("qwertycoin", "linux-x64"),
                ("qwertycoin", "mac-armv8"),
                ("qwertycoin", "win-x64"),
                ("qwertycoin-gui", "linux-x64"),
                ("qwertycoin-gui", "mac-armv8"),
                ("qwertycoin-gui", "install-win-x64"),
                ("qwertycoin-gui", "win-x64"),
            },
        )

    def test_sha256sums_are_exact_and_unique(self):
        digest = "a" * 64
        self.assertEqual(dns.parse_sha256sums(f"{digest}  artifact.zip\n".encode()),
                         {"artifact.zip": digest})
        for invalid in (
            b"",
            f"{digest}  ../artifact.zip\n".encode(),
            f"{digest.upper()}  artifact.zip\n".encode(),
            f"{digest}  artifact.zip\n{digest}  artifact.zip\n".encode(),
        ):
            with self.subTest(invalid=invalid), self.assertRaises(RuntimeError):
                dns.parse_sha256sums(invalid)

    def test_verified_release_requires_exact_assets_and_github_digests(self):
        tag = "v2.0.2"
        repository = dns.CORE_REPOSITORY
        filenames = dns.expected_assets(tag, repository) - {"SHA256SUMS"}
        sums = "".join(f"{'a' * 64}  {name}\n" for name in sorted(filenames)).encode()
        assets = [
            {"name": name, "state": "uploaded", "digest": "sha256:" + "a" * 64,
             "browser_download_url": f"https://github.com/{repository}/releases/download/{tag}/{name}"}
            for name in sorted(filenames)
        ]
        assets.append({
            "name": "SHA256SUMS",
            "state": "uploaded",
            "digest": "sha256:" + hashlib.sha256(sums).hexdigest(),
            "browser_download_url":
                f"https://github.com/{repository}/releases/download/{tag}/SHA256SUMS",
        })
        release = {"tag_name": tag, "draft": False, "prerelease": False, "assets": assets}
        with mock.patch.object(dns, "request_json", return_value=release), \
             mock.patch.object(dns, "request_bytes", return_value=sums):
            self.assertEqual(set(dns.verified_release_hashes(repository, tag, "token")), filenames)

        release["assets"][0]["digest"] = "sha256:" + "b" * 64
        with mock.patch.object(dns, "request_json", return_value=release), \
             mock.patch.object(dns, "request_bytes", return_value=sums), \
             self.assertRaises(RuntimeError):
            dns.verified_release_hashes(repository, tag, "token")

    def test_latest_common_release_ignores_prereleases(self):
        releases = {
            dns.CORE_REPOSITORY: {"v2.0.1", "v2.0.2", "v2.1.0"},
            dns.GUI_REPOSITORY: {"v2.0.1", "v2.0.2"},
        }
        with mock.patch.object(
                dns, "stable_release_tags", side_effect=lambda repository, _token: releases[repository]):
            self.assertEqual(dns.latest_common_stable_tag(None), "v2.0.2")

    def test_transition_is_monotonic_and_immutable(self):
        old_hash = "a" * 64
        new_hash = "b" * 64
        desired = {
            ("qwertycoin", "linux-x64"):
                f"qwertycoin:linux-x64:2.0.2:{new_hash}",
        }
        self.assertEqual(
            dns.validate_transition(
                [record("qwertycoin", "linux-x64", "2.0.1", old_hash)], desired),
            "upgrade",
        )
        self.assertEqual(
            dns.validate_transition(
                [record("qwertycoin", "linux-x64", "2.0.2", new_hash)], desired),
            "unchanged",
        )
        with self.assertRaisesRegex(RuntimeError, "changed after DNS publication"):
            dns.validate_transition(
                [record("qwertycoin", "linux-x64", "2.0.2", old_hash)], desired)
        with self.assertRaisesRegex(RuntimeError, "downgrade"):
            dns.validate_transition(
                [record("qwertycoin", "linux-x64", "2.0.3", old_hash)], desired)

    def test_public_dns_requires_both_exact_dnssec_rrsets(self):
        wanted = {"qwertycoin:linux-x64:2.0.2:" + "a" * 64}
        with mock.patch.object(dns, "doh_rrset", side_effect=[(True, wanted), (True, wanted)]):
            dns.verify_public_dns(wanted, attempts=1, delay=0)
        with mock.patch.object(dns, "doh_rrset", side_effect=[(True, wanted), (False, wanted)]), \
             self.assertRaises(RuntimeError):
            dns.verify_public_dns(wanted, attempts=1, delay=0)

    def test_publish_rolls_back_partial_rrset(self):
        old_hash = "a" * 64
        new_hash = "b" * 64
        before = [
            record("qwertycoin", "linux-x64", "2.0.1", old_hash, "one"),
            record("qwertycoin", "win-x64", "2.0.1", old_hash, "two"),
        ]
        desired = {
            ("qwertycoin", "linux-x64"):
                f"qwertycoin:linux-x64:2.0.2:{new_hash}",
            ("qwertycoin", "win-x64"):
                f"qwertycoin:win-x64:2.0.2:{new_hash}",
        }
        api = mock.Mock(side_effect=[{}, RuntimeError("second patch failed"), {}])
        with mock.patch.object(dns, "load_cloudflare_state", return_value=("zone", before)), \
             mock.patch.object(dns, "cloudflare_request", api), \
             self.assertRaisesRegex(RuntimeError, "original RRset was restored"):
            dns.publish("token", desired)
        self.assertEqual(api.call_count, 3)
        self.assertEqual(api.call_args_list[-1].args[:3],
                         ("token", "PATCH", "/zones/zone/dns_records/one"))
        self.assertEqual(api.call_args_list[-1].args[3],
                         {"content": before[0]["content"], "ttl": dns.RECORD_TTL})

    def test_publish_noop_still_verifies_public_dns(self):
        digest = "a" * 64
        desired = {
            ("qwertycoin", "linux-x64"):
                f"qwertycoin:linux-x64:2.0.2:{digest}",
        }
        before = [record("qwertycoin", "linux-x64", "2.0.2", digest)]
        with mock.patch.object(dns, "load_cloudflare_state", return_value=("zone", before)), \
             mock.patch.object(dns, "verify_public_dns") as verify, \
             mock.patch.object(dns, "cloudflare_request") as api:
            result = dns.publish("token", desired)
        self.assertEqual(result["status"], "unchanged")
        verify.assert_called_once_with(set(desired.values()))
        api.assert_not_called()


if __name__ == "__main__":
    unittest.main()
