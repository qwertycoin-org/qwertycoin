# Private macOS release signing

`tools/release/sign_macos_package.py` signs an already verified, extracted Core
or GUI package on a trusted Mac. The opt-in `macos-signing.yml` workflow automates
this post-build stage on fresh GitHub-hosted macOS runners. It does not change
public release assets, tags, or unsigned build workflows. Consensus code and
wallet data are not changed.

## Identity and privacy gate

Developer ID signatures publicly identify their organization and Team ID. An
individual certificate cannot conceal its holder's legal name. Obtain explicit
approval for the public organization identity before creating a certificate.
Organization identity is not an anonymity guarantee; public corporate records
may link it to individuals.

Use Developer ID Application for app bundles, Mach-O libraries, command-line
programs, and DMGs. Developer ID Installer is separately needed only for PKG
installers. Use a separate CI signing key, provided only to an approved ephemeral signing
job through environment secrets. Never place a private key in source control,
artifacts or logs, or print raw keychain inventories, API credentials, or personal
build paths.

The helper checks the certificate subject against the exact approved organization
and team, rejects extra subject fields, selects the matching usable keychain
identity by certificate fingerprint, and verifies that every signed binary embeds
that exact certificate. It requires a caller-supplied **private JSON array** of
personal-name components, emails and build paths to scan in file names and bytes.
Keep this file outside the repository. The scan detects known UTF-8/ASCII and UTF-16 strings;
it is not a general proof that all possible personal data is absent. Review
certificate metadata, final packaging, provenance and logs separately.

## Inputs and invocation

1. Pin source revisions and download approved release archives and checksums.
2. Verify the archive SHA-256 and safely extract to a private staging directory.
   Never use an installed wallet app or a directory containing wallets, keys,
   blockchain data, or local settings as an input.
3. Inspect the original package's build identity and dependency closure using
   the repository's existing runtime verifier. Keep original archives unchanged.
4. Install the approved certificate and its matching key in the signing Mac's
   keychain. Store notarization credentials with `notarytool store-credentials`
   and validate them; credentials must not be command literals in committed files.
5. Sign to a new output directory (the helper refuses overwrites):

```sh
python3 tools/release/sign_macos_package.py \
  --input /private/staging/verified-package \
  --output /private/staging/signed-package \
  --certificate /private/signing/developer-id-application.cer \
  --organization 'Approved Organization LLC' \
  --team-id ABCDEFGHIJ \
  --privacy-token-file /private/signing/privacy-tokens.json
```

All original Mach-O signatures must verify before copying. Signatures are applied
inside out; `--deep` is used only for verification, never signing. Hardened runtime
and secure timestamps are mandatory. QWC executables receive only `allow-jit`
for RandomX and Qt QML; library validation and executable-memory protections are
not disabled. A signing failure leaves an incomplete private output for inspection,
without a success receipt. Use a new output directory for a corrected retry.

`SIGNING-INFO.json` is written only after signature and privacy gates pass. It
records **not-submitted** for notarization. It supplements the original build
information; signing does not retroactively change the original source/build.

## Notarization and publication are separate gates

Before submission, exercise signed Core tools, the GUI/QML startup and a disposable
RandomX test, not production wallets or nodes. Verify required runtime behavior
under hardened runtime; do not add broad exceptions to silence a failure.

Submit a ZIP, DMG, or PKG to Apple, not a tarball. For a GUI, submit the signed app
in a ZIP first, staple the accepted app, then use the existing GUI DMG packager.
Sign and notarize the final DMG and staple it. Core/CLI tools can be distributed
in a signed/notarized/stapled DMG; raw CLI tools and tarballs cannot carry a
stapled ticket. Verify standalone binaries with `codesign --verify --strict -R '=notarized'`.
Use `spctl` for app bundles and signed containers, not its app-only execute
assessment for raw CLI executables.

```sh
xcrun notarytool submit candidate.zip --keychain-profile PROFILE --output-format json
# Retain the returned submission ID; never resubmit to poll an existing job.
xcrun notarytool info SUBMISSION_ID --keychain-profile PROFILE --output-format json
xcrun notarytool log SUBMISSION_ID --keychain-profile PROFILE private-notary-log.json
# Only after status Accepted, for supported containers:
xcrun stapler staple candidate.dmg
xcrun stapler validate candidate.dmg
spctl --assess --type open --context context:primary-signature candidate.dmg
```

Retain submission IDs and raw Apple logs privately. Public evidence contains
only source provenance, acceptance status, verification results and final SHA-256
hashes, without signer names, team IDs, certificate fingerprints or account IDs. Test the actual mounted/copied app,
not only the prepackaged directory. Update release notes and metadata to describe
the actual signing state. Regenerate all final checksums after stapling. Do not
silently replace existing stable assets or move release tags; follow the normal
reviewed release/publication process.

## Verification

```sh
python3 -m unittest discover -s tools/release/tests -p test_sign_macos_package.py -v
```

These tests cover privacy rejection, unrelated-word false positives, symlink
containment, extra/personal certificate fields, LibreSSL formatting and exact
identity pinning. Native signing, execution, notarization and Gatekeeper checks
are required in addition; unit tests cannot establish those outcomes.

