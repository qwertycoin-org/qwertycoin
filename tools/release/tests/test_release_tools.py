#!/usr/bin/env python3

from importlib.util import module_from_spec, spec_from_file_location
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
import tarfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


def load(name: str):
    spec = spec_from_file_location(name, ROOT / f"{name}.py")
    assert spec and spec.loader
    module = module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


VERIFY = load("verify_candidate_archive")
VALIDATE = load("validate_release_request")
SMOKE = load("smoke_core")
DOCKER_CONTEXT = load("prepare_docker_context")
DOCKER_RELEASE = load("inspect_docker_release")


class ArchiveSecurityTests(unittest.TestCase):
    def assert_rejected(self, callback) -> None:
        with self.assertRaises(SystemExit):
            callback()

    def test_rejects_absolute_traversal_and_backslash_paths(self) -> None:
        for value in ("/payload", "payload/../escape", "payload\\escape"):
            with self.subTest(value=value):
                self.assert_rejected(lambda value=value: VERIFY.normalize_member(value))

    def test_rejects_casefold_collision_and_outside_members(self) -> None:
        self.assert_rejected(lambda: VERIFY.validate_names(["release/File", "release/file"], "release"))
        self.assert_rejected(lambda: VERIFY.validate_names(["other/file"], "release"))

    def test_accepts_text_and_binary_sha256_formats(self) -> None:
        digest = "a" * 64
        self.assertIsNotNone(VERIFY.SHA256_LINE.fullmatch(f"{digest}  release/file"))
        self.assertIsNotNone(VERIFY.SHA256_LINE.fullmatch(f"{digest} *release/file"))

    def test_rejects_missing_required_payload(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary) / "release"
            root.mkdir()
            self.assert_rejected(lambda: VERIFY.require_payload(root, "linux"))

    def test_manifest_requires_complete_coverage_and_exact_digests(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            destination = Path(temporary)
            root = destination / "release"
            root.mkdir()
            payload = root / "payload"
            payload.write_bytes(b"candidate")
            manifest = destination / "release.sha256"
            manifest.write_text(
                f"{hashlib.sha256(payload.read_bytes()).hexdigest()}  release/payload\n",
                encoding="utf-8",
            )
            self.assertEqual(VERIFY.verify_manifest(destination, "release"), 1)
            (root / "unhashed").write_bytes(b"unexpected")
            self.assert_rejected(lambda: VERIFY.verify_manifest(destination, "release"))
            (root / "unhashed").unlink()
            payload.write_bytes(b"changed")
            self.assert_rejected(lambda: VERIFY.verify_manifest(destination, "release"))

    def test_metadata_rejects_conflicting_provenance_and_negative_stable_gate(self) -> None:
        expected_revision = "1" * 40
        workflow_revision = "2" * 40
        args = argparse.Namespace(
            expected_tag="v2.0.0-rc1",
            expected_revision=expected_revision,
            expected_workflow_name="qwc/core-release-candidate",
            expected_workflow_revision=workflow_revision,
            expected_run_id="42",
            expected_run_attempt="1",
            expected_os="Linux",
            expected_arch="x86_64",
            require_stable_gate=False,
        )
        manifest = {
            "network": {"type": "mainnet", "network_id": "ab", "genesis_hash": "cd"},
            "commitments": {"parameter_set_sha256": "ef"},
            "epoch": {"length_blocks": 720},
            "committee": {"size": 9, "threshold": 6, "round_offsets": [0, 200, 400], "rounds_required": 2},
            "admission": {"leading_zero_bits": 18, "lease_epochs": 1},
            "reward": {
                "basis_points": 1000,
                "emission_accounting": "actual-issued-subsidy",
                "fee_policy": "subsidy-only",
                "empty_set_policy": "miner-fallback",
            },
        }
        canonical_manifest = (
            json.dumps(manifest, sort_keys=True, separators=(",", ":"), ensure_ascii=False) + "\n"
        ).encode()
        gate = {
            "candidate_source_revision": expected_revision,
            "stable_permitted": False,
            "manifest_sha256": hashlib.sha256(canonical_manifest).hexdigest(),
        }
        evidence = {
            "programs": {
                "qwertycoind": {"help": "pass", "version": "pass"},
                "qwertycoin-wallet-cli": {"help": "pass", "version": "pass"},
                "qwertycoin-wallet-rpc": {"help": "pass", "version": "pass"},
            },
            "isolated_network": {
                "daemon_rpc_ready": "pass",
                "network": "offline-mainnet",
                "genesis_hash_match": "pass",
                "epose_protocol_version": 2,
                "epose_service_reward_bps": 1000,
                "daemon_rpc_stop_ack": "pass",
                "daemon_clean_exit": "pass",
                "daemon_stop_method": "rpc",
                "wallet_rpc_digest_auth": "pass",
                "wallet_seed_restore_address_match": "pass",
                "secrets_logged": False,
            }
        }
        info = {
            "release": {
                "version": "2.0.0",
                "compatible_version": "2.0.0.0",
                "tag": args.expected_tag,
                "source_revision": expected_revision,
            },
            "workflow": {
                "name": args.expected_workflow_name,
                "revision": workflow_revision,
                "run_id": args.expected_run_id,
                "run_attempt": args.expected_run_attempt,
            },
            "platform": {"os": args.expected_os, "architecture": args.expected_arch},
            "build": {
                "targets": ["daemon", "simplewallet", "wallet_rpc_server"],
                "use_device_trezor": False,
                "type": "Release",
            },
            "submodules": {"external/randomx": "3" * 40},
            "network": {
                "type": "mainnet",
                "network_id": "ab",
                "genesis_hash": "cd",
                "parameter_set_sha256": "ef",
                "epoch_length_blocks": 720,
                "committee_size": 9,
                "committee_threshold": 6,
                "committee_round_offsets": [0, 200, 400],
                "committee_rounds_required": 2,
                "admission_leading_zero_bits": 18,
                "admission_lease_epochs": 1,
                "service_reward_basis_points": 1000,
                "service_reward_emission_accounting": "actual-issued-subsidy",
                "service_reward_fee_policy": "subsidy-only",
                "service_reward_empty_set_policy": "miner-fallback",
            },
            "epose_release_gate": gate,
        }
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            evidence_dir = root / "evidence"
            evidence_dir.mkdir()
            (evidence_dir / "EPOSE-RELEASE-GATE.json").write_text(
                json.dumps(gate), encoding="utf-8"
            )
            (evidence_dir / "PARAMETER-MANIFEST-V2.json").write_text(
                json.dumps(manifest), encoding="utf-8"
            )
            (evidence_dir / "RELEASE-EVIDENCE.json").write_text(
                json.dumps(evidence), encoding="utf-8"
            )
            VERIFY.verify_metadata(info, args, root)
            info["workflow"]["run_id"] = "43"
            self.assert_rejected(lambda: VERIFY.verify_metadata(info, args, root))
            info["workflow"]["run_id"] = args.expected_run_id
            args.require_stable_gate = True
            self.assert_rejected(lambda: VERIFY.verify_metadata(info, args, root))


class RequestValidationTests(unittest.TestCase):
    def test_docker_context_contains_only_runtime_payload(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            package = root / "package"
            package.mkdir()
            for program in DOCKER_CONTEXT.PROGRAMS:
                path = package / program
                path.write_bytes(b"binary")
                path.chmod(0o755)
            for document in DOCKER_CONTEXT.DOCUMENTS:
                (package / document).write_text("notice\n", encoding="utf-8")
            (package / "lib").mkdir()
            (package / "lib/libexample.so").write_bytes(b"library")
            (package / "BUILD-INFO.json").write_text("{}\n", encoding="utf-8")
            dockerfile = root / "Dockerfile"
            dockerfile.write_text("FROM scratch\n", encoding="utf-8")
            entrypoint = root / "entrypoint.sh"
            entrypoint.write_text("#!/bin/sh\n", encoding="utf-8")
            entrypoint.chmod(0o755)
            output = root / "context"

            manifest = DOCKER_CONTEXT.prepare(package, dockerfile, entrypoint, output)

            paths = {item["path"] for item in manifest["files"]}
            self.assertIn("release/qwertycoind", paths)
            self.assertIn("release/lib/libexample.so", paths)
            self.assertNotIn("release/BUILD-INFO.json", paths)
            self.assertTrue((output / "CONTEXT-MANIFEST.json").is_file())
            self.assertGreater(DOCKER_CONTEXT.verify_context(output), 0)

            (output / "release/qwertycoind").write_bytes(b"changed")
            with self.assertRaises(SystemExit):
                DOCKER_CONTEXT.verify_context(output)

    def test_docker_context_verification_rejects_injected_symlink(self) -> None:
        if os.name == "nt":
            self.skipTest("ordinary Windows test users cannot always create symlinks")
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            package = root / "package"
            package.mkdir()
            for program in DOCKER_CONTEXT.PROGRAMS:
                path = package / program
                path.write_bytes(b"binary")
                path.chmod(0o755)
            for document in DOCKER_CONTEXT.DOCUMENTS:
                (package / document).write_text("notice\n", encoding="utf-8")
            (package / "lib").mkdir()
            (package / "lib/libexample.so").write_bytes(b"library")
            dockerfile = root / "Dockerfile"
            dockerfile.write_text("FROM scratch\n", encoding="utf-8")
            entrypoint = root / "entrypoint.sh"
            entrypoint.write_text("#!/bin/sh\n", encoding="utf-8")
            entrypoint.chmod(0o755)
            output = root / "context"
            DOCKER_CONTEXT.prepare(package, dockerfile, entrypoint, output)
            (output / "injected-link").symlink_to("release/qwertycoind")

            with self.assertRaises(SystemExit):
                DOCKER_CONTEXT.verify_context(output)

    def test_docker_context_rejects_symlinked_library(self) -> None:
        if os.name == "nt":
            self.skipTest("ordinary Windows test users cannot always create symlinks")
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            library = root / "lib"
            library.mkdir()
            target = root / "target"
            target.write_bytes(b"library")
            (library / "libbad.so").symlink_to(target)
            with self.assertRaises(SystemExit):
                DOCKER_CONTEXT.validated_library_files(library)

    def test_docker_release_inspection_binds_checksum_and_metadata(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            tag = "v2.0.1-rc1"
            revision = "1" * 40
            root_name = f"qwertycoin-{tag}-linux-x86_64"
            package = root / root_name
            package.mkdir()
            info = {
                "release": {"tag": tag, "version": "2.0.1", "source_revision": revision},
                "workflow": {
                    "name": "qwc/core-release-candidate",
                    "revision": "2" * 40,
                    "run_id": "42",
                    "run_attempt": "1",
                },
                "platform": {"os": "Linux", "architecture": "x86_64"},
            }
            (package / "BUILD-INFO.json").write_text(json.dumps(info), encoding="utf-8")
            archive = root / f"{root_name}.tar.gz"
            with tarfile.open(archive, "w:gz") as output:
                output.add(package, arcname=root_name)
            assets = {
                archive.name: hashlib.sha256(archive.read_bytes()).hexdigest(),
                f"qwertycoin-{tag}-macos-arm64.tar.gz": "a" * 64,
                f"qwertycoin-{tag}-windows-x86_64.zip": "b" * 64,
            }
            checksums = root / "SHA256SUMS"
            checksums.write_text(
                "".join(f"{digest}  {name}\n" for name, digest in sorted(assets.items())),
                encoding="utf-8",
            )
            result = DOCKER_RELEASE.inspect(archive, checksums, tag, revision)
            self.assertEqual(result["full_version"], "2.0.1-rc1")
            self.assertTrue(result["prerelease"])

            checksums.write_text(checksums.read_text(encoding="utf-8").replace(
                assets[archive.name], "0" * 64
            ), encoding="utf-8")
            with self.assertRaises(SystemExit):
                DOCKER_RELEASE.inspect(archive, checksums, tag, revision)

    def test_macos_bundler_resolves_loader_siblings_from_configured_paths(self) -> None:
        bundler = (ROOT / "bundle_macos_runtime.sh").read_text(encoding="utf-8")
        self.assertIn(
            'resolve_from_configured_paths "${dependency#@loader_path/}"', bundler
        )
        self.assertIn('resolve_from_configured_paths "$suffix"', bundler)

    def test_release_workflow_avoids_empty_arrays_under_macos_bash_3(self) -> None:
        workflow = (ROOT.parents[1] / ".github/workflows/release.yml").read_text(
            encoding="utf-8"
        )
        self.assertNotIn("gate_args", workflow)
        self.assertEqual(workflow.count("--require-ready"), 4)
        self.assertIn("options: [require-ready, public-test]", workflow)
        self.assertEqual(workflow.count("STABLE_GATE_POLICY"), 11)

    def test_docker_publish_is_called_after_release_verification(self) -> None:
        workflow = (ROOT.parents[1] / ".github/workflows/assemble-release.yml").read_text(
            encoding="utf-8"
        )
        self.assertIn("publish-docker:", workflow)
        self.assertIn("needs: assemble", workflow)
        self.assertIn("uses: ./.github/workflows/docker-publish.yml", workflow)
        self.assertIn("if: inputs.release_kind != 'draft'", workflow)

    def test_docker_runtime_keeps_mainnet_default_and_exec_dispatch(self) -> None:
        repository = ROOT.parents[1]
        entrypoint = (repository / "docker/entrypoint.sh").read_text(encoding="utf-8")
        dockerfile = (repository / "docker/Dockerfile").read_text(encoding="utf-8")
        self.assertNotIn("--testnet", entrypoint)
        self.assertNotIn("--testnet", dockerfile)
        self.assertNotIn("eval", entrypoint)
        self.assertEqual(entrypoint.count("exec /opt/qwertycoin/"), 4)
        self.assertIn('USER 10001:10001', dockerfile)
        self.assertIn('STOPSIGNAL SIGINT', dockerfile)

    def test_assemble_public_test_override_is_explicit_and_preserves_gate(self) -> None:
        workflow = (
            ROOT.parents[1] / ".github/workflows/assemble-release.yml"
        ).read_text(encoding="utf-8")
        self.assertIn("options: [require-ready, public-test]", workflow)
        self.assertIn("PUBLISH-STABLE-PUBLIC-TEST", workflow)
        self.assertIn(
            '"$RELEASE_KIND" == stable && "$STABLE_GATE_POLICY" == require-ready',
            workflow,
        )
        self.assertIn("EPoSE evidence gate below remains", workflow)

    def test_smoke_evidence_uses_platform_neutral_program_names(self) -> None:
        self.assertEqual(SMOKE.evidence_program_name(Path("qwertycoind")), "qwertycoind")
        self.assertEqual(
            SMOKE.evidence_program_name(Path("qwertycoin-wallet-cli.exe")),
            "qwertycoin-wallet-cli",
        )

    def test_version_parser_matches_current_format(self) -> None:
        text = '#define DEF_QWERTYCOIN_VERSION "2.0.0"\n'
        self.assertEqual(VALIDATE.VERSION_RE.search(text).groups(), ("2", "0", "0"))

    def test_tag_parser_rejects_zero_and_missing_rc_numbers(self) -> None:
        self.assertIsNotNone(VALIDATE.TAG_RE.fullmatch("v2.0.0"))
        self.assertIsNotNone(VALIDATE.TAG_RE.fullmatch("v2.0.0-rc1"))
        self.assertIsNone(VALIDATE.TAG_RE.fullmatch("v2.0.0-rc0"))
        self.assertIsNone(VALIDATE.TAG_RE.fullmatch("v2.0.0-rc"))

    def test_local_release_tag_needs_no_runner_identity_and_is_deterministic(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            repository = root / "source"
            origin = root / "origin.git"
            repository.mkdir()
            subprocess.run(["git", "init", "--quiet"], cwd=repository, check=True)
            subprocess.run(["git", "init", "--quiet", "--bare", str(origin)], check=True)
            subprocess.run(
                ["git", "remote", "add", "origin", str(origin)], cwd=repository, check=True
            )

            env = os.environ.copy()
            env.update(
                {
                    "GIT_AUTHOR_NAME": "Candidate Author",
                    "GIT_AUTHOR_EMAIL": "candidate@example.invalid",
                    "GIT_AUTHOR_DATE": "2026-09-13T00:00:00Z",
                    "GIT_COMMITTER_NAME": "Candidate Author",
                    "GIT_COMMITTER_EMAIL": "candidate@example.invalid",
                    "GIT_COMMITTER_DATE": "2026-09-13T00:00:00Z",
                }
            )
            subprocess.run(
                ["git", "commit", "--quiet", "--allow-empty", "-m", "candidate"],
                cwd=repository,
                env=env,
                check=True,
            )
            revision = subprocess.check_output(
                ["git", "rev-parse", "HEAD"], cwd=repository, text=True
            ).strip()

            runner_env = os.environ.copy()
            runner_env.update(
                {
                    "HOME": str(root / "empty-home"),
                    "XDG_CONFIG_HOME": str(root / "empty-config"),
                    "GIT_CONFIG_NOSYSTEM": "1",
                }
            )
            (root / "empty-home").mkdir()
            (root / "empty-config").mkdir()
            command = [str(ROOT / "prepare_source_tag.sh"), "v2.0.0-rc1", revision]
            subprocess.run(command, cwd=repository, env=runner_env, check=True)
            first_tag = subprocess.check_output(
                ["git", "rev-parse", "refs/tags/v2.0.0-rc1"], cwd=repository, text=True
            ).strip()
            tagger = subprocess.check_output(
                [
                    "git",
                    "for-each-ref",
                    "--format=%(taggername)|%(taggeremail)",
                    "refs/tags/v2.0.0-rc1",
                ],
                cwd=repository,
                text=True,
            ).strip()
            self.assertEqual(tagger, "Qwertycoin Release Automation|<release@qwertycoin.org>")

            subprocess.run(["git", "tag", "-d", "v2.0.0-rc1"], cwd=repository, check=True)
            subprocess.run(command, cwd=repository, env=runner_env, check=True)
            second_tag = subprocess.check_output(
                ["git", "rev-parse", "refs/tags/v2.0.0-rc1"], cwd=repository, text=True
            ).strip()
            self.assertEqual(first_tag, second_tag)


if __name__ == "__main__":
    unittest.main()
