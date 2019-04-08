enable_testing()

set(QwertycoinTests_INCLUDE_DIRS
    ${QwertycoinFramework_INCLUDE_DIRS}
    "${CMAKE_CURRENT_LIST_DIR}")

# QwertycoinTests

add_custom_target(QwertycoinTests)
add_dependencies(QwertycoinTests
    #QwertycoinTests::CoreTests
    QwertycoinTests::CryptoTests
    QwertycoinTests::DifficultyTests
    QwertycoinTests::HashTargetTests
    QwertycoinTests::HashTests
    QwertycoinTests::IntegrationTests
    QwertycoinTests::NodeRpcProxyTests
    QwertycoinTests::PerformanceTests
    QwertycoinTests::SystemTests
    QwertycoinTests::TransfersTests
    #QwertycoinTests::UnitTests
)

# QwertycoinTests::CoreTests

set(QwertycoinTests_CoreTests_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/CoreTests/AccountBoostSerialization.h"
    "${CMAKE_CURRENT_LIST_DIR}/CoreTests/BlockReward.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/CoreTests/BlockReward.h"
    "${CMAKE_CURRENT_LIST_DIR}/CoreTests/BlockValidation.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/CoreTests/BlockValidation.h"
    "${CMAKE_CURRENT_LIST_DIR}/CoreTests/BoostSerializationHelper.h"
    "${CMAKE_CURRENT_LIST_DIR}/CoreTests/ChainSplit1.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/CoreTests/ChainSplit1.h"
    "${CMAKE_CURRENT_LIST_DIR}/CoreTests/ChainSwitch1.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/CoreTests/ChainSwitch1.h"
    "${CMAKE_CURRENT_LIST_DIR}/CoreTests/Chaingen.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/CoreTests/Chaingen.h"
    "${CMAKE_CURRENT_LIST_DIR}/CoreTests/Chaingen001.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/CoreTests/Chaingen001.h"
    "${CMAKE_CURRENT_LIST_DIR}/CoreTests/ChaingenMain.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/CoreTests/CryptoNoteBoostSerialization.h"
    "${CMAKE_CURRENT_LIST_DIR}/CoreTests/DoubleSpend.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/CoreTests/DoubleSpend.h"
    "${CMAKE_CURRENT_LIST_DIR}/CoreTests/IntegerOverflow.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/CoreTests/IntegerOverflow.h"
    "${CMAKE_CURRENT_LIST_DIR}/CoreTests/RandomOuts.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/CoreTests/RandomOuts.h"
    "${CMAKE_CURRENT_LIST_DIR}/CoreTests/RingSignature.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/CoreTests/RingSignature.h"
    "${CMAKE_CURRENT_LIST_DIR}/CoreTests/TestGenerator.h"
    "${CMAKE_CURRENT_LIST_DIR}/CoreTests/TransactionBuilder.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/CoreTests/TransactionBuilder.h"
    "${CMAKE_CURRENT_LIST_DIR}/CoreTests/TransactionTests.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/CoreTests/TransactionTests.h"
    "${CMAKE_CURRENT_LIST_DIR}/CoreTests/TransactionValidation.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/CoreTests/TransactionValidation.h"
    "${CMAKE_CURRENT_LIST_DIR}/CoreTests/UnorderedContainersBoostSerialization.h"
    "${CMAKE_CURRENT_LIST_DIR}/CoreTests/Upgrade.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/CoreTests/Upgrade.h"
    "${CMAKE_CURRENT_LIST_DIR}/CoreTests/double_spend.inl"
)

set(QwertycoinTests_CoreTests_LIBS "")

add_executable(QwertycoinTests_CoreTests ${QwertycoinTests_CoreTests_SOURCES})
add_executable(QwertycoinTests::CoreTests ALIAS QwertycoinTests_CoreTests)
target_include_directories(QwertycoinTests_CoreTests PRIVATE ${QwertycoinTests_INCLUDE_DIRS})
target_link_libraries(QwertycoinTests_CoreTests PRIVATE ${QwertycoinTests_CoreTests_LIBS})
set_target_properties(QwertycoinTests_CoreTests PROPERTIES
    OUTPUT_NAME "core_tests"
    EXCLUDE_FROM_ALL TRUE
    EXCLUDE_FROM_DEFAULT_BUILD TRUE)

