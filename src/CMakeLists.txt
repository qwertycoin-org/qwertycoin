add_definitions(-DSTATICLIB)

file(GLOB_RECURSE BlockchainExplorer BlockchainExplorer/*)
file(GLOB_RECURSE Common Common/*)
file(GLOB_RECURSE ConnectivityTool ConnectivityTool/*)
file(GLOB_RECURSE Crypto crypto/*)
file(GLOB_RECURSE CryptoNoteCore CryptoNoteCore/* CryptoNoteConfig.h)
file(GLOB_RECURSE CryptoNoteProtocol CryptoNoteProtocol/*)
file(GLOB_RECURSE Daemon Daemon/*)
file(GLOB_RECURSE Http HTTP/*)
file(GLOB_RECURSE InProcessNode InProcessNode/*)
file(GLOB_RECURSE Logging Logging/*)
file(GLOB_RECURSE NodeRpcProxy NodeRpcProxy/*)
file(GLOB_RECURSE P2p P2p/*)
file(GLOB_RECURSE Rpc Rpc/*)
file(GLOB_RECURSE Serialization Serialization/*)
file(GLOB_RECURSE SimpleWallet SimpleWallet/*)
file(GLOB_RECURSE Mnemonics mnemonics/*)
if(MSVC)
file(GLOB_RECURSE System System/* Platform/Windows/System/*)
elseif(APPLE)
file(GLOB_RECURSE System System/* Platform/OSX/System/* Platform/Posix/System/*)
elseif(ANDROID)
file(GLOB_RECURSE System System/* Platform/Android/System/* Platform/Posix/System/*)
else()
file(GLOB_RECURSE System System/* Platform/Linux/System/* Platform/Posix/System/*)
endif()
file(GLOB_RECURSE Transfers Transfers/*)
file(GLOB_RECURSE Wallet Wallet/*)
file(GLOB_RECURSE WalletLegacy WalletLegacy/*)

file(GLOB_RECURSE JsonRpcServer JsonRpcServer/*)

file(GLOB_RECURSE PaymentGate PaymentGate/*)
file(GLOB_RECURSE PaymentGateService PaymentGateService/*)
file(GLOB_RECURSE Miner Miner/*)

source_group("" FILES $${Common} ${ConnectivityTool} ${Crypto} ${CryptoNoteCore} ${CryptoNoteProtocol} ${Daemon} ${JsonRpcServer} ${Http} ${Logging} ${NodeRpcProxy} ${P2p} ${Rpc} ${Serialization} ${SimpleWallet} ${System} ${Transfers} ${Wallet} ${WalletLegacy} ${Mnemonics})

add_library(Common ${Common})
add_library(Crypto ${Crypto})
add_library(CryptoNoteCore ${CryptoNoteCore})
add_library(BlockchainExplorer ${BlockchainExplorer})
add_library(Http ${Http})
add_library(InProcessNode ${InProcessNode})
add_library(Logging ${Logging})
add_library(NodeRpcProxy ${NodeRpcProxy})
add_library(Rpc ${Rpc})
add_library(P2P ${CryptoNoteProtocol} ${P2p})
add_library(Serialization ${Serialization})
add_library(System ${System})
add_library(Transfers ${Transfers})
add_library(Wallet ${Wallet} ${WalletLegacy})
add_library(PaymentGate ${PaymentGate})
add_library(JsonRpcServer ${JsonRpcServer})
add_library(Mnemonics ${Mnemonics})

if(MSVC)
  set(DAEMON_SOURCES_OS
    BinaryInfo/daemon.rc
  )
  set(SIMPLE_WALLET_SOURCES_OS
    BinaryInfo/simplewallet.rc
  )
  set(MINER_SOURCES_OS
    BinaryInfo/miner.rc
  )
  set(PG_SOURCES_OS
    BinaryInfo/walletd.rc
  )
  set(CONNECTIVITY_TOOLS_SOURCES_OS
    BinaryInfo/connectivity_tool.rc
  )
endif()

add_executable(Daemon ${Daemon} ${DAEMON_SOURCES_OS})
add_executable(SimpleWallet ${SimpleWallet} ${SIMPLE_WALLET_SOURCES_OS})
add_executable(PaymentGateService ${PaymentGateService} ${PG_SOURCES_OS})
add_executable(ConnectivityTool ${ConnectivityTool} ${CONNECTIVITY_TOOLS_SOURCES_OS})
add_executable(Miner ${Miner} ${MINER_SOURCES_OS})

if(MSVC)
  target_link_libraries(System ws2_32)
  target_link_libraries(Daemon Rpcrt4)
  target_link_libraries(PaymentGateService Rpcrt4)
endif ()

# Add the dependencies we need
target_link_libraries(ConnectivityTool CryptoNoteCore Logging Crypto P2P Rpc Http Serialization Common System ${Boost_LIBRARIES} ${EXTRA_LIBRARIES})
target_link_libraries(Daemon Rpc CryptoNoteCore P2P System Http Logging Common Crypto upnpc-static BlockchainExplorer ${Boost_LIBRARIES} Serialization ${EXTRA_LIBRARIES})
target_link_libraries(SimpleWallet Wallet NodeRpcProxy Transfers Rpc Http CryptoNoteCore System Logging Common Crypto ${Boost_LIBRARIES} Serialization Mnemonics ${EXTRA_LIBRARIES})
target_link_libraries(PaymentGateService PaymentGate JsonRpcServer Wallet NodeRpcProxy Transfers CryptoNoteCore Crypto P2P Rpc Http System Logging Common InProcessNode upnpc-static BlockchainExplorer ${Boost_LIBRARIES} Serialization ${EXTRA_LIBRARIES})
target_link_libraries(Miner CryptoNoteCore Rpc System Http Logging Common Crypto ${Boost_LIBRARIES} Serialization ${EXTRA_LIBRARIES})

if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux" OR APPLE AND NOT ANDROID)
  target_link_libraries(SimpleWallet -lresolv)
  target_link_libraries(Daemon -lresolv)
  target_link_libraries(PaymentGateService -lresolv)
endif ()

add_dependencies(Rpc version)

add_dependencies(ConnectivityTool version)
add_dependencies(Daemon version)
add_dependencies(SimpleWallet version)
add_dependencies(PaymentGateService version)
add_dependencies(P2P version)

set_property(TARGET ConnectivityTool PROPERTY OUTPUT_NAME "connectivity_tool")
set_property(TARGET Daemon PROPERTY OUTPUT_NAME "Qwertycoind")
set_property(TARGET SimpleWallet PROPERTY OUTPUT_NAME "simplewallet")
set_property(TARGET PaymentGateService PROPERTY OUTPUT_NAME "walletd")
set_property(TARGET Miner PROPERTY OUTPUT_NAME "miner")