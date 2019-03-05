# Generate version.h header
configure_file(
    "${CMAKE_CURRENT_LIST_DIR}/version.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/version.h"
    @ONLY)
message(STATUS "Generated version.h header: ${CMAKE_CURRENT_BINARY_DIR}/version.h")

set(Qwertycoin_INCLUDE_DIRS
    "${Boost_INCLUDE_DIRS}"
    "${MINIUPNP_INCLUDE_DIRS}"
    "${SPARSEHASH_INCLUDE_DIRS}"
    "${CMAKE_CURRENT_BINARY_DIR}"
    "${CMAKE_CURRENT_LIST_DIR}"
    "${CMAKE_CURRENT_LIST_DIR}/../include")

if(MSVC)
    list(APPEND Qwertycoin_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/Platform/Windows")
    list(APPEND Qwertycoin_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/Platform/msc")
elseif(APPLE)
    list(APPEND Qwertycoin_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/Platform/OSX")
    list(APPEND Qwertycoin_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/Platform/Posix")
elseif(ANDROID)
    list(APPEND Qwertycoin_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/Platform/Android")
    list(APPEND Qwertycoin_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/Platform/Posix")
else()
    list(APPEND Qwertycoin_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/Platform/Linux")
    list(APPEND Qwertycoin_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/Platform/Posix")
endif()

# Qwertycoin::BinaryInfo # NOTE: Ignore this. It's not a target.

# Qwertycoin::BlockchainExplorer

set(QwertycoinBlockchainExplorer_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/BlockchainExplorer/BlockchainExplorer.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/BlockchainExplorer/BlockchainExplorer.h"
    "${CMAKE_CURRENT_LIST_DIR}/BlockchainExplorer/BlockchainExplorerDataBuilder.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/BlockchainExplorer/BlockchainExplorerDataBuilder.h"
    "${CMAKE_CURRENT_LIST_DIR}/BlockchainExplorer/BlockchainExplorerErrors.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/BlockchainExplorer/BlockchainExplorerErrors.h")

add_library(QwertycoinBlockchainExplorer STATIC ${QwertycoinBlockchainExplorer_SOURCES})
add_library(Qwertycoin::BlockchainExplorer ALIAS QwertycoinBlockchainExplorer)
target_include_directories(QwertycoinBlockchainExplorer PRIVATE ${Qwertycoin_INCLUDE_DIRS})

# Qwertycoin::Common

set(QwertycoinCommon_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/Common/ArrayRef.h"
    "${CMAKE_CURRENT_LIST_DIR}/Common/ArrayView.h"
    "${CMAKE_CURRENT_LIST_DIR}/Common/Base58.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Common/Base58.h"
    "${CMAKE_CURRENT_LIST_DIR}/Common/BlockingQueue.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Common/BlockingQueue.h"
    "${CMAKE_CURRENT_LIST_DIR}/Common/CommandLine.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Common/CommandLine.h"
    "${CMAKE_CURRENT_LIST_DIR}/Common/ConsoleHandler.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Common/ConsoleHandler.h"
    "${CMAKE_CURRENT_LIST_DIR}/Common/ConsoleTools.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Common/ConsoleTools.h"
    "${CMAKE_CURRENT_LIST_DIR}/Common/DnsTools.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Common/DnsTools.h"
    "${CMAKE_CURRENT_LIST_DIR}/Common/FileMappedVector.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Common/FileMappedVector.h"
    "${CMAKE_CURRENT_LIST_DIR}/Common/IInputStream.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Common/IInputStream.h"
    "${CMAKE_CURRENT_LIST_DIR}/Common/IOutputStream.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Common/IOutputStream.h"
    "${CMAKE_CURRENT_LIST_DIR}/Common/JsonValue.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Common/JsonValue.h"
    "${CMAKE_CURRENT_LIST_DIR}/Common/Lazy.h"
    "${CMAKE_CURRENT_LIST_DIR}/Common/Math.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Common/Math.h"
    "${CMAKE_CURRENT_LIST_DIR}/Common/MemoryInputStream.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Common/MemoryInputStream.h"
    "${CMAKE_CURRENT_LIST_DIR}/Common/ObserverManager.h"
    "${CMAKE_CURRENT_LIST_DIR}/Common/PathTools.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Common/PathTools.h"
    "${CMAKE_CURRENT_LIST_DIR}/Common/ScopeExit.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Common/ScopeExit.h"
    "${CMAKE_CURRENT_LIST_DIR}/Common/ShuffleGenerator.h"
    "${CMAKE_CURRENT_LIST_DIR}/Common/SignalHandler.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Common/SignalHandler.h"
    "${CMAKE_CURRENT_LIST_DIR}/Common/StdInputStream.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Common/StdInputStream.h"
    "${CMAKE_CURRENT_LIST_DIR}/Common/StdOutputStream.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Common/StdOutputStream.h"
    "${CMAKE_CURRENT_LIST_DIR}/Common/StreamTools.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Common/StreamTools.h"
    "${CMAKE_CURRENT_LIST_DIR}/Common/StringBuffer.h"
    "${CMAKE_CURRENT_LIST_DIR}/Common/StringInputStream.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Common/StringInputStream.h"
    "${CMAKE_CURRENT_LIST_DIR}/Common/StringOutputStream.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Common/StringOutputStream.h"
    "${CMAKE_CURRENT_LIST_DIR}/Common/StringTools.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Common/StringTools.h"
    "${CMAKE_CURRENT_LIST_DIR}/Common/StringUtils.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Common/StringUtils.h"
    "${CMAKE_CURRENT_LIST_DIR}/Common/StringView.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Common/StringView.h"
    "${CMAKE_CURRENT_LIST_DIR}/Common/Util.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Common/Util.h"
    "${CMAKE_CURRENT_LIST_DIR}/Common/Varint.h"
    "${CMAKE_CURRENT_LIST_DIR}/Common/VectorOutputStream.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Common/VectorOutputStream.h"
    "${CMAKE_CURRENT_LIST_DIR}/Common/VectorUtils.h"
    "${CMAKE_CURRENT_LIST_DIR}/Common/int-util.h"
    "${CMAKE_CURRENT_LIST_DIR}/Common/pod-class.h"
    "${CMAKE_CURRENT_LIST_DIR}/Common/static_assert.h")

set(QwertycoinCommon_LIBS
    Threads::Threads
    Qwertycoin::Crypto)

add_library(QwertycoinCommon STATIC ${QwertycoinCommon_SOURCES})
add_library(Qwertycoin::Common ALIAS QwertycoinCommon)
target_include_directories(QwertycoinCommon PRIVATE ${Qwertycoin_INCLUDE_DIRS})
target_link_libraries(QwertycoinCommon PRIVATE ${QwertycoinCommon_LIBS})

if (UNIX AND NOT ANDROID)
    target_link_libraries(QwertycoinCommon PUBLIC -lresolv)
endif()

# Qwertycoin::ConnectivityTool

set(QwertycoinConnectivityTool_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/ConnectivityTool/ConnectivityTool.cpp")

if(WIN32)
    list(APPEND QwertycoinConnectivityTool_SOURCES "${CMAKE_CURRENT_LIST_DIR}/BinaryInfo/connectivity_tool.rc")
endif()

set(QwertycoinConnectivityTool_LIBS
    Boost::program_options
    Qwertycoin::Common
    Qwertycoin::CryptoNoteCore
    Qwertycoin::P2p
    Qwertycoin::Rpc
    Qwertycoin::Serialization
    Qwertycoin::System)

if(WIN32)
    list(APPEND QwertycoinConnectivityTool_LIBS ws2_32)
endif()

add_executable(QwertycoinConnectivityTool ${QwertycoinConnectivityTool_SOURCES})
add_executable(Qwertycoin::ConnectivityTool ALIAS QwertycoinConnectivityTool)
target_include_directories(QwertycoinConnectivityTool PRIVATE ${Qwertycoin_INCLUDE_DIRS})
target_link_libraries(QwertycoinConnectivityTool ${QwertycoinConnectivityTool_LIBS})
set_target_properties(QwertycoinConnectivityTool PROPERTIES OUTPUT_NAME "connectivity_tool")

# Qwertycoin::Crypto

set(QwertycoinCrypto_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/crypto/aesb.c"
    "${CMAKE_CURRENT_LIST_DIR}/crypto/aesb.h"
    "${CMAKE_CURRENT_LIST_DIR}/crypto/blake256.c"
    "${CMAKE_CURRENT_LIST_DIR}/crypto/blake256.h"
    "${CMAKE_CURRENT_LIST_DIR}/crypto/chacha8.c"
    "${CMAKE_CURRENT_LIST_DIR}/crypto/chacha8.h"
    "${CMAKE_CURRENT_LIST_DIR}/crypto/cn_slow_hash.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/crypto/cn_slow_hash_hard_arm.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/crypto/cn_slow_hash_hard_intel.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/crypto/cn_slow_hash_soft.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/crypto/crypto-ops-data.c"
    "${CMAKE_CURRENT_LIST_DIR}/crypto/crypto-ops.c"
    "${CMAKE_CURRENT_LIST_DIR}/crypto/crypto-ops.h"
    "${CMAKE_CURRENT_LIST_DIR}/crypto/crypto.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/crypto/crypto.h"
    "${CMAKE_CURRENT_LIST_DIR}/crypto/generic-ops.h"
    "${CMAKE_CURRENT_LIST_DIR}/crypto/groestl.c"
    "${CMAKE_CURRENT_LIST_DIR}/crypto/groestl.h"
    "${CMAKE_CURRENT_LIST_DIR}/crypto/groestl_tables.h"
    "${CMAKE_CURRENT_LIST_DIR}/crypto/hash-extra-blake.c"
    "${CMAKE_CURRENT_LIST_DIR}/crypto/hash-extra-groestl.c"
    "${CMAKE_CURRENT_LIST_DIR}/crypto/hash-extra-jh.c"
    "${CMAKE_CURRENT_LIST_DIR}/crypto/hash-extra-skein.c"
    "${CMAKE_CURRENT_LIST_DIR}/crypto/hash-ops.h"
    "${CMAKE_CURRENT_LIST_DIR}/crypto/hash.c"
    "${CMAKE_CURRENT_LIST_DIR}/crypto/hash.h"
    "${CMAKE_CURRENT_LIST_DIR}/crypto/initializer.h"
    "${CMAKE_CURRENT_LIST_DIR}/crypto/jh.c"
    "${CMAKE_CURRENT_LIST_DIR}/crypto/jh.h"
    "${CMAKE_CURRENT_LIST_DIR}/crypto/keccak.c"
    "${CMAKE_CURRENT_LIST_DIR}/crypto/keccak.h"
    "${CMAKE_CURRENT_LIST_DIR}/crypto/oaes_config.h"
    "${CMAKE_CURRENT_LIST_DIR}/crypto/oaes_lib.c"
    "${CMAKE_CURRENT_LIST_DIR}/crypto/oaes_lib.h"
    "${CMAKE_CURRENT_LIST_DIR}/crypto/random.c"
    "${CMAKE_CURRENT_LIST_DIR}/crypto/random.h"
    "${CMAKE_CURRENT_LIST_DIR}/crypto/skein.c"
    "${CMAKE_CURRENT_LIST_DIR}/crypto/skein.h"
    "${CMAKE_CURRENT_LIST_DIR}/crypto/skein_port.h"
    "${CMAKE_CURRENT_LIST_DIR}/crypto/slow-hash.c"
    "${CMAKE_CURRENT_LIST_DIR}/crypto/slow-hash.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/crypto/slow-hash.inl"
    "${CMAKE_CURRENT_LIST_DIR}/crypto/tree-hash.c")

add_library(QwertycoinCrypto STATIC ${QwertycoinCrypto_SOURCES})
add_library(Qwertycoin::Crypto ALIAS QwertycoinCrypto)
target_include_directories(QwertycoinCrypto PRIVATE ${Qwertycoin_INCLUDE_DIRS})

if(NOT ARM AND NOT PPC64LE)
    message(STATUS "[Qwertycoin::Crypto] AES support enabled.")
    target_compile_options(QwertycoinCrypto PRIVATE -maes)
elseif(PPC64LE)
    message(STATUS "[Qwertycoin::Crypto] AES support disabled.")
endif()

# Qwertycoin::CryptoNoteCore

set(QwertycoinCryptoNoteCore_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/Account.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/Account.h"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/BlockIndex.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/BlockIndex.h"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/Blockchain.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/Blockchain.h"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/BlockchainIndices.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/BlockchainIndices.h"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/BlockchainMessages.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/BlockchainMessages.h"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/Checkpoints.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/Checkpoints.h"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/Core.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/Core.h"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/CoreConfig.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/CoreConfig.h"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/CryptoNoteBasic.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/CryptoNoteBasic.h"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/CryptoNoteBasicImpl.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/CryptoNoteBasicImpl.h"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/CryptoNoteFormatUtils.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/CryptoNoteFormatUtils.h"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/CryptoNoteSerialization.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/CryptoNoteSerialization.h"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/CryptoNoteStatInfo.h"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/CryptoNoteTools.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/CryptoNoteTools.h"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/Currency.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/Currency.h"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/Difficulty.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/Difficulty.h"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/IBlock.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/IBlock.h"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/IBlockchainStorageObserver.h"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/ICore.h"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/ICoreObserver.h"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/IMinerHandler.h"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/ITimeProvider.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/ITimeProvider.h"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/ITransactionValidator.h"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/ITxPoolObserver.h"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/IntrusiveLinkedList.h"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/MessageQueue.h"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/Miner.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/Miner.h"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/MinerConfig.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/MinerConfig.h"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/OnceInInterval.h"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/SwappedMap.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/SwappedMap.h"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/SwappedVector.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/SwappedVector.h"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/Transaction.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/TransactionApi.h"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/TransactionApiExtra.h"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/TransactionExtra.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/TransactionExtra.h"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/TransactionPool.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/TransactionPool.h"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/TransactionPrefixImpl.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/TransactionUtils.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/TransactionUtils.h"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/UpgradeDetector.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/UpgradeDetector.h"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/VerificationContext.h")

set(QwertycoinCryptoNoteCore_LIBS
    Boost::filesystem
    Boost::program_options
    sparsehash::sparsehash
    Qwertycoin::BlockchainExplorer
    Qwertycoin::Common
    Qwertycoin::Crypto
    Qwertycoin::Serialization)

add_library(QwertycoinCryptoNoteCore STATIC ${QwertycoinCryptoNoteCore_SOURCES})
add_library(Qwertycoin::CryptoNoteCore ALIAS QwertycoinCryptoNoteCore)
target_include_directories(QwertycoinCryptoNoteCore PRIVATE ${Qwertycoin_INCLUDE_DIRS})
target_link_libraries(QwertycoinCryptoNoteCore PRIVATE ${QwertycoinCryptoNoteCore_LIBS})

# Qwertycoin::CryptoNoteProtocol

set(QwertycoinCryptoNoteProtocol_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteProtocol/CryptoNoteProtocolDefinitions.h"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteProtocol/CryptoNoteProtocolHandler.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteProtocol/CryptoNoteProtocolHandler.h"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteProtocol/CryptoNoteProtocolHandlerCommon.h"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteProtocol/ICryptoNoteProtocolObserver.h"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteProtocol/ICryptoNoteProtocolQuery.h")

add_library(QwertycoinCryptoNoteProtocol STATIC ${QwertycoinCryptoNoteProtocol_SOURCES})
add_library(Qwertycoin::CryptoNoteProtocol ALIAS QwertycoinCryptoNoteProtocol)
target_include_directories(QwertycoinCryptoNoteProtocol PRIVATE ${Qwertycoin_INCLUDE_DIRS})

# Qwertycoin::Daemon

set(QwertycoinDaemon_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/Daemon/Daemon.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Daemon/DaemonCommandsHandler.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Daemon/DaemonCommandsHandler.h")

if(WIN32)
    list(APPEND QwertycoinDaemon_SOURCES "${CMAKE_CURRENT_LIST_DIR}/BinaryInfo/daemon.rc")
endif()

set(QwertycoinDaemon_LIBS
    Boost::filesystem
    Qwertycoin::Common
    Qwertycoin::CryptoNoteCore
    Qwertycoin::Logging
    Qwertycoin::P2p
    Qwertycoin::Rpc
    Qwertycoin::Serialization
    Qwertycoin::System)

add_executable(QwertycoinDaemon ${QwertycoinDaemon_SOURCES})
add_executable(Qwertycoin::Daemon ALIAS QwertycoinDaemon)
target_include_directories(QwertycoinDaemon PRIVATE ${Qwertycoin_INCLUDE_DIRS})
target_link_libraries(QwertycoinDaemon PRIVATE ${QwertycoinDaemon_LIBS})
set_target_properties(QwertycoinDaemon PROPERTIES OUTPUT_NAME "qwertycoind")

# Qwertycoin::Http

set(QwertycoinHttp_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/HTTP/HttpParser.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/HTTP/HttpParser.h"
    "${CMAKE_CURRENT_LIST_DIR}/HTTP/HttpParserErrorCodes.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/HTTP/HttpParserErrorCodes.h"
    "${CMAKE_CURRENT_LIST_DIR}/HTTP/HttpRequest.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/HTTP/HttpRequest.h"
    "${CMAKE_CURRENT_LIST_DIR}/HTTP/HttpResponse.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/HTTP/HttpResponse.h")

add_library(QwertycoinHttp STATIC ${QwertycoinHttp_SOURCES})
add_library(Qwertycoin::Http ALIAS QwertycoinHttp)
target_include_directories(QwertycoinHttp PRIVATE ${Qwertycoin_INCLUDE_DIRS})

# Qwertycoin::InProcessNode

set(QwertycoinInProcessNode_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/InProcessNode/InProcessNode.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/InProcessNode/InProcessNode.h"
    "${CMAKE_CURRENT_LIST_DIR}/InProcessNode/InProcessNodeErrors.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/InProcessNode/InProcessNodeErrors.h")

add_library(QwertycoinInProcessNode STATIC ${QwertycoinInProcessNode_SOURCES})
add_library(Qwertycoin::InProcessNode ALIAS QwertycoinInProcessNode)
target_include_directories(QwertycoinInProcessNode PRIVATE ${Qwertycoin_INCLUDE_DIRS})

# Qwertycoin::JsonRpcServer

set(QwertycoinJsonRpcServer_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/JsonRpcServer/JsonRpcServer.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/JsonRpcServer/JsonRpcServer.h")

add_library(QwertycoinJsonRpcServer STATIC ${QwertycoinJsonRpcServer_SOURCES})
add_library(Qwertycoin::JsonRpcServer ALIAS QwertycoinJsonRpcServer)
target_include_directories(QwertycoinJsonRpcServer PRIVATE ${Qwertycoin_INCLUDE_DIRS})

# Qwertycoin::Logging

set(QwertycoinLogging_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/Logging/CommonLogger.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Logging/CommonLogger.h"
    "${CMAKE_CURRENT_LIST_DIR}/Logging/ConsoleLogger.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Logging/ConsoleLogger.h"
    "${CMAKE_CURRENT_LIST_DIR}/Logging/FileLogger.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Logging/FileLogger.h"
    "${CMAKE_CURRENT_LIST_DIR}/Logging/ILogger.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Logging/ILogger.h"
    "${CMAKE_CURRENT_LIST_DIR}/Logging/LoggerGroup.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Logging/LoggerGroup.h"
    "${CMAKE_CURRENT_LIST_DIR}/Logging/LoggerManager.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Logging/LoggerManager.h"
    "${CMAKE_CURRENT_LIST_DIR}/Logging/LoggerMessage.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Logging/LoggerMessage.h"
    "${CMAKE_CURRENT_LIST_DIR}/Logging/LoggerRef.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Logging/LoggerRef.h"
    "${CMAKE_CURRENT_LIST_DIR}/Logging/StreamLogger.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Logging/StreamLogger.h")

add_library(QwertycoinLogging STATIC ${QwertycoinLogging_SOURCES})
add_library(Qwertycoin::Logging ALIAS QwertycoinLogging)
target_include_directories(QwertycoinLogging PRIVATE ${Qwertycoin_INCLUDE_DIRS})

# Qwertycoin::Miner

set(QwertycoinMiner_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/Miner/BlockchainMonitor.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Miner/BlockchainMonitor.h"
    "${CMAKE_CURRENT_LIST_DIR}/Miner/Miner.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Miner/Miner.h"
    "${CMAKE_CURRENT_LIST_DIR}/Miner/MinerEvent.h"
    "${CMAKE_CURRENT_LIST_DIR}/Miner/MinerManager.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Miner/MinerManager.h"
    "${CMAKE_CURRENT_LIST_DIR}/Miner/MiningConfig.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Miner/MiningConfig.h"
    "${CMAKE_CURRENT_LIST_DIR}/Miner/main.cpp")

if(WIN32)
    list(APPEND QwertycoinMiner_SOURCES "${CMAKE_CURRENT_LIST_DIR}/BinaryInfo/miner.rc")
endif()

set(QwertycoinMiner_LIBS
    Boost::program_options
    Qwertycoin::Common
    Qwertycoin::CryptoNoteCore
    Qwertycoin::Logging
    Qwertycoin::Rpc
    Qwertycoin::Serialization
    Qwertycoin::System)

if(WIN32)
    list(APPEND QwertycoinMiner_LIBS ws2_32)
endif()

add_executable(QwertycoinMiner ${QwertycoinMiner_SOURCES})
add_executable(Qwertycoin::Miner ALIAS QwertycoinMiner)
target_include_directories(QwertycoinMiner PRIVATE ${Qwertycoin_INCLUDE_DIRS})
target_link_libraries(QwertycoinMiner PRIVATE ${QwertycoinMiner_LIBS})
set_target_properties(QwertycoinMiner PROPERTIES OUTPUT_NAME "miner")

# Qwertycoin::Mnemonics

set(QwertycoinMnemonics_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/mnemonics/chinese_simplified.h"
    "${CMAKE_CURRENT_LIST_DIR}/mnemonics/dutch.h"
    "${CMAKE_CURRENT_LIST_DIR}/mnemonics/electrum-words.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/mnemonics/electrum-words.h"
    "${CMAKE_CURRENT_LIST_DIR}/mnemonics/english.h"
    "${CMAKE_CURRENT_LIST_DIR}/mnemonics/french.h"
    "${CMAKE_CURRENT_LIST_DIR}/mnemonics/german.h"
    "${CMAKE_CURRENT_LIST_DIR}/mnemonics/italian.h"
    "${CMAKE_CURRENT_LIST_DIR}/mnemonics/japanese.h"
    "${CMAKE_CURRENT_LIST_DIR}/mnemonics/language_base.h"
    "${CMAKE_CURRENT_LIST_DIR}/mnemonics/polish.h"
    "${CMAKE_CURRENT_LIST_DIR}/mnemonics/portuguese.h"
    "${CMAKE_CURRENT_LIST_DIR}/mnemonics/russian.h"
    "${CMAKE_CURRENT_LIST_DIR}/mnemonics/singleton.h"
    "${CMAKE_CURRENT_LIST_DIR}/mnemonics/spanish.h"
    "${CMAKE_CURRENT_LIST_DIR}/mnemonics/ukrainian.h")

add_library(QwertycoinMnemonics STATIC ${QwertycoinMnemonics_SOURCES})
add_library(Qwertycoin::Mnemonics ALIAS QwertycoinMnemonics)
target_include_directories(QwertycoinMnemonics PRIVATE ${Qwertycoin_INCLUDE_DIRS})

# Qwertycoin::NodeRpcProxy

set(QwertycoinNodeRpcProxy_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/NodeRpcProxy/NodeErrors.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/NodeRpcProxy/NodeErrors.h"
    "${CMAKE_CURRENT_LIST_DIR}/NodeRpcProxy/NodeRpcProxy.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/NodeRpcProxy/NodeRpcProxy.h")

add_library(QwertycoinNodeRpcProxy STATIC ${QwertycoinNodeRpcProxy_SOURCES})
add_library(Qwertycoin::NodeRpcProxy ALIAS QwertycoinNodeRpcProxy)
target_include_directories(QwertycoinNodeRpcProxy PRIVATE ${Qwertycoin_INCLUDE_DIRS})

# Qwertycoin::P2p

set(QwertycoinP2p_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/P2p/ConnectionContext.h"
    "${CMAKE_CURRENT_LIST_DIR}/P2p/IP2pNodeInternal.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/P2p/IP2pNodeInternal.h"
    "${CMAKE_CURRENT_LIST_DIR}/P2p/LevinProtocol.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/P2p/LevinProtocol.h"
    "${CMAKE_CURRENT_LIST_DIR}/P2p/NetNode.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/P2p/NetNode.h"
    "${CMAKE_CURRENT_LIST_DIR}/P2p/NetNodeCommon.h"
    "${CMAKE_CURRENT_LIST_DIR}/P2p/NetNodeConfig.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/P2p/NetNodeConfig.h"
    "${CMAKE_CURRENT_LIST_DIR}/P2p/P2pConnectionProxy.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/P2p/P2pConnectionProxy.h"
    "${CMAKE_CURRENT_LIST_DIR}/P2p/P2pContext.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/P2p/P2pContext.h"
    "${CMAKE_CURRENT_LIST_DIR}/P2p/P2pContextOwner.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/P2p/P2pContextOwner.h"
    "${CMAKE_CURRENT_LIST_DIR}/P2p/P2pInterfaces.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/P2p/P2pInterfaces.h"
    "${CMAKE_CURRENT_LIST_DIR}/P2p/P2pNetworks.h"
    "${CMAKE_CURRENT_LIST_DIR}/P2p/P2pNode.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/P2p/P2pNode.h"
    "${CMAKE_CURRENT_LIST_DIR}/P2p/P2pNodeConfig.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/P2p/P2pNodeConfig.h"
    "${CMAKE_CURRENT_LIST_DIR}/P2p/P2pProtocolDefinitions.h"
    "${CMAKE_CURRENT_LIST_DIR}/P2p/P2pProtocolTypes.h"
    "${CMAKE_CURRENT_LIST_DIR}/P2p/PeerListManager.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/P2p/PeerListManager.h")

set(QwertycoinP2p_LIBS
    MiniUPnP::miniupnpc
    Qwertycoin::CryptoNoteProtocol)

add_library(QwertycoinP2p STATIC ${QwertycoinP2p_SOURCES})
add_library(Qwertycoin::P2p ALIAS QwertycoinP2p)
target_include_directories(QwertycoinP2p PRIVATE ${Qwertycoin_INCLUDE_DIRS})
target_link_libraries(QwertycoinP2p PRIVATE ${QwertycoinP2p_LIBS})

# Qwertycoin::PaymentGate

set(QwertycoinPaymentGate_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/PaymentGate/NodeFactory.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/PaymentGate/NodeFactory.h"
    "${CMAKE_CURRENT_LIST_DIR}/PaymentGate/PaymentServiceJsonRpcMessages.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/PaymentGate/PaymentServiceJsonRpcMessages.h"
    "${CMAKE_CURRENT_LIST_DIR}/PaymentGate/PaymentServiceJsonRpcServer.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/PaymentGate/PaymentServiceJsonRpcServer.h"
    "${CMAKE_CURRENT_LIST_DIR}/PaymentGate/WalletService.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/PaymentGate/WalletService.h"
    "${CMAKE_CURRENT_LIST_DIR}/PaymentGate/WalletServiceErrorCategory.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/PaymentGate/WalletServiceErrorCategory.h")

set(QwertycoinPaymentGate_LIBS
    Qwertycoin::JsonRpcServer
    Qwertycoin::NodeRpcProxy
    Qwertycoin::System
    Qwertycoin::Wallet)

add_library(QwertycoinPaymentGate STATIC ${QwertycoinPaymentGate_SOURCES})
add_library(Qwertycoin::PaymentGate ALIAS QwertycoinPaymentGate)
target_include_directories(QwertycoinPaymentGate PRIVATE ${Qwertycoin_INCLUDE_DIRS})
target_link_libraries(QwertycoinPaymentGate PRIVATE ${QwertycoinPaymentGate_LIBS})

# Qwertycoin::PaymentGateService

set(QwertycoinPaymentGateService_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/PaymentGateService/ConfigurationManager.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/PaymentGateService/ConfigurationManager.h"
    "${CMAKE_CURRENT_LIST_DIR}/PaymentGateService/PaymentGateService.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/PaymentGateService/PaymentGateService.h"
    "${CMAKE_CURRENT_LIST_DIR}/PaymentGateService/PaymentServiceConfiguration.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/PaymentGateService/PaymentServiceConfiguration.h"
    "${CMAKE_CURRENT_LIST_DIR}/PaymentGateService/RpcNodeConfiguration.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/PaymentGateService/RpcNodeConfiguration.h"
    "${CMAKE_CURRENT_LIST_DIR}/PaymentGateService/main.cpp")

if(WIN32)
    list(APPEND QwertycoinPaymentGateService_SOURCES "${CMAKE_CURRENT_LIST_DIR}/BinaryInfo/walletd.rc")
endif()

set(QwertycoinPaymentGateService_LIBS
    Boost::filesystem
    Boost::program_options
    Qwertycoin::Common
    Qwertycoin::CryptoNoteCore
    Qwertycoin::InProcessNode
    Qwertycoin::Logging
    Qwertycoin::Rpc
    Qwertycoin::System)

add_executable(QwertycoinPaymentGateService ${QwertycoinPaymentGateService_SOURCES})
add_executable(Qwertycoin::PaymentGateService ALIAS QwertycoinPaymentGateService)
target_include_directories(QwertycoinPaymentGateService PRIVATE ${Qwertycoin_INCLUDE_DIRS})
target_link_libraries(QwertycoinPaymentGateService ${QwertycoinPaymentGateService_LIBS})
set_target_properties(QwertycoinPaymentGateService PROPERTIES OUTPUT_NAME "walletd")

# Qwertycoin::Platform # TODO: Merge this target into Qwertycoin::System

# Qwertycoin::Rpc

set(QwertycoinRpc_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/Rpc/CoreRpcServerCommandsDefinitions.h"
    "${CMAKE_CURRENT_LIST_DIR}/Rpc/CoreRpcServerErrorCodes.h"
    "${CMAKE_CURRENT_LIST_DIR}/Rpc/HttpClient.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Rpc/HttpClient.h"
    "${CMAKE_CURRENT_LIST_DIR}/Rpc/HttpServer.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Rpc/HttpServer.h"
    "${CMAKE_CURRENT_LIST_DIR}/Rpc/JsonRpc.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Rpc/JsonRpc.h"
    "${CMAKE_CURRENT_LIST_DIR}/Rpc/RpcServer.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Rpc/RpcServer.h"
    "${CMAKE_CURRENT_LIST_DIR}/Rpc/RpcServerConfig.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Rpc/RpcServerConfig.h")

set(QwertycoinRpc_LIBS
    Qwertycoin::BlockchainExplorer
    Qwertycoin::CryptoNoteCore
    Qwertycoin::P2p
    Qwertycoin::PaymentGate
    Qwertycoin::Serialization
    Qwertycoin::System
    Qwertycoin::Http)

add_library(QwertycoinRpc STATIC ${QwertycoinRpc_SOURCES})
add_library(Qwertycoin::Rpc ALIAS QwertycoinRpc)
target_include_directories(QwertycoinRpc PRIVATE ${Qwertycoin_INCLUDE_DIRS})
target_link_libraries(QwertycoinRpc PRIVATE ${QwertycoinRpc_LIBS})

# Qwertycoin::Serialization

set(QwertycoinSerialization_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/Serialization/BinaryInputStreamSerializer.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Serialization/BinaryInputStreamSerializer.h"
    "${CMAKE_CURRENT_LIST_DIR}/Serialization/BinaryOutputStreamSerializer.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Serialization/BinaryOutputStreamSerializer.h"
    "${CMAKE_CURRENT_LIST_DIR}/Serialization/BinarySerializationTools.h"
    "${CMAKE_CURRENT_LIST_DIR}/Serialization/BlockchainExplorerDataSerialization.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Serialization/BlockchainExplorerDataSerialization.h"
    "${CMAKE_CURRENT_LIST_DIR}/Serialization/ISerializer.h"
    "${CMAKE_CURRENT_LIST_DIR}/Serialization/IStream.h"
    "${CMAKE_CURRENT_LIST_DIR}/Serialization/JsonInputStreamSerializer.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Serialization/JsonInputStreamSerializer.h"
    "${CMAKE_CURRENT_LIST_DIR}/Serialization/JsonInputValueSerializer.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Serialization/JsonInputValueSerializer.h"
    "${CMAKE_CURRENT_LIST_DIR}/Serialization/JsonOutputStreamSerializer.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Serialization/JsonOutputStreamSerializer.h"
    "${CMAKE_CURRENT_LIST_DIR}/Serialization/KVBinaryCommon.h"
    "${CMAKE_CURRENT_LIST_DIR}/Serialization/KVBinaryInputStreamSerializer.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Serialization/KVBinaryInputStreamSerializer.h"
    "${CMAKE_CURRENT_LIST_DIR}/Serialization/KVBinaryOutputStreamSerializer.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Serialization/KVBinaryOutputStreamSerializer.h"
    "${CMAKE_CURRENT_LIST_DIR}/Serialization/MemoryStream.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Serialization/MemoryStream.h"
    "${CMAKE_CURRENT_LIST_DIR}/Serialization/SerializationOverloads.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Serialization/SerializationOverloads.h"
    "${CMAKE_CURRENT_LIST_DIR}/Serialization/SerializationTools.h")

add_library(QwertycoinSerialization STATIC ${QwertycoinSerialization_SOURCES})
add_library(Qwertycoin::Serialization ALIAS QwertycoinSerialization)
target_include_directories(QwertycoinSerialization PRIVATE ${Qwertycoin_INCLUDE_DIRS})
target_link_libraries(QwertycoinSerialization PRIVATE Qwertycoin::Common)

# Qwertycoin::SimpleWallet

set(QwertycoinSimpleWallet_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/SimpleWallet/PasswordContainer.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/SimpleWallet/PasswordContainer.h"
    "${CMAKE_CURRENT_LIST_DIR}/SimpleWallet/SimpleWallet.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/SimpleWallet/SimpleWallet.h")

if(WIN32)
    list(APPEND QwertycoinSimpleWallet_SOURCES "${CMAKE_CURRENT_LIST_DIR}/BinaryInfo/simplewallet.rc")
endif()

set(QwertycoinSimpleWallet_LIBS
    Boost::filesystem
    Boost::program_options
    Qwertycoin::Common
    Qwertycoin::Logging
    Qwertycoin::NodeRpcProxy
    Qwertycoin::Rpc
    Qwertycoin::Serialization
    Qwertycoin::Wallet)

if(WIN32)
    list(APPEND QwertycoinSimpleWallet_LIBS ws2_32)
endif()

add_executable(QwertycoinSimpleWallet ${QwertycoinSimpleWallet_SOURCES})
add_executable(Qwertycoin::SimpleWallet ALIAS QwertycoinSimpleWallet)
target_include_directories(QwertycoinSimpleWallet PRIVATE ${Qwertycoin_INCLUDE_DIRS})
target_link_libraries(QwertycoinSimpleWallet ${QwertycoinSimpleWallet_LIBS})
set_target_properties(QwertycoinSimpleWallet PROPERTIES OUTPUT_NAME "simplewallet")

# Qwertycoin::System

set(QwertycoinSystem_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/System/Context.h"
    "${CMAKE_CURRENT_LIST_DIR}/System/ContextGroup.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/System/ContextGroup.h"
    "${CMAKE_CURRENT_LIST_DIR}/System/ContextGroupTimeout.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/System/ContextGroupTimeout.h"
    "${CMAKE_CURRENT_LIST_DIR}/System/Event.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/System/Event.h"
    "${CMAKE_CURRENT_LIST_DIR}/System/EventLock.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/System/EventLock.h"
    "${CMAKE_CURRENT_LIST_DIR}/System/InterruptedException.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/System/InterruptedException.h"
    "${CMAKE_CURRENT_LIST_DIR}/System/Ipv4Address.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/System/Ipv4Address.h"
    "${CMAKE_CURRENT_LIST_DIR}/System/OperationTimeout.h"
    "${CMAKE_CURRENT_LIST_DIR}/System/RemoteContext.h"
    "${CMAKE_CURRENT_LIST_DIR}/System/RemoteEventLock.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/System/RemoteEventLock.h"
    "${CMAKE_CURRENT_LIST_DIR}/System/TcpStream.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/System/TcpStream.h")

if(MSVC)
    list(APPEND QwertycoinSystem_SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/Platform/Windows/System/Dispatcher.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/Windows/System/Dispatcher.h"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/Windows/System/ErrorMessage.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/Windows/System/ErrorMessage.h"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/Windows/System/Future.h"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/Windows/System/Ipv4Resolver.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/Windows/System/Ipv4Resolver.h"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/Windows/System/MemoryMappedFile.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/Windows/System/MemoryMappedFile.h"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/Windows/System/TcpConnection.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/Windows/System/TcpConnection.h"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/Windows/System/TcpConnector.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/Windows/System/TcpConnector.h"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/Windows/System/TcpListener.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/Windows/System/TcpListener.h"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/Windows/System/Timer.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/Windows/System/Timer.h")
elseif(APPLE)
    list(APPEND QwertycoinSystem_SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/Platform/OSX/System/Context.c"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/OSX/System/Context.h"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/OSX/System/Dispatcher.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/OSX/System/Dispatcher.h"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/OSX/System/ErrorMessage.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/OSX/System/ErrorMessage.h"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/OSX/System/Future.h"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/OSX/System/Ipv4Resolver.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/OSX/System/Ipv4Resolver.h"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/OSX/System/TcpConnection.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/OSX/System/TcpConnection.h"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/OSX/System/TcpConnector.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/OSX/System/TcpConnector.h"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/OSX/System/TcpListener.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/OSX/System/TcpListener.h"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/OSX/System/Timer.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/OSX/System/Timer.h"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/OSX/System/asm.s"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/Posix/System/MemoryMappedFile.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/Posix/System/MemoryMappedFile.h")
elseif(ANDROID)
    list(APPEND QwertycoinSystem_SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/Platform/Android/System/Context.h"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/Android/System/Dispatcher.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/Android/System/Dispatcher.h"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/Android/System/ErrorMessage.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/Android/System/ErrorMessage.h"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/Android/System/Future.h"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/Android/System/Ipv4Resolver.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/Android/System/Ipv4Resolver.h"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/Android/System/TcpConnection.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/Android/System/TcpConnection.h"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/Android/System/TcpConnector.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/Android/System/TcpConnector.h"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/Android/System/TcpListener.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/Android/System/TcpListener.h"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/Android/System/Timer.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/Android/System/Timer.h"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/Posix/System/MemoryMappedFile.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/Posix/System/MemoryMappedFile.h")
else()
    list(APPEND QwertycoinSystem_SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/Platform/Linux/System/Dispatcher.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/Linux/System/Dispatcher.h"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/Linux/System/ErrorMessage.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/Linux/System/ErrorMessage.h"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/Linux/System/Future.h"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/Linux/System/Ipv4Resolver.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/Linux/System/Ipv4Resolver.h"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/Linux/System/TcpConnection.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/Linux/System/TcpConnection.h"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/Linux/System/TcpConnector.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/Linux/System/TcpConnector.h"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/Linux/System/TcpListener.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/Linux/System/TcpListener.h"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/Linux/System/Timer.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/Linux/System/Timer.h"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/Posix/System/MemoryMappedFile.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/Posix/System/MemoryMappedFile.h")
endif()

add_library(QwertycoinSystem STATIC ${QwertycoinSystem_SOURCES})
add_library(Qwertycoin::System ALIAS QwertycoinSystem)
target_include_directories(QwertycoinSystem PRIVATE ${Qwertycoin_INCLUDE_DIRS})

# Qwertycoin::Transfers

set(QwertycoinTransfers_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/Transfers/BlockchainSynchronizer.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Transfers/BlockchainSynchronizer.h"
    "${CMAKE_CURRENT_LIST_DIR}/Transfers/CommonTypes.h"
    "${CMAKE_CURRENT_LIST_DIR}/Transfers/IBlockchainSynchronizer.h"
    "${CMAKE_CURRENT_LIST_DIR}/Transfers/IObservableImpl.h"
    "${CMAKE_CURRENT_LIST_DIR}/Transfers/SynchronizationState.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Transfers/SynchronizationState.h"
    "${CMAKE_CURRENT_LIST_DIR}/Transfers/TransfersConsumer.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Transfers/TransfersConsumer.h"
    "${CMAKE_CURRENT_LIST_DIR}/Transfers/TransfersContainer.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Transfers/TransfersContainer.h"
    "${CMAKE_CURRENT_LIST_DIR}/Transfers/TransfersSubscription.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Transfers/TransfersSubscription.h"
    "${CMAKE_CURRENT_LIST_DIR}/Transfers/TransfersSynchronizer.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Transfers/TransfersSynchronizer.h"
    "${CMAKE_CURRENT_LIST_DIR}/Transfers/TypeHelpers.h")

add_library(QwertycoinTransfers STATIC ${QwertycoinTransfers_SOURCES})
add_library(Qwertycoin::Transfers ALIAS QwertycoinTransfers)
target_include_directories(QwertycoinTransfers PRIVATE ${Qwertycoin_INCLUDE_DIRS})
target_link_libraries(QwertycoinTransfers PRIVATE Qwertycoin::CryptoNoteCore)

# Qwertycoin::Wallet

set(QwertycoinWallet_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/Wallet/IFusionManager.h"
    "${CMAKE_CURRENT_LIST_DIR}/Wallet/LegacyKeysImporter.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Wallet/LegacyKeysImporter.h"
    "${CMAKE_CURRENT_LIST_DIR}/Wallet/WalletAsyncContextCounter.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Wallet/WalletAsyncContextCounter.h"
    "${CMAKE_CURRENT_LIST_DIR}/Wallet/WalletErrors.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Wallet/WalletErrors.h"
    "${CMAKE_CURRENT_LIST_DIR}/Wallet/WalletGreen.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Wallet/WalletGreen.h"
    "${CMAKE_CURRENT_LIST_DIR}/Wallet/WalletIndices.h"
    "${CMAKE_CURRENT_LIST_DIR}/Wallet/WalletRpcServer.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Wallet/WalletRpcServer.h"
    "${CMAKE_CURRENT_LIST_DIR}/Wallet/WalletRpcServerCommandsDefinitions.h"
    "${CMAKE_CURRENT_LIST_DIR}/Wallet/WalletRpcServerErrorCodes.h"
    "${CMAKE_CURRENT_LIST_DIR}/Wallet/WalletSerializationV1.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Wallet/WalletSerializationV1.h"
    "${CMAKE_CURRENT_LIST_DIR}/Wallet/WalletSerializationV2.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Wallet/WalletSerializationV2.h"
    "${CMAKE_CURRENT_LIST_DIR}/Wallet/WalletUtils.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Wallet/WalletUtils.h"
    "${CMAKE_CURRENT_LIST_DIR}/WalletLegacy/KeysStorage.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/WalletLegacy/KeysStorage.h"
    "${CMAKE_CURRENT_LIST_DIR}/WalletLegacy/WalletHelper.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/WalletLegacy/WalletHelper.h"
    "${CMAKE_CURRENT_LIST_DIR}/WalletLegacy/WalletLegacy.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/WalletLegacy/WalletLegacy.h"
    "${CMAKE_CURRENT_LIST_DIR}/WalletLegacy/WalletLegacyEvent.h"
    "${CMAKE_CURRENT_LIST_DIR}/WalletLegacy/WalletLegacySerialization.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/WalletLegacy/WalletLegacySerialization.h"
    "${CMAKE_CURRENT_LIST_DIR}/WalletLegacy/WalletLegacySerializer.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/WalletLegacy/WalletLegacySerializer.h"
    "${CMAKE_CURRENT_LIST_DIR}/WalletLegacy/WalletRequest.h"
    "${CMAKE_CURRENT_LIST_DIR}/WalletLegacy/WalletSendTransactionContext.h"
    "${CMAKE_CURRENT_LIST_DIR}/WalletLegacy/WalletTransactionSender.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/WalletLegacy/WalletTransactionSender.h"
    "${CMAKE_CURRENT_LIST_DIR}/WalletLegacy/WalletUnconfirmedTransactions.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/WalletLegacy/WalletUnconfirmedTransactions.h"
    "${CMAKE_CURRENT_LIST_DIR}/WalletLegacy/WalletUserTransactionsCache.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/WalletLegacy/WalletUserTransactionsCache.h"
    "${CMAKE_CURRENT_LIST_DIR}/WalletLegacy/WalletUtils.h")

set(QwertycoinWallet_LIBS
    Boost::filesystem
    Qwertycoin::Common
    Qwertycoin::Crypto
    Qwertycoin::CryptoNoteCore
    Qwertycoin::Mnemonics
    Qwertycoin::Rpc
    Qwertycoin::Transfers)

add_library(QwertycoinWallet STATIC ${QwertycoinWallet_SOURCES})
add_library(Qwertycoin::Wallet ALIAS QwertycoinWallet)
target_include_directories(QwertycoinWallet PRIVATE ${Qwertycoin_INCLUDE_DIRS})
target_link_libraries(QwertycoinWallet PRIVATE ${QwertycoinWallet_LIBS})

# Qwertycoin::WalletLegacy # TODO: Merge this target into Qwertycoin::Wallet