add_test(NAME CoreTests
         COMMAND core_tests --generate_and_play_test_data)

# QwertycoinTests::CryptoTests

set(QwertycoinTests_CryptoTests_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/CryptoTests/crypto-ops-data.c"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoTests/crypto-ops.c"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoTests/crypto-tests.h"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoTests/crypto.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoTests/hash.c"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoTests/main.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/CryptoTests/random.c"
)

set(QwertycoinTests_CryptoTests_LIBS "")

add_executable(QwertycoinTests_CryptoTests ${QwertycoinTests_CryptoTests_SOURCES})
add_executable(QwertycoinTests::CryptoTests ALIAS QwertycoinTests_CryptoTests)
target_include_directories(QwertycoinTests_CryptoTests PRIVATE ${QwertycoinTests_INCLUDE_DIRS})
target_link_libraries(QwertycoinTests_CryptoTests PRIVATE ${QwertycoinTests_CryptoTests_LIBS})
set_target_properties(QwertycoinTests_CryptoTests PROPERTIES OUTPUT_NAME "crypto_tests")

add_test(NAME CryptoTests
         COMMAND crypto_tests ${CMAKE_CURRENT_LIST_DIR}/CryptoTests/tests.txt)

# QwertycoinTests::Data (skipped, not a library or executable target)

# QwertycoinTests::DifficultyTests

set(QwertycoinTests_DifficultyTests_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/DifficultyTests/Difficulty.cpp"
)

set(QwertycoinTests_DifficultyTests_LIBS
    QwertycoinFramework::Logging
    QwertycoinFramework::CryptoNoteCore
)

add_executable(QwertycoinTests_DifficultyTests ${QwertycoinTests_DifficultyTests_SOURCES})
add_executable(QwertycoinTests::DifficultyTests ALIAS QwertycoinTests_DifficultyTests)
target_include_directories(QwertycoinTests_DifficultyTests PRIVATE ${QwertycoinTests_INCLUDE_DIRS})
target_link_libraries(QwertycoinTests_DifficultyTests PRIVATE ${QwertycoinTests_DifficultyTests_LIBS})
set_target_properties(QwertycoinTests_DifficultyTests PROPERTIES OUTPUT_NAME "difficulty_tests")

add_test(NAME DifficultyTests
         COMMAND difficulty_tests ${CMAKE_CURRENT_LIST_DIR}/DifficultyTests/data.txt)

# QwertycoinTests::HashTargetTests

set(QwertycoinTests_HashTargetTests_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/HashTargetTests/HashTarget.cpp"
)

set(QwertycoinTests_HashTargetTests_LIBS
    QwertycoinFramework::CryptoNoteCore
)

add_executable(QwertycoinTests_HashTargetTests ${QwertycoinTests_HashTargetTests_SOURCES})
add_executable(QwertycoinTests::HashTargetTests ALIAS QwertycoinTests_HashTargetTests)
target_include_directories(QwertycoinTests_HashTargetTests PRIVATE ${QwertycoinTests_INCLUDE_DIRS})
target_link_libraries(QwertycoinTests_HashTargetTests PRIVATE ${QwertycoinTests_HashTargetTests_LIBS})
set_target_properties(QwertycoinTests_HashTargetTests PROPERTIES OUTPUT_NAME "hash_target_tests")

add_test(NAME HashTargetTests
         COMMAND hash_target_tests)

# QwertycoinTests::HashTests

set(QwertycoinTests_HashTests_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/HashTests/main.cpp"
)

set(QwertycoinTests_HashTests_LIBS
    QwertycoinFramework::Crypto
)

