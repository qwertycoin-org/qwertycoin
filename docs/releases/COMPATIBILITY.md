# Core and SDK compatibility matrix

This file is the canonical cross-repository compatibility record for the
current published release and the reviewed development source graph. Git
submodule commits are compatibility boundaries: consumers must not silently
replace them with a moving branch.

## Core version identifiers

`src/version.cpp.in` defines the product version and release name once, using
`QWERTYCOIN_*` names. `QWERTYCOIN_VERSION` is the three-component product
version; `QWERTYCOIN_VERSION_FULL` appends the generated source tag. The tag and
release flag are exposed as `QWERTYCOIN_VERSION_TAG` and
`QWERTYCOIN_VERSION_IS_RELEASE`.

`QWERTYCOIN_COMPATIBLE_VERSION` derives the existing four-component format by
appending `.0` to the product version. Its `_FULL` form appends the same source
tag. Ledger/Trezor version payloads and the legacy ZMQ info response retain
their previous four-component bytes; protocol field names, RPC schemas and
release status semantics are unchanged. This is not a version bump, hard fork
or network migration.

`version.h` retains the five old `MONERO_*` names as source-only macro aliases
to the corresponding Qwertycoin identifiers. Downstream source projects can
rebuild unchanged, but these aliases are not old link-time symbols: consumers
must rebuild against the matching headers and libraries when updating the
Core pin. New Core code uses only the Qwertycoin names. The release metadata
writer derives the compatible version from the canonical definition and also
accepts the previous literal format when building older immutable source pins.
Historical upstream copyright notices and hardware protocol names are not
branding identifiers and remain intact. No Explorer changes are included.

## Published release

| Component | Release/source | Compatible dependency |
| --- | --- | --- |
| Qwertycoin Core | `v2.0.3` / `766839bb49475200977f6302b60c01e34d66e253` | QWC v2 mainnet, HF17 |
| Qwertycoin GUI | `v2.0.3` / `f45788f2589404de2baa50612d1f9833b731e16b` | Core gitlink `766839bb49475200977f6302b60c01e34d66e253` |

The downloadable Core and GUI packages are the supported end-user release
pair. A matching version number is not sufficient evidence: release packages
must retain their source revision, checksums, and build provenance.

## Development source graph

Verified 2026-10-03:

| Consumer | Branch revision | Required dependency pin |
| --- | --- | --- |
| Qwertycoin Core | `766839bb49475200977f6302b60c01e34d66e253` (`main`) | No SDK consumer implied |
| Qwertycoin GUI | `f45788f2589404de2baa50612d1f9833b731e16b` (`master`) | Core `766839bb49475200977f6302b60c01e34d66e253` |
| `qwertycoin-cpp` | `81ba6d82c4e4de7ed820b61df778653ab0304bd3` (`master`) | Core `cd6ce02da439cd54eafbda68d952f9d88c7fd994` |
| `qwertycoin-ts` | `5771d407315b941e456770f0dc6d7d8ee47300e9` (`master`) | C++ bridge `46179a320807fd5f90541b3c6e61b654b07e963a`, transitively Core `cd6ce02da439cd54eafbda68d952f9d88c7fd994` |

These development rows describe reviewed source pins, not a claim that all
four repositories form one published release. In particular, the SDK/WASM
source graph can intentionally lead the downloadable Core/GUI pair.

## Update rule

When a dependency pin changes:

1. update the consumer gitlink in a dedicated reviewable commit;
2. run the consumer's required build and contract tests against that exact pin;
3. update this matrix and the consumer's compatibility note in the same PR;
4. for a release, bind packages, checksums, and provenance to immutable source
   revisions; and
5. never infer compatibility from a branch name or version string alone.
