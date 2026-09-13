# Native Core release process

This document describes the manual, fail-closed process for producing portable Qwertycoin Core packages. It does not authorize a consensus, genesis, reward, network-identity or EPoSE parameter change.

## Safety and release boundary

Only `.github/workflows/release.yml` and `.github/workflows/assemble-release.yml` are active release workflows. Both use `workflow_dispatch`; pushes, pull requests and tags do not start the native matrix. The older build, Depends, EPoSE and Gitian templates remain disabled under `.github/workflows-disabled/`.

Every candidate is bound to:

- one full 40-character Core source revision;
- the recursive submodule Gitlinks from that revision;
- the requested version tag and a consistent local tag state used by `cmake/GitVersion.cmake`;
- the trusted workflow revision and Actions run/attempt;
- the current network, genesis, parameter-manifest and EPoSE release-gate identity.

The workflow source and the built source are separate checkouts. `run.head_sha` identifies the trusted workflow revision; `BUILD-INFO.json` identifies the source revision actually built. The assemble workflow validates both.

The authoritative evaluator is `tests/epose/release_gate_v2.py`. Stable candidate builds and stable publication require `overall_status=ready` **and** a manifest source revision equal to the requested Core revision. A no-go or stale candidate binding blocks stable publication. RC candidates and private drafts may preserve a no-go result for review, but must not claim stable readiness.

## Candidate build

From the Actions page, select **qwc/core-release-candidate** and provide:

- `release_tag`: `vMAJOR.MINOR.PATCH` or `vMAJOR.MINOR.PATCH-rcN`;
- `expected_revision`: the exact lowercase Core commit SHA;
- `target`: `linux`, `macos`, `windows`, or `all`.

Use one platform at a time for the first proof of a new workflow or toolchain. If one platform fails, keep successful candidates and rerun only the failed platform after a reviewable fix. Existing candidates may be reused only when source revision, tag, release workflow contract and packaged gate evidence remain identical.

The native jobs build only:

```text
daemon
simplewallet
wallet_rpc_server
```

They produce `qwertycoind`, `qwertycoin-wallet-cli` and `qwertycoin-wallet-rpc`. Trezor support and readline are disabled for this first portable matrix. No GUI, Qt, QML or additional Core utility is included.

Each uploaded Actions artifact contains an archive and its archive checksum. Inside the archive, an independent manifest hashes every regular file. `BUILD-INFO.json` records source, submodules, tag, workflow, toolchain, build options, compatibility, signing, network identity and test evidence.

## Candidate checks

All platforms run the same smoke once on the staged portable directory and again after safely extracting the final archive. It invokes `--version` and `--help`, starts an offline mainnet daemon with a temporary data directory and loopback-only ports, verifies its genesis hash and EPoSE v2 reward baseline against `BUILD-INFO.json`, creates a disposable wallet through digest-authenticated wallet RPC, restores it from its mnemonic in memory, compares the primary address and stops cleanly. The daemon never connects to a peer. Test credentials live only in a mode-restricted temporary config file; seeds, passwords and private keys are never printed or uploaded.

Linux additionally builds and runs the focused EPoSE C++ test target and the standard-library manifest/gate/reference/security tests. This candidate-bound Linux evidence is required during assembly.

Compatibility contracts:

- Linux: Ubuntu 22.04 x86_64 build, no `-march=native`, GLIBC at most 2.35 and GLIBCXX at most 3.4.30; non-system ELF dependencies are bundled with relative RPATHs.
- macOS: native `macos-15` ARM64 build, maximum deployment requirement 15.0; non-system dylibs use package-relative install names/RPATHs and are ad-hoc signed after modification.
- Windows: native Windows 2025/MSYS2 MINGW64 x86_64 build; all direct and transitive non-system DLL imports must resolve from the package. This proves the tested runner environment, not every older Windows release.

## Assemble and publish

After independently reviewing all three candidates, select **qwc/core-assemble-release** and provide the exact tag, source revision, three successful candidate run IDs and one release kind:

- `draft` with confirmation `CREATE-DRAFT`;
- `prerelease` with confirmation `PUBLISH-PRERELEASE`;
- `stable` with gate policy `require-ready` and confirmation `PUBLISH-STABLE`;
- an explicitly authorized public-test client release with `stable`, gate policy
  `public-test` and confirmation `PUBLISH-STABLE-PUBLIC-TEST`.

The `public-test` policy does not rewrite or satisfy EPoSE evidence. It preserves the
evaluated gate status in every archive and in the release notes while allowing a
versioned client compatibility release when the network identity changes. It is not
an EPoSE audit or activation-readiness claim.

Assembly compiles nothing. It verifies the repository, trusted workflow, job result, run attempt, unique unexpired artifact, external checksum, safe archive structure, complete inner manifest, exact metadata, required files and EPoSE gate. It generates `SHA256SUMS` from the final archive bytes.

New releases are first created as private drafts. An interrupted matching draft can be completed only after every existing asset is downloaded and proven byte-identical. Unexpected assets, a moved tag, a different source revision, mismatched metadata or a public partial release are conflicts. No asset is blindly overwritten and no tag is moved.

After upload, the workflow reads the release by immutable release ID, downloads all assets again, verifies `SHA256SUMS`, and for published releases resolves the public tag back to the exact source revision. Only then is the assemble run successful.

## Signing boundary

Without configured publisher certificates:

- Linux and Windows archives are unsigned;
- macOS binaries are ad-hoc signed, not Developer ID signed or notarized;
- SHA-256 checksums provide integrity, not publisher identity.

Future publisher signing belongs before final archive creation and hashing. Credentials must use the protected GitHub secret store and must never appear in logs, command arguments, URLs or repository files.