add_executable(QwertycoinTests_HashTests ${QwertycoinTests_HashTests_SOURCES})
add_executable(QwertycoinTests::HashTests ALIAS QwertycoinTests_HashTests)
target_include_directories(QwertycoinTests_HashTests PRIVATE ${QwertycoinTests_INCLUDE_DIRS})
target_link_libraries(QwertycoinTests_HashTests PRIVATE ${QwertycoinTests_HashTests_LIBS})
set_target_properties(QwertycoinTests_HashTests PROPERTIES OUTPUT_NAME "hash_tests")

foreach(hash IN ITEMS extra-blake extra-groestl extra-jh extra-skein fast slow tree)
    add_test(NAME HashTests-${hash}
             COMMAND hash_tests ${hash} ${CMAKE_CURRENT_LIST_DIR}/HashTests/tests-${hash}.txt)
endforeach()

# QwertycoinTests::IntegrationTestLib

set(QwertycoinTests_IntegrationTestLib_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/IntegrationTestLib/BaseFunctionalTests.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/IntegrationTestLib/BaseFunctionalTests.h"
    "${CMAKE_CURRENT_LIST_DIR}/IntegrationTestLib/InProcTestNode.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/IntegrationTestLib/InProcTestNode.h"
    "${CMAKE_CURRENT_LIST_DIR}/IntegrationTestLib/Logger.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/IntegrationTestLib/Logger.h"
    "${CMAKE_CURRENT_LIST_DIR}/IntegrationTestLib/NetworkConfiguration.h"
    "${CMAKE_CURRENT_LIST_DIR}/IntegrationTestLib/NodeCallback.h"
    "${CMAKE_CURRENT_LIST_DIR}/IntegrationTestLib/NodeObserver.h"
    "${CMAKE_CURRENT_LIST_DIR}/IntegrationTestLib/ObservableValue.h"
    "${CMAKE_CURRENT_LIST_DIR}/IntegrationTestLib/Process.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/IntegrationTestLib/Process.h"
    "${CMAKE_CURRENT_LIST_DIR}/IntegrationTestLib/RPCTestNode.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/IntegrationTestLib/RPCTestNode.h"
    "${CMAKE_CURRENT_LIST_DIR}/IntegrationTestLib/TestNetwork.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/IntegrationTestLib/TestNetwork.h"
    "${CMAKE_CURRENT_LIST_DIR}/IntegrationTestLib/TestNode.h"
    "${CMAKE_CURRENT_LIST_DIR}/IntegrationTestLib/TestWalletLegacy.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/IntegrationTestLib/TestWalletLegacy.h"
)

set(QwertycoinTests_IntegrationTestLib_LIBS
    QwertycoinFramework::InProcessNode
)

add_library(QwertycoinTests_IntegrationTestLib ${QwertycoinTests_IntegrationTestLib_SOURCES})
add_library(QwertycoinTests::IntegrationTestLib ALIAS QwertycoinTests_IntegrationTestLib)
target_include_directories(QwertycoinTests_IntegrationTestLib PRIVATE ${QwertycoinTests_INCLUDE_DIRS})
target_link_libraries(QwertycoinTests_IntegrationTestLib PRIVATE ${QwertycoinTests_IntegrationTestLib_LIBS})

# QwertycoinTests::IntegrationTests

set(QwertycoinTests_IntegrationTests_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/IntegrationTests/BaseTests.h"
    "${CMAKE_CURRENT_LIST_DIR}/IntegrationTests/IntegrationTests.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/IntegrationTests/MultiVersion.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/IntegrationTests/Node.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/IntegrationTests/WalletLegacyObserver.h"
    "${CMAKE_CURRENT_LIST_DIR}/IntegrationTests/WalletLegacyTests.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/IntegrationTests/main.cpp"
)

set(QwertycoinTests_IntegrationTests_LIBS
    Boost::program_options
    GTest::main
    QwertycoinFramework::CryptoNoteCore
    QwertycoinFramework::CryptoNoteProtocol
    QwertycoinFramework::Http
    QwertycoinFramework::InProcessNode
    QwertycoinFramework::Logging
    QwertycoinFramework::Rpc
    QwertycoinFramework::Serialization
    QwertycoinFramework::System
    QwertycoinTests::IntegrationTestLib
)

