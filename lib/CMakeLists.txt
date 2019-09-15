# Generate version.h header
configure_file(
    "${CMAKE_CURRENT_LIST_DIR}/Global/version.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/version.h"
    @ONLY
)
message(STATUS "Generated version.h header: ${CMAKE_CURRENT_BINARY_DIR}/version.h")

set(QwertycoinFramework_INCLUDE_DIRS
    "${Boost_INCLUDE_DIRS}"
    "${Breakpad_INCLUDE_DIRS}"
    "${MINIUPNP_INCLUDE_DIRS}"
    "${SPARSEHASH_INCLUDE_DIRS}"
    "${CMAKE_CURRENT_BINARY_DIR}"
    "${CMAKE_CURRENT_LIST_DIR}"
    "${CMAKE_CURRENT_LIST_DIR}/../include"
)

if(MSVC)
    list(APPEND QwertycoinFramework_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/Platform/Windows")
    list(APPEND QwertycoinFramework_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/Platform/msc")
elseif(APPLE)
    list(APPEND QwertycoinFramework_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/Platform/OSX")
    list(APPEND QwertycoinFramework_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/Platform/Posix")
elseif(ANDROID)
    list(APPEND QwertycoinFramework_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/Platform/Android")
    list(APPEND QwertycoinFramework_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/Platform/Posix")
elseif(FREEBSD)
    list(APPEND QwertycoinFramework_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/Platform/FreeBSD")
    list(APPEND QwertycoinFramework_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/Platform/Posix")
else()
    list(APPEND QwertycoinFramework_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/Platform/Linux")
    list(APPEND QwertycoinFramework_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/Platform/Posix")
endif()

set(QwertycoinFramework_INCLUDE_DIRS ${QwertycoinFramework_INCLUDE_DIRS} PARENT_SCOPE)

# QwertycoinFramework

set(QwertycoinFramework_LIBS
    QwertycoinFramework_BlockchainExplorer
    QwertycoinFramework_Breakpad
    QwertycoinFramework_Common
    QwertycoinFramework_Crypto
    QwertycoinFramework_CryptoNoteCore
    QwertycoinFramework_CryptoNoteProtocol
    QwertycoinFramework_Global
    QwertycoinFramework_Http
    QwertycoinFramework_InProcessNode
    QwertycoinFramework_JsonRpcServer
    QwertycoinFramework_Logging
    QwertycoinFramework_Mnemonics
    QwertycoinFramework_NodeRpcProxy
    QwertycoinFramework_P2p
    QwertycoinFramework_PaymentGate
    QwertycoinFramework_Rpc
    QwertycoinFramework_Serialization
    QwertycoinFramework_System
    QwertycoinFramework_Transfers
    QwertycoinFramework_Wallet
)

add_custom_target(QwertycoinFramework)
add_dependencies(QwertycoinFramework ${QwertycoinFramework_LIBS})

# QwertycoinFramework::BlockchainExplorer

set(QwertycoinFramework_BlockchainExplorer_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/BlockchainExplorer/BlockchainExplorer.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/BlockchainExplorer/BlockchainExplorer.h"
    "${CMAKE_CURRENT_LIST_DIR}/BlockchainExplorer/BlockchainExplorerData.h"
    "${CMAKE_CURRENT_LIST_DIR}/BlockchainExplorer/BlockchainExplorerDataBuilder.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/BlockchainExplorer/BlockchainExplorerDataBuilder.h"
    "${CMAKE_CURRENT_LIST_DIR}/BlockchainExplorer/BlockchainExplorerErrors.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/BlockchainExplorer/BlockchainExplorerErrors.h"
    "${CMAKE_CURRENT_LIST_DIR}/BlockchainExplorer/IBlockchainExplorer.h"
    "${CMAKE_CURRENT_LIST_DIR}/BlockchainExplorer/IBlockchainObserver.h"
)

set(QwertycoinFramework_BlockchainExplorer_LIBS
    QwertycoinFramework::Global
)

add_library(QwertycoinFramework_BlockchainExplorer STATIC ${QwertycoinFramework_BlockchainExplorer_SOURCES})
add_library(QwertycoinFramework::BlockchainExplorer ALIAS QwertycoinFramework_BlockchainExplorer)
target_include_directories(QwertycoinFramework_BlockchainExplorer PRIVATE ${QwertycoinFramework_INCLUDE_DIRS})
target_link_libraries(QwertycoinFramework_BlockchainExplorer PRIVATE ${QwertycoinFramework_BlockchainExplorer_LIBS})

# QwertycoinFramework::Breakpad

set(QwertycoinFramework_Breakpad_SOURCES
    ${Breakpad_SOURCES} # Compile Google Brekapad sources as part of this lib
    "${CMAKE_CURRENT_LIST_DIR}/Breakpad/Breakpad.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Breakpad/Breakpad.h"
)

set(QwertycoinFramework_Breakpad_LIBS
    QwertycoinFramework::Global
)

if(APPLE)
    find_library(CoreFoundationLibrary CoreFoundation)
    find_library(CFNetworkLibrary CFNetwork)
    set(QwertycoinFramework_Breakpad_LIBS
        ${QwertycoinFramework_Breakpad_LIBS}
        ${CoreFoundationLibrary}
        ${CFNetworkLibrary}
    )
endif()

add_library(QwertycoinFramework_Breakpad STATIC ${QwertycoinFramework_Breakpad_SOURCES})
add_library(QwertycoinFramework::Breakpad ALIAS QwertycoinFramework_Breakpad)
target_include_directories(QwertycoinFramework_Breakpad PRIVATE ${QwertycoinFramework_INCLUDE_DIRS})
target_link_libraries(QwertycoinFramework_Breakpad PRIVATE ${QwertycoinFramework_Breakpad_LIBS})

if(WIN32)
    # Fix for "cannot convert argument 1 from 'const wchar_t *' to 'LPCSTR'" and similar issues
    target_compile_definitions(QwertycoinFramework_Breakpad PRIVATE -DUNICODE -D_UNICODE)
endif()

# QwertycoinFramework::Common

set(QwertycoinFramework_Common_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/Common/ArrayRef.h"
    "${CMAKE_CURRENT_LIST_DIR}/Common/ArrayView.h"
    "${CMAKE_CURRENT_LIST_DIR}/Common/Base58.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Common/Base58.h"
    "${CMAKE_CURRENT_LIST_DIR}/Common/BlockingQueue.h"
    "${CMAKE_CURRENT_LIST_DIR}/Common/CommandLine.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Common/CommandLine.h"
    "${CMAKE_CURRENT_LIST_DIR}/Common/ConsoleHandler.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Common/ConsoleHandler.h"
    "${CMAKE_CURRENT_LIST_DIR}/Common/ConsoleTools.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Common/ConsoleTools.h"
    "${CMAKE_CURRENT_LIST_DIR}/Common/DnsTools.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Common/DnsTools.h"
    "${CMAKE_CURRENT_LIST_DIR}/Common/FileMappedVector.h"
    "${CMAKE_CURRENT_LIST_DIR}/Common/IInputStream.h"
    "${CMAKE_CURRENT_LIST_DIR}/Common/IOutputStream.h"
    "${CMAKE_CURRENT_LIST_DIR}/Common/JsonValue.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Common/JsonValue.h"
    "${CMAKE_CURRENT_LIST_DIR}/Common/Lazy.h"
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
    "${CMAKE_CURRENT_LIST_DIR}/Common/static_assert.h"
)

set(QwertycoinFramework_Common_LIBS
    Threads::Threads
    QwertycoinFramework::Crypto
    QwertycoinFramework::Global
)

add_library(QwertycoinFramework_Common STATIC ${QwertycoinFramework_Common_SOURCES})
add_library(QwertycoinFramework::Common ALIAS QwertycoinFramework_Common)
target_include_directories(QwertycoinFramework_Common PRIVATE ${QwertycoinFramework_INCLUDE_DIRS})
target_link_libraries(QwertycoinFramework_Common PRIVATE ${QwertycoinFramework_Common_LIBS})

if(UNIX AND NOT ANDROID)
    target_link_libraries(QwertycoinFramework_Common PUBLIC -lresolv)
endif()

# QwertycoinFramework::Crypto

set(QwertycoinFramework_Crypto_SOURCES
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
    "${CMAKE_CURRENT_LIST_DIR}/crypto/tree-hash.c"
)

add_library(QwertycoinFramework_Crypto STATIC ${QwertycoinFramework_Crypto_SOURCES})
add_library(QwertycoinFramework::Crypto ALIAS QwertycoinFramework_Crypto)
target_include_directories(QwertycoinFramework_Crypto PRIVATE ${QwertycoinFramework_INCLUDE_DIRS})

if(NOT WIN32 AND NOT ARM AND NOT PPC64LE)
    message(STATUS "[QwertycoinFramework::Crypto] AES support enabled.")
    target_compile_options(QwertycoinFramework_Crypto PRIVATE -maes)
elseif(PPC64LE)
    message(STATUS "[QwertycoinFramework::Crypto] AES support disabled.")
endif()

# QwertycoinFramework::CryptoNoteCore

set(QwertycoinFramework_CryptoNoteCore_SOURCES
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
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/IBlock.h"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/IBlockchainStorageObserver.h"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/ICore.h"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/ICoreObserver.h"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/IMinerHandler.h"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/ITimeProvider.h"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/ITransaction.h"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/ITransactionValidator.h"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/ITxPoolObserver.h"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/IntrusiveLinkedList.h"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/MessageQueue.h"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/Miner.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/Miner.h"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/MinerConfig.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/MinerConfig.h"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/OnceInInterval.h"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/SwappedMap.h"
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
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/UpgradeDetector.h"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteCore/VerificationContext.h"
)

set(QwertycoinFramework_CryptoNoteCore_LIBS
    Boost::filesystem
    Boost::program_options
    sparsehash::sparsehash
    QwertycoinFramework::BlockchainExplorer
    QwertycoinFramework::Common
    QwertycoinFramework::Crypto
    QwertycoinFramework::Global
    QwertycoinFramework::Logging
    QwertycoinFramework::Serialization
)

add_library(QwertycoinFramework_CryptoNoteCore STATIC ${QwertycoinFramework_CryptoNoteCore_SOURCES})
add_library(QwertycoinFramework::CryptoNoteCore ALIAS QwertycoinFramework_CryptoNoteCore)
target_include_directories(QwertycoinFramework_CryptoNoteCore PRIVATE ${QwertycoinFramework_INCLUDE_DIRS})
target_link_libraries(QwertycoinFramework_CryptoNoteCore PRIVATE ${QwertycoinFramework_CryptoNoteCore_LIBS})

# QwertycoinFramework::CryptoNoteProtocol

set(QwertycoinFramework_CryptoNoteProtocol_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteProtocol/CryptoNoteProtocolDefinitions.h"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteProtocol/CryptoNoteProtocolHandler.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteProtocol/CryptoNoteProtocolHandler.h"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteProtocol/CryptoNoteProtocolHandlerCommon.h"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteProtocol/ICryptoNoteProtocolObserver.h"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoNoteProtocol/ICryptoNoteProtocolQuery.h"
)

set(QwertycoinFramework_CryptoNoteProtocol_LIBS
    QwertycoinFramework::Global
)

add_library(QwertycoinFramework_CryptoNoteProtocol STATIC ${QwertycoinFramework_CryptoNoteProtocol_SOURCES})
add_library(QwertycoinFramework::CryptoNoteProtocol ALIAS QwertycoinFramework_CryptoNoteProtocol)
target_include_directories(QwertycoinFramework_CryptoNoteProtocol PRIVATE ${QwertycoinFramework_INCLUDE_DIRS})
target_link_libraries(QwertycoinFramework_CryptoNoteProtocol PRIVATE ${QwertycoinFramework_CryptoNoteProtocol_LIBS})

# QwertycoinFramework::Global

set(QwertycoinFramework_Global_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/Global/Checkpoints.h"
    "${CMAKE_CURRENT_LIST_DIR}/Global/Constants.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Global/Constants.h"
    "${CMAKE_CURRENT_LIST_DIR}/Global/CryptoNoteConfig.h"
)

add_library(QwertycoinFramework_Global STATIC ${QwertycoinFramework_Global_SOURCES})
add_library(QwertycoinFramework::Global ALIAS QwertycoinFramework_Global)
target_include_directories(QwertycoinFramework_Global PRIVATE ${QwertycoinFramework_INCLUDE_DIRS})

# QwertycoinFramework::Http

set(QwertycoinFramework_Http_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/Http/HttpParser.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Http/HttpParser.h"
    "${CMAKE_CURRENT_LIST_DIR}/Http/HttpParserErrorCodes.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Http/HttpParserErrorCodes.h"
    "${CMAKE_CURRENT_LIST_DIR}/Http/HttpRequest.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Http/HttpRequest.h"
    "${CMAKE_CURRENT_LIST_DIR}/Http/HttpResponse.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Http/HttpResponse.h"
)

add_library(QwertycoinFramework_Http STATIC ${QwertycoinFramework_Http_SOURCES})
add_library(QwertycoinFramework::Http ALIAS QwertycoinFramework_Http)
target_include_directories(QwertycoinFramework_Http PRIVATE ${QwertycoinFramework_INCLUDE_DIRS})

# QwertycoinFramework::InProcessNode

set(QwertycoinFramework_InProcessNode_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/InProcessNode/InProcessNode.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/InProcessNode/InProcessNode.h"
    "${CMAKE_CURRENT_LIST_DIR}/InProcessNode/InProcessNodeErrors.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/InProcessNode/InProcessNodeErrors.h"
)

set(QwertycoinFramework_InProcessNode_LIBS
    QwertycoinFramework::Global
)

add_library(QwertycoinFramework_InProcessNode STATIC ${QwertycoinFramework_InProcessNode_SOURCES})
add_library(QwertycoinFramework::InProcessNode ALIAS QwertycoinFramework_InProcessNode)
target_include_directories(QwertycoinFramework_InProcessNode PRIVATE ${QwertycoinFramework_INCLUDE_DIRS})
target_link_libraries(QwertycoinFramework_InProcessNode PRIVATE ${QwertycoinFramework_InProcessNode_LIBS})

# QwertycoinFramework::JsonRpcServer

set(QwertycoinFramework_JsonRpcServer_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/JsonRpcServer/JsonRpcServer.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/JsonRpcServer/JsonRpcServer.h"
)

add_library(QwertycoinFramework_JsonRpcServer STATIC ${QwertycoinFramework_JsonRpcServer_SOURCES})
add_library(QwertycoinFramework::JsonRpcServer ALIAS QwertycoinFramework_JsonRpcServer)
target_include_directories(QwertycoinFramework_JsonRpcServer PRIVATE ${QwertycoinFramework_INCLUDE_DIRS})

# QwertycoinFramework::Logging

set(QwertycoinFramework_Logging_SOURCES
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
    "${CMAKE_CURRENT_LIST_DIR}/Logging/StreamLogger.h"
)

add_library(QwertycoinFramework_Logging STATIC ${QwertycoinFramework_Logging_SOURCES})
add_library(QwertycoinFramework::Logging ALIAS QwertycoinFramework_Logging)
target_include_directories(QwertycoinFramework_Logging PRIVATE ${QwertycoinFramework_INCLUDE_DIRS})

# QwertycoinFramework::Mnemonics

set(QwertycoinFramework_Mnemonics_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/Mnemonics/chinese_simplified.h"
    "${CMAKE_CURRENT_LIST_DIR}/Mnemonics/dutch.h"
    "${CMAKE_CURRENT_LIST_DIR}/Mnemonics/electrum-words.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Mnemonics/electrum-words.h"
    "${CMAKE_CURRENT_LIST_DIR}/Mnemonics/english.h"
    "${CMAKE_CURRENT_LIST_DIR}/Mnemonics/french.h"
    "${CMAKE_CURRENT_LIST_DIR}/Mnemonics/german.h"
    "${CMAKE_CURRENT_LIST_DIR}/Mnemonics/italian.h"
    "${CMAKE_CURRENT_LIST_DIR}/Mnemonics/japanese.h"
    "${CMAKE_CURRENT_LIST_DIR}/Mnemonics/language_base.h"
    "${CMAKE_CURRENT_LIST_DIR}/Mnemonics/polish.h"
    "${CMAKE_CURRENT_LIST_DIR}/Mnemonics/portuguese.h"
    "${CMAKE_CURRENT_LIST_DIR}/Mnemonics/russian.h"
    "${CMAKE_CURRENT_LIST_DIR}/Mnemonics/singleton.h"
    "${CMAKE_CURRENT_LIST_DIR}/Mnemonics/spanish.h"
    "${CMAKE_CURRENT_LIST_DIR}/Mnemonics/ukrainian.h"
)

add_library(QwertycoinFramework_Mnemonics STATIC ${QwertycoinFramework_Mnemonics_SOURCES})
add_library(QwertycoinFramework::Mnemonics ALIAS QwertycoinFramework_Mnemonics)
target_include_directories(QwertycoinFramework_Mnemonics PRIVATE ${QwertycoinFramework_INCLUDE_DIRS})

# QwertycoinFramework::NodeRpcProxy

set(QwertycoinFramework_NodeRpcProxy_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/NodeRpcProxy/NodeErrors.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/NodeRpcProxy/NodeErrors.h"
    "${CMAKE_CURRENT_LIST_DIR}/NodeRpcProxy/NodeRpcProxy.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/NodeRpcProxy/NodeRpcProxy.h"
)

set(QwertycoinFramework_NodeRpcProxy_LIBS
    QwertycoinFramework::Global
    QwertycoinFramework::Http
    QwertycoinFramework::Rpc
    QwertycoinFramework::System
)

add_library(QwertycoinFramework_NodeRpcProxy STATIC ${QwertycoinFramework_NodeRpcProxy_SOURCES})
add_library(QwertycoinFramework::NodeRpcProxy ALIAS QwertycoinFramework_NodeRpcProxy)
target_include_directories(QwertycoinFramework_NodeRpcProxy PRIVATE ${QwertycoinFramework_INCLUDE_DIRS})
target_link_libraries(QwertycoinFramework_NodeRpcProxy PRIVATE ${QwertycoinFramework_NodeRpcProxy_LIBS})

# QwertycoinFramework::P2p

set(QwertycoinFramework_P2p_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/P2p/ConnectionContext.h"
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
    "${CMAKE_CURRENT_LIST_DIR}/P2p/P2pInterfaces.h"
    "${CMAKE_CURRENT_LIST_DIR}/P2p/P2pNetworks.h"
    "${CMAKE_CURRENT_LIST_DIR}/P2p/P2pNode.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/P2p/P2pNode.h"
    "${CMAKE_CURRENT_LIST_DIR}/P2p/P2pNodeConfig.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/P2p/P2pNodeConfig.h"
    "${CMAKE_CURRENT_LIST_DIR}/P2p/P2pProtocolDefinitions.h"
    "${CMAKE_CURRENT_LIST_DIR}/P2p/P2pProtocolTypes.h"
    "${CMAKE_CURRENT_LIST_DIR}/P2p/PeerListManager.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/P2p/PeerListManager.h"
)

set(QwertycoinFramework_P2p_LIBS
    MiniUPnP::miniupnpc
    QwertycoinFramework::CryptoNoteProtocol
    QwertycoinFramework::Global
)

if(WIN32 AND MSVC)
    list(APPEND QwertycoinFramework_P2p_LIBS Bcrypt)
endif()

add_library(QwertycoinFramework_P2p STATIC ${QwertycoinFramework_P2p_SOURCES})
add_library(QwertycoinFramework::P2p ALIAS QwertycoinFramework_P2p)
target_include_directories(QwertycoinFramework_P2p PRIVATE ${QwertycoinFramework_INCLUDE_DIRS})
target_link_libraries(QwertycoinFramework_P2p PRIVATE ${QwertycoinFramework_P2p_LIBS})

# QwertycoinFramework::PaymentGate

set(QwertycoinFramework_PaymentGate_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/PaymentGate/NodeFactory.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/PaymentGate/NodeFactory.h"
    "${CMAKE_CURRENT_LIST_DIR}/PaymentGate/PaymentServiceJsonRpcMessages.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/PaymentGate/PaymentServiceJsonRpcMessages.h"
    "${CMAKE_CURRENT_LIST_DIR}/PaymentGate/PaymentServiceJsonRpcServer.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/PaymentGate/PaymentServiceJsonRpcServer.h"
    "${CMAKE_CURRENT_LIST_DIR}/PaymentGate/WalletService.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/PaymentGate/WalletService.h"
    "${CMAKE_CURRENT_LIST_DIR}/PaymentGate/WalletServiceErrorCategory.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/PaymentGate/WalletServiceErrorCategory.h"
)

set(QwertycoinFramework_PaymentGate_LIBS
    QwertycoinFramework::JsonRpcServer
    QwertycoinFramework::NodeRpcProxy
    QwertycoinFramework::System
    QwertycoinFramework::Wallet
)

add_library(QwertycoinFramework_PaymentGate STATIC ${QwertycoinFramework_PaymentGate_SOURCES})
add_library(QwertycoinFramework::PaymentGate ALIAS QwertycoinFramework_PaymentGate)
target_include_directories(QwertycoinFramework_PaymentGate PRIVATE ${QwertycoinFramework_INCLUDE_DIRS})
target_link_libraries(QwertycoinFramework_PaymentGate PRIVATE ${QwertycoinFramework_PaymentGate_LIBS})

# QwertycoinFramework::Platform # TODO: Merge this target into QwertycoinFramework::System

# QwertycoinFramework::Rpc

set(QwertycoinFramework_Rpc_SOURCES
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
    "${CMAKE_CURRENT_LIST_DIR}/Rpc/RpcServerConfig.h"
)

set(QwertycoinFramework_Rpc_LIBS
    QwertycoinFramework::BlockchainExplorer
    QwertycoinFramework::CryptoNoteCore
    QwertycoinFramework::Global
    QwertycoinFramework::P2p
    QwertycoinFramework::PaymentGate
    QwertycoinFramework::Serialization
    QwertycoinFramework::System
    QwertycoinFramework::Http
)

add_library(QwertycoinFramework_Rpc STATIC ${QwertycoinFramework_Rpc_SOURCES})
add_library(QwertycoinFramework::Rpc ALIAS QwertycoinFramework_Rpc)
target_include_directories(QwertycoinFramework_Rpc PRIVATE ${QwertycoinFramework_INCLUDE_DIRS})
target_link_libraries(QwertycoinFramework_Rpc PRIVATE ${QwertycoinFramework_Rpc_LIBS})

# QwertycoinFramework::Serialization

set(QwertycoinFramework_Serialization_SOURCES
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
    "${CMAKE_CURRENT_LIST_DIR}/Serialization/MemoryStream.h"
    "${CMAKE_CURRENT_LIST_DIR}/Serialization/SerializationOverloads.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Serialization/SerializationOverloads.h"
    "${CMAKE_CURRENT_LIST_DIR}/Serialization/SerializationTools.h"
)

add_library(QwertycoinFramework_Serialization STATIC ${QwertycoinFramework_Serialization_SOURCES})
add_library(QwertycoinFramework::Serialization ALIAS QwertycoinFramework_Serialization)
target_include_directories(QwertycoinFramework_Serialization PRIVATE ${QwertycoinFramework_INCLUDE_DIRS})
target_link_libraries(QwertycoinFramework_Serialization PRIVATE QwertycoinFramework::Common)

# QwertycoinFramework::System

set(QwertycoinFramework_System_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/System/Context.h"
    "${CMAKE_CURRENT_LIST_DIR}/System/ContextGroup.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/System/ContextGroup.h"
    "${CMAKE_CURRENT_LIST_DIR}/System/ContextGroupTimeout.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/System/ContextGroupTimeout.h"
    "${CMAKE_CURRENT_LIST_DIR}/System/Event.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/System/Event.h"
    "${CMAKE_CURRENT_LIST_DIR}/System/EventLock.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/System/EventLock.h"
    "${CMAKE_CURRENT_LIST_DIR}/System/InterruptedException.h"
    "${CMAKE_CURRENT_LIST_DIR}/System/Ipv4Address.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/System/Ipv4Address.h"
    "${CMAKE_CURRENT_LIST_DIR}/System/OperationTimeout.h"
    "${CMAKE_CURRENT_LIST_DIR}/System/RemoteContext.h"
    "${CMAKE_CURRENT_LIST_DIR}/System/RemoteEventLock.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/System/RemoteEventLock.h"
    "${CMAKE_CURRENT_LIST_DIR}/System/TcpStream.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/System/TcpStream.h"
)

if(MSVC)
    list(APPEND QwertycoinFramework_System_SOURCES
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
        "${CMAKE_CURRENT_LIST_DIR}/Platform/Windows/System/Timer.h"
    )
elseif(APPLE)
    list(APPEND QwertycoinFramework_System_SOURCES
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
        "${CMAKE_CURRENT_LIST_DIR}/Platform/Posix/System/MemoryMappedFile.h"
    )
elseif(ANDROID)
    list(APPEND QwertycoinFramework_System_SOURCES
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
        "${CMAKE_CURRENT_LIST_DIR}/Platform/Posix/System/MemoryMappedFile.h"
    )
elseif(FREEBSD)
    list(APPEND QwertycoinFramework_System_SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/Platform/FreeBSD/System/Context.h"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/FreeBSD/System/Dispatcher.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/FreeBSD/System/Dispatcher.h"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/FreeBSD/System/ErrorMessage.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/FreeBSD/System/ErrorMessage.h"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/FreeBSD/System/Future.h"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/FreeBSD/System/Ipv4Resolver.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/FreeBSD/System/Ipv4Resolver.h"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/FreeBSD/System/TcpConnection.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/FreeBSD/System/TcpConnection.h"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/FreeBSD/System/TcpConnector.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/FreeBSD/System/TcpConnector.h"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/FreeBSD/System/TcpListener.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/FreeBSD/System/TcpListener.h"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/FreeBSD/System/Timer.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/FreeBSD/System/Timer.h"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/Posix/System/MemoryMappedFile.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Platform/Posix/System/MemoryMappedFile.h"
    )
else()
    list(APPEND QwertycoinFramework_System_SOURCES
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
        "${CMAKE_CURRENT_LIST_DIR}/Platform/Posix/System/MemoryMappedFile.h"
    )
endif()

add_library(QwertycoinFramework_System STATIC ${QwertycoinFramework_System_SOURCES})
add_library(QwertycoinFramework::System ALIAS QwertycoinFramework_System)
target_include_directories(QwertycoinFramework_System PRIVATE ${QwertycoinFramework_INCLUDE_DIRS})

if(WIN32)
    target_link_libraries(QwertycoinFramework_System PRIVATE ws2_32)
endif()

# QwertycoinFramework::Transfers

set(QwertycoinFramework_Transfers_SOURCES
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
    "${CMAKE_CURRENT_LIST_DIR}/Transfers/TypeHelpers.h"
)

set(QwertycoinFramework_Transfers_LIBS
    QwertycoinFramework::CryptoNoteCore
    QwertycoinFramework::Global
)

add_library(QwertycoinFramework_Transfers STATIC ${QwertycoinFramework_Transfers_SOURCES})
add_library(QwertycoinFramework::Transfers ALIAS QwertycoinFramework_Transfers)
target_include_directories(QwertycoinFramework_Transfers PRIVATE ${QwertycoinFramework_INCLUDE_DIRS})
target_link_libraries(QwertycoinFramework_Transfers PRIVATE ${QwertycoinFramework_Transfers_LIBS})

# QwertycoinFramework::Wallet

set(QwertycoinFramework_Wallet_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/Wallet/IFusionManager.h"
    "${CMAKE_CURRENT_LIST_DIR}/Wallet/IWallet.h"
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
    "${CMAKE_CURRENT_LIST_DIR}/WalletLegacy/IWalletLegacy.h"
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
    "${CMAKE_CURRENT_LIST_DIR}/WalletLegacy/WalletUtils.h"
)

set(QwertycoinFramework_Wallet_LIBS
    Boost::filesystem
    QwertycoinFramework::Common
    QwertycoinFramework::Crypto
    QwertycoinFramework::CryptoNoteCore
    QwertycoinFramework::Global
    QwertycoinFramework::Logging
    QwertycoinFramework::Mnemonics
    QwertycoinFramework::Rpc
    QwertycoinFramework::Transfers
)

add_library(QwertycoinFramework_Wallet STATIC ${QwertycoinFramework_Wallet_SOURCES})
add_library(QwertycoinFramework::Wallet ALIAS QwertycoinFramework_Wallet)
target_include_directories(QwertycoinFramework_Wallet PRIVATE ${QwertycoinFramework_INCLUDE_DIRS})
target_link_libraries(QwertycoinFramework_Wallet PRIVATE ${QwertycoinFramework_Wallet_LIBS})

# QwertycoinFramework::WalletLegacy # TODO: Merge this target into QwertycoinFramework::Wallet

if(BUILD_WITH_COVERAGE AND (${CMAKE_BUILD_TYPE} MATCHES Debug))
    message(STATUS "Included code-coverage settings.")
    message(STATUS "NOTE: Code-coverage is enabled by default when CMAKE_BUILD_TYPE=Debug.")

    # Enable code-coverage settings for each library
    foreach(lib ${QwertycoinFramework_LIBS})
        target_link_libraries(${lib} PUBLIC codecov)
    endforeach()
endif()

install(TARGETS
            QwertycoinFramework_BlockchainExplorer
            QwertycoinFramework_Common
            QwertycoinFramework_Crypto
            QwertycoinFramework_CryptoNoteCore
            QwertycoinFramework_CryptoNoteProtocol
            QwertycoinFramework_Http
            QwertycoinFramework_InProcessNode
            QwertycoinFramework_JsonRpcServer
            QwertycoinFramework_Logging
            QwertycoinFramework_Mnemonics
            QwertycoinFramework_NodeRpcProxy
            QwertycoinFramework_P2p
            QwertycoinFramework_PaymentGate
            QwertycoinFramework_Rpc
            QwertycoinFramework_Serialization
            QwertycoinFramework_System
            QwertycoinFramework_Transfers
            QwertycoinFramework_Wallet
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib/static
)
