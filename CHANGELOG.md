Release Notes 5.2.2
- Fix Bug "Qwertycoind crashes without any message"

Release Notes 5.1.3
- Release 5.1.3 due to massive network problems in case of trying to switch to an alternative chain. The primary nodes detected this issue and blocked all outdated Nodes. To solve this Issues in short time we decided to change net network identifer to ensure that every node uses the same version.
- This Version code is: 5.1.3.3000 (6db7f2b-g6db7f2b)
- It's important to update any Pool, Node, Exchange or Wallet to this new Version ast fast as possible. We offer binary images of the latest releases here: https://releases.qwertycoin.org
- If you would like to compile yourself, read the project "How to Compile" Details.

Release Notes 5.0.1
- Hunter (package manager)
- Added configuration file for Hunter (package manager)
- Changed Boost version to 1.66.0
- Added polly submodule with cmake toolchains for cross compilation
- Build all targets by default
- Improved build times
- Added platform files for Android
- additional infos coming soon...

Release Notes 5.0.0
- Changed back from heavy to cryptonight classic
- additional infos coming soon...

Release Notes 4.0.0
Qwertycoin plan his second stage for the upcoming hard fork to heavy algorithm
- New network identifiers to ensure that every single Users, Poolowner, Exchange update their Qwertycoin Software
- Upgrade the core to Cryptonight-Heavy / ASIC Resistant
- Switch to newest Zawy LWMA-2 Difficulty algorithm
- Upgrade Blocksize for creating larger Blocks and Transactions
- Increase Syncperformance
- Load Checkpoints from CSV file (CLI Only)
- Better Rpc error code handling
- Unmixable dust update
- Fix Debug for win32 systems
- Check for fee address
- Masternode optional check for fee in relayed transaction
- Checked double bug. ok. this make 2nd key track
- Added RPC method validateaddress
- New branding

Release Notes 2.1.0
Qwertycoin has a Hardfork @ Block Height 8473
-Snapshot on Height 8473
-New Network Identifiers, New P2P Trust Key and new Set of Masternodes.

Realse Notes 2.2.0
Changed to Blocksize Increase on height 60,000 to 1,000,000 instead of 10,000

Release notes Qwertycoin 1.8.0

- Android platform support
- Added logging to blockchain synchronizer
- Refactored mnemonics
- Fixed memory leaks in mnemonics
- Fixed memory leak in simplewallet
- Fixed memory leak in cn_slow_hash
- Minor optimization of JSON KV-serializer
- LoggerMessage implementation is now platform-independent
- Deterministic wallets and Mnemonic seed, courtesy of Monero Developers
- Various build fixes
- Transactions confirmations in RPC response for walletd
- Log file location for simplewallet
- New RPC methods
- Support for solo GPU mining
- Additional validation and attribution

Release Notes 1.5.6

Qwertycoin project is moved to GitHub

Core Update @ Block 30,600

March 12, 2018

Release notes Qwertycoin 1.8.0:

Android platform support
Added logging to blockchain synchronizer
Refactored mnemonics
Fixed memory leaks in mnemonics
Fixed memory leak in simplewallet
Fixed memory leak in cn_slow_hash
Minor optimization of JSON KV-serializer
LoggerMessage implementation is now platform-independent
Various build fixes
Transactions confirmations in RPC response for walletd
Log file location for simplewallet
New RPC methods
Additional validation and attribution

------------------


Qwertycoin Wallet v3.0.0 & Hard fork @ Height 88,000

June 7, 2018

Qwertycoin v3 Wallets are released!
Download now: QWC Wallet
Hard fork at block height 88,000! Here’s a changelog:
+ Increase pending Transaction Alt block lifetime to 48 hours
+ Decrease pending Transaction Alt block lifetime to 24 hours
+ Smoother synchronization process
+ Cleaner daemon errors and debug messages (i.e. set_log 4)
+ Faster gui synchronizing
+ Transfer protection: Can’t send coins unless wallet synchronized!
+ Jagerman timestamp fix (negative block time and 51% attacks)
+ Many smaller improvements

--------------


Qwertycoin v4 (Diavik)

July 10, 2018

New network identifiers to ensure that every single Users, Poolowner, Exchange update their Qwertycoin Software
Upgrade the core to Cryptonight-Heavy / ASIC Resistant ON HEIGHT 110,520
Upgrade Blocksize for creating larger Blocks and Transactions
Increase Syncperformance
Load Checkpoints from CSV file (CLI Only)
Better Rpc error code handling
Unmixable dust update
Fix Debug for win32 systems
Check for fee address
Masternode optional check for fee in relayed transaction
Checked double bug. ok. this make 2nd key track
Added RPC method validateaddress
New branding