add_executable(QwertycoinTests_IntegrationTests ${QwertycoinTests_IntegrationTests_SOURCES})
add_executable(QwertycoinTests::IntegrationTests ALIAS QwertycoinTests_IntegrationTests)
target_include_directories(QwertycoinTests_IntegrationTests PRIVATE ${QwertycoinTests_INCLUDE_DIRS})
target_link_libraries(QwertycoinTests_IntegrationTests PRIVATE ${QwertycoinTests_IntegrationTests_LIBS})
set_target_properties(QwertycoinTests_IntegrationTests PROPERTIES OUTPUT_NAME "integration_tests")

# QwertycoinTests::NodeRpcProxyTests

set(QwertycoinTests_NodeRpcProxyTests_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/NodeRpcProxyTests/NodeRpcProxyTests.cpp"
)

set(QwertycoinTests_NodeRpcProxyTests_LIBS
    QwertycoinFramework::Common
    QwertycoinFramework::Logging
    QwertycoinFramework::NodeRpcProxy
    QwertycoinFramework::Rpc
    QwertycoinFramework::Serialization
)

add_executable(QwertycoinTests_NodeRpcProxyTests ${QwertycoinTests_NodeRpcProxyTests_SOURCES})
add_executable(QwertycoinTests::NodeRpcProxyTests ALIAS QwertycoinTests_NodeRpcProxyTests)
target_include_directories(QwertycoinTests_NodeRpcProxyTests PRIVATE ${QwertycoinTests_INCLUDE_DIRS})
target_link_libraries(QwertycoinTests_NodeRpcProxyTests PRIVATE ${QwertycoinTests_NodeRpcProxyTests_LIBS})
set_target_properties(QwertycoinTests_NodeRpcProxyTests PROPERTIES OUTPUT_NAME "node_rpc_proxy_tests")

# QwertycoinTests::PerformanceTests

set(QwertycoinTests_PerformanceTests_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/PerformanceTests/CheckRingSignature.h"
    "${CMAKE_CURRENT_LIST_DIR}/PerformanceTests/ConstructTransaction.h"
    "${CMAKE_CURRENT_LIST_DIR}/PerformanceTests/CryptoNoteSlowHash.h"
    "${CMAKE_CURRENT_LIST_DIR}/PerformanceTests/DerivePublicKey.h"
    "${CMAKE_CURRENT_LIST_DIR}/PerformanceTests/DeriveSecretKey.h"
    "${CMAKE_CURRENT_LIST_DIR}/PerformanceTests/GenerateKeyDerivation.h"
    "${CMAKE_CURRENT_LIST_DIR}/PerformanceTests/GenerateKeyImage.h"
    "${CMAKE_CURRENT_LIST_DIR}/PerformanceTests/GenerateKeyImageHelper.h"
    "${CMAKE_CURRENT_LIST_DIR}/PerformanceTests/IsOutToAccount.h"
    "${CMAKE_CURRENT_LIST_DIR}/PerformanceTests/MultiTransactionTestBase.h"
    "${CMAKE_CURRENT_LIST_DIR}/PerformanceTests/PerformanceTests.h"
    "${CMAKE_CURRENT_LIST_DIR}/PerformanceTests/PerformanceUtils.h"
    "${CMAKE_CURRENT_LIST_DIR}/PerformanceTests/SingleTransactionTestBase.h"
    "${CMAKE_CURRENT_LIST_DIR}/PerformanceTests/main.cpp"
)

set(QwertycoinTests_PerformanceTests_LIBS
    Boost::chrono
    QwertycoinFramework::CryptoNoteCore
    QwertycoinFramework::Logging
)

