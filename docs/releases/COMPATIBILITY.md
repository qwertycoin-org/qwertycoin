# Core and SDK compatibility matrix

This file is the canonical cross-repository compatibility record for the
current published release and the reviewed development source graph. Git
submodule commits are compatibility boundaries: consumers must not silently
replace them with a moving branch.

## Published release

| Component | Release/source | Compatible dependency |
| --- | --- | --- |
| Qwertycoin Core | `v2.0.2` / `54308d8473dc5606d054c0ba428cfb2d64e758c1` | QWC v2 mainnet, HF17 |
| Qwertycoin GUI | `v2.0.2` / release source | Core gitlink `54308d8473dc5606d054c0ba428cfb2d64e758c1` |

The downloadable Core and GUI packages are the supported end-user release
pair. A matching version number is not sufficient evidence: release packages
must retain their source revision, checksums, and build provenance.

## Development source graph

Verified 2026-10-01:

| Consumer | Branch revision | Required dependency pin |
| --- | --- | --- |
| Qwertycoin Core | `a71c0eb2c5b5675f9664fde5738e9cd9ba2e1eac` (`main`) | No SDK consumer implied |
| Qwertycoin GUI | `ecd1844f1b2e3416dec16c07a21e6850e11b630c` (`master`) | Core `54308d8473dc5606d054c0ba428cfb2d64e758c1` |
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