## Protected GitHub Actions workflow

The Core and GUI repositories carry identical copies of `macos_ci.py`,
`verify_macos_ci.py`, `sign_macos_package.py` and their signing tests. Keep those
copies synchronized; each workflow uses only its own reviewed default-branch
checkout, never a mutable cross-repository script download.

### Environment configuration

Create `macos-signing` with a required release-maintainer reviewer, no administrator
bypass, and a custom **branch-only** deployment policy for `main` (Core) or `master`
(GUI). Do not allow tags, pull-request refs, forks or arbitrary feature branches.
Reviewers must inspect the exact workflow/source revisions before approval.

Create `macos-verification` with the same branch-only policy. It contains only
verification policy, never a private signing or notarization key. It can run
without another approval after the signing job succeeds.

Set these environment secrets, without committing their actual values:

| Secret | Signing | Verification |
| --- | --- | --- |
| `APPLE_SIGNING_CERT_DER` (base64 DER leaf certificate) | Yes | Yes |
| `APPLE_SIGNING_CERT_SHA256` (lowercase hex DER digest) | Yes | Yes |
| `APPLE_SIGNING_ORGANIZATION` (exact approved subject organization) | Yes | Yes |
| `APPLE_SIGNING_TEAM_ID` (approved team) | Yes | Yes |
| `PRIVATE_IDENTITY_TOKENS` (private JSON array, not base64) | Yes | Yes |
| `APPLE_SIGNING_CERT_P12` (base64 password-protected P12) | Yes | **No** |
| `APPLE_SIGNING_CERT_PASSWORD` | Yes | **No** |
| `APPLE_NOTARY_KEY_P8` (base64 App Store Connect API key) | Yes | **No** |
| `APPLE_NOTARY_KEY_ID` | Yes | **No** |
| `APPLE_NOTARY_ISSUER_ID` | Yes | **No** |

Certificate identity is necessarily readable from Apple signatures. Do not repeat
real organizations, account emails, team IDs or certificate metadata in repository
files, examples, PR descriptions, logs or standalone reports. Native diagnostics
are captured and withheld, not redacted after being uploaded. Only reviewed,
allowlisted DMGs and identity-neutral JSON/checksum reports are uploaded.

### Run sequence

1. Merge the reviewed signing workflow through the normal repository process.
   The protected environments intentionally do **not** permit a feature-branch
   test. Default-branch registration is required before dispatch.
2. Use the existing `release.yml` workflow to build the exact approved source
   revision for macOS. Keep its successful, first-attempt run ID. The source must
   be an ancestor of the build-workflow revision, which must be an ancestor of
   the signing-workflow revision. The build workflow must match the reviewed one.
   Use a new build if artifacts have expired or the recipe changed.
3. Dispatch `macos-signing.yml` on the default branch with `release_tag`,
   `expected_revision` and `source_run_id`. The artifact name must match the
   normal `qwertycoin[-gui]-vX.Y.Z-macos-arm64` format.
4. Preflight checks repository/event/branch/job success, source ancestry, workflow
   recipe and artifact identity. Approve the `macos-signing` environment in GitHub.
5. The signing job downloads the exact artifact ID, **fails** on ZIP digest or
   member-set mismatches, safely validates archive contents and source metadata,
   then imports the key into a temporary keychain. No candidate program runs in
   this job. For GUI it notarizes/staples the app before packaging the DMG; both
   kinds then sign, notarize and staple the final DMG. Cleanup restores the
   keychain search list and deletes temporary credentials even on failure.
6. A separate fresh runner verifies the exact signed artifact digest, embedded
   certificate pin, Developer ID chain, secure timestamps, hardened runtime,
   entitlements, stapled tickets, Gatekeeper, mounted/copied payload, private-token
   scans including extended attributes, program versions, GUI/QML (GUI only), and
   two disposable HF17/RandomX blocks. No existing wallets or chain data are used.
7. Only after those checks pass, download `verified-signed-<package-name>`.
   It contains the DMG, final `SHA256SUMS`, `SIGNING-EVIDENCE.json` and
   `VERIFICATION.json`. Never publish `unverified-signed-*` intermediate artifacts.

This workflow deliberately has **no publication permission**. Existing
multi-platform release assembly gates remain unchanged; their tarball/legacy DMG
artifacts are not silently replaced by this workflow. Promote the verified signed
DMG through a separately reviewed release-asset/publication operation, updating
checksums and release notes. No existing release or tag is modified automatically.
The portable tarball remains a build candidate, not a stapled distribution format.

If notarization times out, do not repeatedly submit the same candidate. Inspect
Apple's submission history privately before retrying. Do not upload raw diagnostics
to a public issue to debug a failure. Rotate the CI certificate/API key by updating
both environments' policy values together and repeating native and hosted gates.

### CI verification

```sh
python3 -m unittest discover -s tools/release/tests -p 'test_*macos*.py' -v
```

Tests cover untrusted callers/builds, mutable revisions, reruns, artifact digest
mismatches, archive traversal/duplicates/symlinks, secret separation, hosted-runner
requirements, suppressed private diagnostics and temporary-keychain cleanup.
A successful native integration test is not a successful hosted workflow run;
report both separately. Public-runner verification is required before promotion.