add_executable(QwertycoinTests_PerformanceTests ${QwertycoinTests_PerformanceTests_SOURCES})
add_executable(QwertycoinTests::PerformanceTests ALIAS QwertycoinTests_PerformanceTests)
target_include_directories(QwertycoinTests_PerformanceTests PRIVATE ${QwertycoinTests_INCLUDE_DIRS})
target_link_libraries(QwertycoinTests_PerformanceTests PRIVATE ${QwertycoinTests_PerformanceTests_LIBS})
set_target_properties(QwertycoinTests_PerformanceTests PROPERTIES OUTPUT_NAME "performance_tests")

# QwertycoinTests::SystemTests

set(QwertycoinTests_SystemTests_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/SystemTests/ContextGroupTests.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/SystemTests/ContextGroupTimeoutTests.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/SystemTests/ContextTests.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/SystemTests/DispatcherTests.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/SystemTests/ErrorMessageTests.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/SystemTests/EventLockTests.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/SystemTests/EventTests.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/SystemTests/Ipv4AddressTests.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/SystemTests/Ipv4ResolverTests.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/SystemTests/OperationTimeoutTests.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/SystemTests/RemoteContextTests.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/SystemTests/TcpConnectionTests.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/SystemTests/TcpConnectorTests.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/SystemTests/TcpListenerTests.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/SystemTests/TimerTests.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/SystemTests/main.cpp"
)

set(QwertycoinTests_SystemTests_LIBS
    GTest::main
    QwertycoinFramework::System
)

add_executable(QwertycoinTests_SystemTests ${QwertycoinTests_SystemTests_SOURCES})
add_executable(QwertycoinTests::SystemTests ALIAS QwertycoinTests_SystemTests)
target_include_directories(QwertycoinTests_SystemTests PRIVATE ${QwertycoinTests_INCLUDE_DIRS})
target_link_libraries(QwertycoinTests_SystemTests PRIVATE ${QwertycoinTests_SystemTests_LIBS})
set_target_properties(QwertycoinTests_SystemTests PROPERTIES OUTPUT_NAME "system_tests")

add_test(NAME SystemTests
         COMMAND system_tests)

# QwertycoinTests::TestGenerator

set(QwertycoinTests_TestGenerator_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/TestGenerator/TestGenerator.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/TestGenerator/TestGenerator.h"
)

set(QwertycoinTests_TestGenerator_LIBS "")

add_library(QwertycoinTests_TestGenerator ${QwertycoinTests_TestGenerator_SOURCES})
add_library(QwertycoinTests::TestGenerator ALIAS QwertycoinTests_TestGenerator)
target_include_directories(QwertycoinTests_TestGenerator PRIVATE ${QwertycoinTests_INCLUDE_DIRS})
target_link_libraries(QwertycoinTests_TestGenerator PRIVATE ${QwertycoinTests_TestGenerator_LIBS})

# QwertycoinTests::TransfersTests

set(QwertycoinTests_TransfersTests_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/TransfersTests/Globals.h"
    "${CMAKE_CURRENT_LIST_DIR}/TransfersTests/TestNodeRpcProxy.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/TransfersTests/TestTxPoolSync.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/TransfersTests/Tests.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/TransfersTests/main.cpp"
)

set(QwertycoinTests_TransfersTests_LIBS
    Boost::program_options
    GTest::main
    QwertycoinFramework::Common
    QwertycoinFramework::CryptoNoteCore
    QwertycoinFramework::CryptoNoteProtocol
    QwertycoinFramework::InProcessNode
    QwertycoinFramework::Logging
    QwertycoinFramework::Rpc
    QwertycoinFramework::Serialization
    QwertycoinFramework::System
    QwertycoinFramework::Transfers
    QwertycoinTests::IntegrationTestLib
)

add_executable(QwertycoinTests_TransfersTests ${QwertycoinTests_TransfersTests_SOURCES})
add_executable(QwertycoinTests::TransfersTests ALIAS QwertycoinTests_TransfersTests)
target_include_directories(QwertycoinTests_TransfersTests PRIVATE ${QwertycoinTests_INCLUDE_DIRS})
target_link_libraries(QwertycoinTests_TransfersTests PRIVATE ${QwertycoinTests_TransfersTests_LIBS})
set_target_properties(QwertycoinTests_TransfersTests PROPERTIES OUTPUT_NAME "transfers_tests")

