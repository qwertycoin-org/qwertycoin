# Private macOS release signing

`tools/release/sign_macos_package.py` signs an already verified, extracted Core
or GUI package on a trusted Mac. It is an **opt-in local post-build stage**; it
does not change CI credentials, public release assets, tags, or unsigned build
workflows. Consensus code and wallet data are not changed.

## Identity and privacy gate

Developer ID signatures publicly identify their organization and Team ID. An
individual certificate cannot conceal its holder's legal name. Obtain explicit
approval for the public organization identity before creating a certificate.
Organization identity is not an anonymity guarantee; public corporate records
may link it to individuals.

Use Developer ID Application for app bundles, Mach-O libraries, command-line
programs, and DMGs. Developer ID Installer is separately needed only for PKG
installers. Never export a signing private key to a public runner or print raw
keychain inventories, private keys, API credentials, or personal build paths.

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

Record submission IDs, accepted status, staple validation, Gatekeeper checks and
final SHA-256 hashes as separate evidence. Test the actual mounted/copied app,
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
