#pragma once

extern const char* const QWERTYCOIN_VERSION;
extern const char* const QWERTYCOIN_RELEASE_NAME;
extern const char* const QWERTYCOIN_VERSION_TAG;
extern const char* const QWERTYCOIN_VERSION_FULL;
extern const bool QWERTYCOIN_VERSION_IS_RELEASE;
extern const char* const QWERTYCOIN_COMPATIBLE_VERSION;
extern const char* const QWERTYCOIN_COMPATIBLE_VERSION_FULL;

// Legacy source aliases for downstream projects rebuilding against this header.
// They do not define a second version or preserve the old binary symbol names.
#define MONERO_VERSION_TAG QWERTYCOIN_VERSION_TAG
#define MONERO_VERSION QWERTYCOIN_COMPATIBLE_VERSION
#define MONERO_RELEASE_NAME QWERTYCOIN_RELEASE_NAME
#define MONERO_VERSION_FULL QWERTYCOIN_COMPATIBLE_VERSION_FULL
#define MONERO_VERSION_IS_RELEASE QWERTYCOIN_VERSION_IS_RELEASE