add_test(NAME TransfersTests
         COMMAND transfers_tests)

# QwertycoinTests::UnitTests

set(QwertycoinTests_UnitTests_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/UnitTests/ArrayRefTests.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/UnitTests/ArrayViewTests.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/UnitTests/Base58.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/UnitTests/BinarySerializationCompatibility.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/UnitTests/BlockReward.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/UnitTests/BlockingQueue.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/UnitTests/Chacha8.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/UnitTests/Checkpoints.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/UnitTests/DecomposeAmountIntoDigits.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/UnitTests/EventWaiter.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/UnitTests/EventWaiter.h"
    "${CMAKE_CURRENT_LIST_DIR}/UnitTests/ICoreStub.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/UnitTests/ICoreStub.h"
    "${CMAKE_CURRENT_LIST_DIR}/UnitTests/ICryptoNoteProtocolQueryStub.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/UnitTests/ICryptoNoteProtocolQueryStub.h"
    "${CMAKE_CURRENT_LIST_DIR}/UnitTests/INodeStubs.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/UnitTests/INodeStubs.h"
    "${CMAKE_CURRENT_LIST_DIR}/UnitTests/MulDiv.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/UnitTests/ParseAmount.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/UnitTests/PaymentGateTests.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/UnitTests/Serialization.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/UnitTests/SerializationKV.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/UnitTests/Shuffle.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/UnitTests/StringBufferTests.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/UnitTests/StringViewTests.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/UnitTests/TestBcS.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/UnitTests/TestBlockchainExplorer.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/UnitTests/TestBlockchainGenerator.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/UnitTests/TestBlockchainGenerator.h"
    "${CMAKE_CURRENT_LIST_DIR}/UnitTests/TestCurrency.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/UnitTests/TestFileMappedVector.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/UnitTests/TestFormatUtils.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/UnitTests/TestInprocessNode.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/UnitTests/TestJsonValue.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/UnitTests/TestMessageQueue.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/UnitTests/TestPath.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/UnitTests/TestPeerlist.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/UnitTests/TestProtocolPack.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/UnitTests/TestTransactionPoolDetach.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/UnitTests/TestTransfers.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/UnitTests/TestTransfersConsumer.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/UnitTests/TestTransfersContainer.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/UnitTests/TestTransfersContainerKeyImage.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/UnitTests/TestTransfersSubscription.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/UnitTests/TestUpgradeDetector.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/UnitTests/TestWallet.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/UnitTests/TestWalletLegacy.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/UnitTests/TestWalletService.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/UnitTests/TransactionApi.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/UnitTests/TransactionApiHelpers.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/UnitTests/TransactionApiHelpers.h"
    "${CMAKE_CURRENT_LIST_DIR}/UnitTests/TransactionPool.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/UnitTests/TransfersObserver.h"
    "${CMAKE_CURRENT_LIST_DIR}/UnitTests/UnitTestsUtils.h"
    "${CMAKE_CURRENT_LIST_DIR}/UnitTests/main.cpp"
)

set(QwertycoinTests_UnitTests_LIBS
    GTest::main
)

add_executable(QwertycoinTests_UnitTests ${QwertycoinTests_UnitTests_SOURCES})
add_executable(QwertycoinTests::UnitTests ALIAS QwertycoinTests_UnitTests)
target_include_directories(QwertycoinTests_UnitTests PRIVATE ${QwertycoinTests_INCLUDE_DIRS})
target_link_libraries(QwertycoinTests_UnitTests PRIVATE ${QwertycoinTests_UnitTests_LIBS})
set_target_properties(QwertycoinTests_UnitTests PROPERTIES
    OUTPUT_NAME "unit_tests"
    EXCLUDE_FROM_ALL TRUE
    EXCLUDE_FROM_DEFAULT_BUILD TRUE)

add_test(NAME UnitTests
         COMMAND unit_tests)
