##
## Auto Generated makefile by CodeLite IDE
## any manual changes will be erased      
##
## Debug
ProjectName            :=qwertycoin
ConfigurationName      :=Debug
WorkspacePath          :=/home/exploshot/Dev/qwertycoin
ProjectPath            :=/home/exploshot/Dev/qwertycoin
IntermediateDirectory  :=./Debug
OutDir                 := $(IntermediateDirectory)
CurrentFileName        :=
CurrentFilePath        :=
CurrentFileFullPath    :=
User                   :=ExploShot
Date                   :=09/04/19
CodeLitePath           :=/home/exploshot/.codelite
LinkerName             :=/usr/bin/x86_64-linux-gnu-g++
SharedObjectLinkerName :=/usr/bin/x86_64-linux-gnu-g++ -shared -fPIC
ObjectSuffix           :=.o
DependSuffix           :=.o.d
PreprocessSuffix       :=.i
DebugSwitch            :=-g 
IncludeSwitch          :=-I
LibrarySwitch          :=-l
OutputSwitch           :=-o 
LibraryPathSwitch      :=-L
PreprocessorSwitch     :=-D
SourceSwitch           :=-c 
OutputFile             :=$(IntermediateDirectory)/$(ProjectName)
Preprocessors          :=
ObjectSwitch           :=-o 
ArchiveOutputSwitch    := 
PreprocessOnlySwitch   :=-E
ObjectsFileList        :="qwertycoin.txt"
PCHCompileFlags        :=
MakeDirCommand         :=mkdir -p
LinkOptions            :=  -O0
IncludePath            :=  $(IncludeSwitch). $(IncludeSwitch). 
IncludePCH             := 
RcIncludePath          := 
Libs                   := 
ArLibs                 :=  
LibPath                := $(LibraryPathSwitch). $(LibraryPathSwitch). $(LibraryPathSwitch)Debug 

##
## Common variables
## AR, CXX, CC, AS, CXXFLAGS and CFLAGS can be overriden using an environment variables
##
AR       := /usr/bin/x86_64-linux-gnu-ar rcu
CXX      := /usr/bin/x86_64-linux-gnu-g++
CC       := /usr/bin/x86_64-linux-gnu-gcc
CXXFLAGS :=  -g -Wall $(Preprocessors)
CFLAGS   :=   $(Preprocessors)
ASFLAGS  := 
AS       := /usr/bin/x86_64-linux-gnu-as


##
## User defined environment variables
##
CodeLiteDir:=/usr/share/codelite
Objects0=$(IntermediateDirectory)/cmake_test-static-assert.c$(ObjectSuffix) $(IntermediateDirectory)/cmake_test-static-assert.cpp$(ObjectSuffix) $(IntermediateDirectory)/utils_test-static-assert.c$(ObjectSuffix) $(IntermediateDirectory)/lib_crypto_slow-hash.c$(ObjectSuffix) $(IntermediateDirectory)/lib_crypto_oaes_lib.c$(ObjectSuffix) $(IntermediateDirectory)/lib_crypto_crypto-ops.c$(ObjectSuffix) $(IntermediateDirectory)/lib_crypto_cn_slow_hash_hard_intel.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_crypto_slow-hash.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_crypto_aesb.c$(ObjectSuffix) $(IntermediateDirectory)/lib_crypto_chacha8.c$(ObjectSuffix) \
	$(IntermediateDirectory)/lib_crypto_groestl.c$(ObjectSuffix) $(IntermediateDirectory)/lib_crypto_keccak.c$(ObjectSuffix) $(IntermediateDirectory)/lib_crypto_hash-extra-jh.c$(ObjectSuffix) $(IntermediateDirectory)/lib_crypto_hash-extra-blake.c$(ObjectSuffix) $(IntermediateDirectory)/lib_crypto_jh.c$(ObjectSuffix) $(IntermediateDirectory)/lib_crypto_cn_slow_hash_soft.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_crypto_hash-extra-skein.c$(ObjectSuffix) $(IntermediateDirectory)/lib_crypto_tree-hash.c$(ObjectSuffix) $(IntermediateDirectory)/lib_crypto_blake256.c$(ObjectSuffix) $(IntermediateDirectory)/lib_crypto_random.c$(ObjectSuffix) \
	$(IntermediateDirectory)/lib_crypto_crypto-ops-data.c$(ObjectSuffix) $(IntermediateDirectory)/lib_crypto_cn_slow_hash_hard_arm.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_crypto_hash.c$(ObjectSuffix) $(IntermediateDirectory)/lib_crypto_hash-extra-groestl.c$(ObjectSuffix) $(IntermediateDirectory)/lib_crypto_skein.c$(ObjectSuffix) $(IntermediateDirectory)/lib_crypto_crypto.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_CryptoNoteCore_MinerConfig.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_CryptoNoteCore_TransactionExtra.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_CryptoNoteCore_Difficulty.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_CryptoNoteCore_Blockchain.cpp$(ObjectSuffix) \
	$(IntermediateDirectory)/lib_CryptoNoteCore_Transaction.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_CryptoNoteCore_CryptoNoteSerialization.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_CryptoNoteCore_CryptoNoteFormatUtils.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_CryptoNoteCore_UpgradeDetector.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_CryptoNoteCore_BlockIndex.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_CryptoNoteCore_BlockchainIndices.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_CryptoNoteCore_CryptoNoteBasicImpl.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_CryptoNoteCore_IBlock.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_CryptoNoteCore_Core.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_CryptoNoteCore_Currency.cpp$(ObjectSuffix) \
	$(IntermediateDirectory)/lib_CryptoNoteCore_CryptoNoteBasic.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_CryptoNoteCore_Miner.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_CryptoNoteCore_CryptoNoteTools.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_CryptoNoteCore_TransactionPool.cpp$(ObjectSuffix) 

Objects1=$(IntermediateDirectory)/lib_CryptoNoteCore_TransactionPrefixImpl.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_CryptoNoteCore_ITimeProvider.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_CryptoNoteCore_Account.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_CryptoNoteCore_BlockchainMessages.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_CryptoNoteCore_TransactionUtils.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_CryptoNoteCore_SwappedMap.cpp$(ObjectSuffix) \
	$(IntermediateDirectory)/lib_CryptoNoteCore_SwappedVector.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_CryptoNoteCore_Checkpoints.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_CryptoNoteCore_CoreConfig.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Rpc_RpcServerConfig.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Rpc_JsonRpc.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Rpc_RpcServer.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Rpc_HttpServer.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Rpc_HttpClient.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_HTTP_HttpParserErrorCodes.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_HTTP_HttpParser.cpp$(ObjectSuffix) \
	$(IntermediateDirectory)/lib_HTTP_HttpRequest.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_HTTP_HttpResponse.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_NodeRpcProxy_NodeRpcProxy.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_NodeRpcProxy_NodeErrors.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Mnemonics_electrum-words.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Wallet_LegacyKeysImporter.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Wallet_WalletErrors.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Wallet_WalletGreen.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Wallet_WalletRpcServer.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Wallet_WalletUtils.cpp$(ObjectSuffix) \
	$(IntermediateDirectory)/lib_Wallet_WalletAsyncContextCounter.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Wallet_WalletSerializationV2.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Wallet_WalletSerializationV1.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Common_StringTools.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Common_IInputStream.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Common_ScopeExit.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Common_CommandLine.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Common_JsonValue.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Common_ConsoleHandler.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Common_StringInputStream.cpp$(ObjectSuffix) \
	

Objects2=$(IntermediateDirectory)/lib_Common_StdInputStream.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Common_ConsoleTools.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Common_Base58.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Common_Math.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Common_StringView.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Common_StringOutputStream.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Common_VectorOutputStream.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Common_FileMappedVector.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Common_StdOutputStream.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Common_DnsTools.cpp$(ObjectSuffix) \
	$(IntermediateDirectory)/lib_Common_StringUtils.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Common_BlockingQueue.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Common_StreamTools.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Common_Util.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Common_SignalHandler.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Common_MemoryInputStream.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Common_PathTools.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Common_IOutputStream.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Logging_FileLogger.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Logging_LoggerGroup.cpp$(ObjectSuffix) \
	$(IntermediateDirectory)/lib_Logging_LoggerManager.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Logging_ILogger.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Logging_StreamLogger.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Logging_LoggerRef.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Logging_LoggerMessage.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Logging_CommonLogger.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Logging_ConsoleLogger.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_WalletLegacy_KeysStorage.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_WalletLegacy_WalletTransactionSender.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_WalletLegacy_WalletUserTransactionsCache.cpp$(ObjectSuffix) \
	$(IntermediateDirectory)/lib_WalletLegacy_WalletHelper.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_WalletLegacy_WalletUnconfirmedTransactions.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_WalletLegacy_WalletLegacySerialization.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_WalletLegacy_WalletLegacySerializer.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_WalletLegacy_WalletLegacy.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_InProcessNode_InProcessNodeErrors.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_InProcessNode_InProcessNode.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Transfers_TransfersConsumer.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Transfers_BlockchainSynchronizer.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Transfers_TransfersSynchronizer.cpp$(ObjectSuffix) \
	

Objects3=$(IntermediateDirectory)/lib_Transfers_TransfersContainer.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Transfers_SynchronizationState.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Transfers_TransfersSubscription.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_JsonRpcServer_JsonRpcServer.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_CryptoNoteProtocol_CryptoNoteProtocolHandler.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_PaymentGate_NodeFactory.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_PaymentGate_WalletServiceErrorCategory.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_PaymentGate_PaymentServiceJsonRpcMessages.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_PaymentGate_PaymentServiceJsonRpcServer.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_PaymentGate_WalletService.cpp$(ObjectSuffix) \
	$(IntermediateDirectory)/lib_P2p_P2pContext.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_P2p_PeerListManager.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_P2p_P2pConnectionProxy.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_P2p_IP2pNodeInternal.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_P2p_LevinProtocol.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_P2p_NetNode.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_P2p_NetNodeConfig.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_P2p_P2pInterfaces.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_P2p_P2pNode.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_P2p_P2pNodeConfig.cpp$(ObjectSuffix) \
	$(IntermediateDirectory)/lib_P2p_P2pContextOwner.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Serialization_SerializationOverloads.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Serialization_KVBinaryOutputStreamSerializer.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Serialization_JsonInputStreamSerializer.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Serialization_JsonOutputStreamSerializer.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Serialization_JsonInputValueSerializer.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Serialization_BinaryOutputStreamSerializer.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Serialization_BlockchainExplorerDataSerialization.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Serialization_KVBinaryInputStreamSerializer.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Serialization_MemoryStream.cpp$(ObjectSuffix) \
	$(IntermediateDirectory)/lib_Serialization_BinaryInputStreamSerializer.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_System_Ipv4Address.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_System_ContextGroup.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_System_RemoteEventLock.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_System_ContextGroupTimeout.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_System_TcpStream.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_System_InterruptedException.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_System_Event.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_System_EventLock.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_BlockchainExplorer_BlockchainExplorerDataBuilder.cpp$(ObjectSuffix) \
	$(IntermediateDirectory)/lib_BlockchainExplorer_BlockchainExplorer.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_BlockchainExplorer_BlockchainExplorerErrors.cpp$(ObjectSuffix) 

Objects4=$(IntermediateDirectory)/src_Miner_MiningConfig.cpp$(ObjectSuffix) $(IntermediateDirectory)/src_Miner_MinerManager.cpp$(ObjectSuffix) $(IntermediateDirectory)/src_Miner_Miner.cpp$(ObjectSuffix) $(IntermediateDirectory)/src_Miner_BlockchainMonitor.cpp$(ObjectSuffix) $(IntermediateDirectory)/src_Miner_main.cpp$(ObjectSuffix) $(IntermediateDirectory)/src_Daemon_DaemonCommandsHandler.cpp$(ObjectSuffix) $(IntermediateDirectory)/src_Daemon_Daemon.cpp$(ObjectSuffix) $(IntermediateDirectory)/src_ConnectivityTool_ConnectivityTool.cpp$(ObjectSuffix) \
	$(IntermediateDirectory)/src_PaymentGateService_PaymentGateService.cpp$(ObjectSuffix) $(IntermediateDirectory)/src_PaymentGateService_RpcNodeConfiguration.cpp$(ObjectSuffix) $(IntermediateDirectory)/src_PaymentGateService_PaymentServiceConfiguration.cpp$(ObjectSuffix) $(IntermediateDirectory)/src_PaymentGateService_ConfigurationManager.cpp$(ObjectSuffix) $(IntermediateDirectory)/src_PaymentGateService_main.cpp$(ObjectSuffix) $(IntermediateDirectory)/src_SimpleWallet_SimpleWallet.cpp$(ObjectSuffix) $(IntermediateDirectory)/src_SimpleWallet_PasswordContainer.cpp$(ObjectSuffix) $(IntermediateDirectory)/build_CMakeFiles_feature_tests.cxx$(ObjectSuffix) $(IntermediateDirectory)/build_CMakeFiles_feature_tests.c$(ObjectSuffix) $(IntermediateDirectory)/tests_HashTests_main.cpp$(ObjectSuffix) \
	$(IntermediateDirectory)/tests_SystemTests_EventLockTests.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_SystemTests_EventTests.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_SystemTests_TcpConnectorTests.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_SystemTests_TimerTests.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_SystemTests_OperationTimeoutTests.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_SystemTests_ContextTests.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_SystemTests_ErrorMessageTests.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_SystemTests_Ipv4ResolverTests.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_SystemTests_RemoteContextTests.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_SystemTests_TcpConnectionTests.cpp$(ObjectSuffix) \
	$(IntermediateDirectory)/tests_SystemTests_ContextGroupTimeoutTests.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_SystemTests_ContextGroupTests.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_SystemTests_Ipv4AddressTests.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_SystemTests_TcpListenerTests.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_SystemTests_main.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_SystemTests_DispatcherTests.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_IntegrationTests_WalletLegacyTests.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_IntegrationTests_MultiVersion.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_IntegrationTests_Node.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_IntegrationTests_IntegrationTests.cpp$(ObjectSuffix) \
	$(IntermediateDirectory)/tests_IntegrationTests_main.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_CryptoTests_crypto-ops.c$(ObjectSuffix) $(IntermediateDirectory)/tests_CryptoTests_random.c$(ObjectSuffix) $(IntermediateDirectory)/tests_CryptoTests_crypto-ops-data.c$(ObjectSuffix) $(IntermediateDirectory)/tests_CryptoTests_main.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_CryptoTests_hash.c$(ObjectSuffix) $(IntermediateDirectory)/tests_CryptoTests_crypto.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_TestGenerator_TestGenerator.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_PerformanceTests_main.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_NodeRpcProxyTests_NodeRpcProxyTests.cpp$(ObjectSuffix) \
	$(IntermediateDirectory)/tests_UnitTests_TestTransfersContainer.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_UnitTests_TestPath.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_UnitTests_BinarySerializationCompatibility.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_UnitTests_TransactionApiHelpers.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_UnitTests_TestWalletService.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_UnitTests_TestPeerlist.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_UnitTests_TestFormatUtils.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_UnitTests_EventWaiter.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_UnitTests_MulDiv.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_UnitTests_ArrayViewTests.cpp$(ObjectSuffix) \
	$(IntermediateDirectory)/tests_UnitTests_TestTransfersConsumer.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_UnitTests_Base58.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_UnitTests_TestMessageQueue.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_UnitTests_TestTransfersContainerKeyImage.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_UnitTests_TestBlockchainGenerator.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_UnitTests_TestUpgradeDetector.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_UnitTests_Shuffle.cpp$(ObjectSuffix) 

Objects5=$(IntermediateDirectory)/tests_UnitTests_TestProtocolPack.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_UnitTests_INodeStubs.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_UnitTests_StringViewTests.cpp$(ObjectSuffix) \
	$(IntermediateDirectory)/tests_UnitTests_DecomposeAmountIntoDigits.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_UnitTests_TestTransfersSubscription.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_UnitTests_TestBlockchainExplorer.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_UnitTests_TestWallet.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_UnitTests_TestTransfers.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_UnitTests_TransactionPool.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_UnitTests_TestFileMappedVector.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_UnitTests_TestInprocessNode.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_UnitTests_ICryptoNoteProtocolQueryStub.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_UnitTests_TestTransactionPoolDetach.cpp$(ObjectSuffix) \
	$(IntermediateDirectory)/tests_UnitTests_TransactionApi.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_UnitTests_TestBcS.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_UnitTests_TestWalletLegacy.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_UnitTests_BlockReward.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_UnitTests_ICoreStub.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_UnitTests_BlockingQueue.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_UnitTests_Chacha8.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_UnitTests_main.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_UnitTests_StringBufferTests.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_UnitTests_Serialization.cpp$(ObjectSuffix) \
	$(IntermediateDirectory)/tests_UnitTests_TestCurrency.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_UnitTests_ParseAmount.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_UnitTests_ArrayRefTests.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_UnitTests_SerializationKV.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_UnitTests_PaymentGateTests.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_UnitTests_Checkpoints.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_UnitTests_TestJsonValue.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_CoreTests_TransactionBuilder.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_CoreTests_ChainSplit1.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_CoreTests_RingSignature.cpp$(ObjectSuffix) \
	$(IntermediateDirectory)/tests_CoreTests_ChaingenMain.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_CoreTests_Upgrade.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_CoreTests_TransactionValidation.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_CoreTests_ChainSwitch1.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_CoreTests_DoubleSpend.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_CoreTests_BlockValidation.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_CoreTests_RandomOuts.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_CoreTests_Chaingen001.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_CoreTests_TransactionTests.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_CoreTests_BlockReward.cpp$(ObjectSuffix) \
	$(IntermediateDirectory)/tests_CoreTests_IntegerOverflow.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_CoreTests_Chaingen.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_IntegrationTestLib_TestNetwork.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_IntegrationTestLib_BaseFunctionalTests.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_IntegrationTestLib_RPCTestNode.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_IntegrationTestLib_InProcTestNode.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_IntegrationTestLib_TestWalletLegacy.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_IntegrationTestLib_Process.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_IntegrationTestLib_Logger.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_DifficultyTests_Difficulty.cpp$(ObjectSuffix) \
	$(IntermediateDirectory)/tests_HashTargetTests_HashTarget.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_TransfersTests_Tests.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_TransfersTests_main.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_TransfersTests_TestTxPoolSync.cpp$(ObjectSuffix) $(IntermediateDirectory)/tests_TransfersTests_TestNodeRpcProxy.cpp$(ObjectSuffix) 

Objects6=$(IntermediateDirectory)/lib_Platform_Linux_System_ErrorMessage.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Platform_Linux_System_TcpConnection.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Platform_Linux_System_TcpListener.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Platform_Linux_System_Dispatcher.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Platform_Linux_System_Ipv4Resolver.cpp$(ObjectSuffix) \
	$(IntermediateDirectory)/lib_Platform_Linux_System_Timer.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Platform_Linux_System_TcpConnector.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Platform_Posix_System_MemoryMappedFile.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Platform_FreeBSD_System_Context.c$(ObjectSuffix) $(IntermediateDirectory)/lib_Platform_FreeBSD_System_ErrorMessage.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Platform_FreeBSD_System_TcpConnection.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Platform_FreeBSD_System_TcpListener.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Platform_FreeBSD_System_Dispatcher.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Platform_FreeBSD_System_Ipv4Resolver.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Platform_FreeBSD_System_Timer.cpp$(ObjectSuffix) \
	$(IntermediateDirectory)/lib_Platform_FreeBSD_System_TcpConnector.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Platform_Android_System_ErrorMessage.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Platform_Android_System_TcpConnection.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Platform_Android_System_TcpListener.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Platform_Android_System_Dispatcher.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Platform_Android_System_Ipv4Resolver.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Platform_Android_System_Timer.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Platform_Android_System_TcpConnector.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Platform_Windows_System_ErrorMessage.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Platform_Windows_System_TcpConnection.cpp$(ObjectSuffix) \
	$(IntermediateDirectory)/lib_Platform_Windows_System_TcpListener.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Platform_Windows_System_MemoryMappedFile.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Platform_Windows_System_Dispatcher.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Platform_Windows_System_Ipv4Resolver.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Platform_Windows_System_Timer.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Platform_Windows_System_TcpConnector.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Platform_OSX_System_Context.c$(ObjectSuffix) $(IntermediateDirectory)/lib_Platform_OSX_System_ErrorMessage.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Platform_OSX_System_TcpConnection.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Platform_OSX_System_TcpListener.cpp$(ObjectSuffix) \
	$(IntermediateDirectory)/lib_Platform_OSX_System_Dispatcher.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Platform_OSX_System_Ipv4Resolver.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Platform_OSX_System_Timer.cpp$(ObjectSuffix) $(IntermediateDirectory)/lib_Platform_OSX_System_TcpConnector.cpp$(ObjectSuffix) $(IntermediateDirectory)/build_CMakeFiles_3.10.2_CompilerIdCXX_CMakeCXXCompilerId.cpp$(ObjectSuffix) $(IntermediateDirectory)/build_CMakeFiles_3.10.2_CompilerIdC_CMakeCCompilerId.c$(ObjectSuffix) $(IntermediateDirectory)/build__3rdParty_Hunter_toolchain__builds_CMakeFiles_feature_tests.cxx$(ObjectSuffix) $(IntermediateDirectory)/build__3rdParty_Hunter_toolchain__builds_CMakeFiles_feature_tests.c$(ObjectSuffix) $(IntermediateDirectory)/build__3rdParty_Hunter_toolchain__builds_CMakeFiles_3.10.2_CompilerIdCXX_CMakeCXXCompilerId.cpp$(ObjectSuffix) $(IntermediateDirectory)/build__3rdParty_Hunter_toolchain__builds_CMakeFiles_3.10.2_CompilerIdC_CMakeCCompilerId.c$(ObjectSuffix) \
	



Objects=$(Objects0) $(Objects1) $(Objects2) $(Objects3) $(Objects4) $(Objects5) $(Objects6) 

##
## Main Build Targets 
##
.PHONY: all clean PreBuild PrePreBuild PostBuild MakeIntermediateDirs
all: $(OutputFile)

$(OutputFile): $(IntermediateDirectory)/.d $(Objects) 
	@$(MakeDirCommand) $(@D)
	@echo "" > $(IntermediateDirectory)/.d
	@echo $(Objects0)  > $(ObjectsFileList)
	@echo $(Objects1) >> $(ObjectsFileList)
	@echo $(Objects2) >> $(ObjectsFileList)
	@echo $(Objects3) >> $(ObjectsFileList)
	@echo $(Objects4) >> $(ObjectsFileList)
	@echo $(Objects5) >> $(ObjectsFileList)
	@echo $(Objects6) >> $(ObjectsFileList)
	$(LinkerName) $(OutputSwitch)$(OutputFile) @$(ObjectsFileList) $(LibPath) $(Libs) $(LinkOptions)

MakeIntermediateDirs:
	@test -d ./Debug || $(MakeDirCommand) ./Debug


$(IntermediateDirectory)/.d:
	@test -d ./Debug || $(MakeDirCommand) ./Debug

PreBuild:


##
## Objects
##
$(IntermediateDirectory)/cmake_test-static-assert.c$(ObjectSuffix): cmake/test-static-assert.c $(IntermediateDirectory)/cmake_test-static-assert.c$(DependSuffix)
	$(CC) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/cmake/test-static-assert.c" $(CFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/cmake_test-static-assert.c$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/cmake_test-static-assert.c$(DependSuffix): cmake/test-static-assert.c
	@$(CC) $(CFLAGS) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/cmake_test-static-assert.c$(ObjectSuffix) -MF$(IntermediateDirectory)/cmake_test-static-assert.c$(DependSuffix) -MM cmake/test-static-assert.c

$(IntermediateDirectory)/cmake_test-static-assert.c$(PreprocessSuffix): cmake/test-static-assert.c
	$(CC) $(CFLAGS) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/cmake_test-static-assert.c$(PreprocessSuffix) cmake/test-static-assert.c

$(IntermediateDirectory)/cmake_test-static-assert.cpp$(ObjectSuffix): cmake/test-static-assert.cpp $(IntermediateDirectory)/cmake_test-static-assert.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/cmake/test-static-assert.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/cmake_test-static-assert.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/cmake_test-static-assert.cpp$(DependSuffix): cmake/test-static-assert.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/cmake_test-static-assert.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/cmake_test-static-assert.cpp$(DependSuffix) -MM cmake/test-static-assert.cpp

$(IntermediateDirectory)/cmake_test-static-assert.cpp$(PreprocessSuffix): cmake/test-static-assert.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/cmake_test-static-assert.cpp$(PreprocessSuffix) cmake/test-static-assert.cpp

$(IntermediateDirectory)/utils_test-static-assert.c$(ObjectSuffix): utils/test-static-assert.c $(IntermediateDirectory)/utils_test-static-assert.c$(DependSuffix)
	$(CC) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/utils/test-static-assert.c" $(CFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/utils_test-static-assert.c$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/utils_test-static-assert.c$(DependSuffix): utils/test-static-assert.c
	@$(CC) $(CFLAGS) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/utils_test-static-assert.c$(ObjectSuffix) -MF$(IntermediateDirectory)/utils_test-static-assert.c$(DependSuffix) -MM utils/test-static-assert.c

$(IntermediateDirectory)/utils_test-static-assert.c$(PreprocessSuffix): utils/test-static-assert.c
	$(CC) $(CFLAGS) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/utils_test-static-assert.c$(PreprocessSuffix) utils/test-static-assert.c

$(IntermediateDirectory)/lib_crypto_slow-hash.c$(ObjectSuffix): lib/crypto/slow-hash.c $(IntermediateDirectory)/lib_crypto_slow-hash.c$(DependSuffix)
	$(CC) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/crypto/slow-hash.c" $(CFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_crypto_slow-hash.c$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_crypto_slow-hash.c$(DependSuffix): lib/crypto/slow-hash.c
	@$(CC) $(CFLAGS) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_crypto_slow-hash.c$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_crypto_slow-hash.c$(DependSuffix) -MM lib/crypto/slow-hash.c

$(IntermediateDirectory)/lib_crypto_slow-hash.c$(PreprocessSuffix): lib/crypto/slow-hash.c
	$(CC) $(CFLAGS) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_crypto_slow-hash.c$(PreprocessSuffix) lib/crypto/slow-hash.c

$(IntermediateDirectory)/lib_crypto_oaes_lib.c$(ObjectSuffix): lib/crypto/oaes_lib.c $(IntermediateDirectory)/lib_crypto_oaes_lib.c$(DependSuffix)
	$(CC) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/crypto/oaes_lib.c" $(CFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_crypto_oaes_lib.c$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_crypto_oaes_lib.c$(DependSuffix): lib/crypto/oaes_lib.c
	@$(CC) $(CFLAGS) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_crypto_oaes_lib.c$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_crypto_oaes_lib.c$(DependSuffix) -MM lib/crypto/oaes_lib.c

$(IntermediateDirectory)/lib_crypto_oaes_lib.c$(PreprocessSuffix): lib/crypto/oaes_lib.c
	$(CC) $(CFLAGS) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_crypto_oaes_lib.c$(PreprocessSuffix) lib/crypto/oaes_lib.c

$(IntermediateDirectory)/lib_crypto_crypto-ops.c$(ObjectSuffix): lib/crypto/crypto-ops.c $(IntermediateDirectory)/lib_crypto_crypto-ops.c$(DependSuffix)
	$(CC) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/crypto/crypto-ops.c" $(CFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_crypto_crypto-ops.c$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_crypto_crypto-ops.c$(DependSuffix): lib/crypto/crypto-ops.c
	@$(CC) $(CFLAGS) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_crypto_crypto-ops.c$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_crypto_crypto-ops.c$(DependSuffix) -MM lib/crypto/crypto-ops.c

$(IntermediateDirectory)/lib_crypto_crypto-ops.c$(PreprocessSuffix): lib/crypto/crypto-ops.c
	$(CC) $(CFLAGS) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_crypto_crypto-ops.c$(PreprocessSuffix) lib/crypto/crypto-ops.c

$(IntermediateDirectory)/lib_crypto_cn_slow_hash_hard_intel.cpp$(ObjectSuffix): lib/crypto/cn_slow_hash_hard_intel.cpp $(IntermediateDirectory)/lib_crypto_cn_slow_hash_hard_intel.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/crypto/cn_slow_hash_hard_intel.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_crypto_cn_slow_hash_hard_intel.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_crypto_cn_slow_hash_hard_intel.cpp$(DependSuffix): lib/crypto/cn_slow_hash_hard_intel.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_crypto_cn_slow_hash_hard_intel.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_crypto_cn_slow_hash_hard_intel.cpp$(DependSuffix) -MM lib/crypto/cn_slow_hash_hard_intel.cpp

$(IntermediateDirectory)/lib_crypto_cn_slow_hash_hard_intel.cpp$(PreprocessSuffix): lib/crypto/cn_slow_hash_hard_intel.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_crypto_cn_slow_hash_hard_intel.cpp$(PreprocessSuffix) lib/crypto/cn_slow_hash_hard_intel.cpp

$(IntermediateDirectory)/lib_crypto_slow-hash.cpp$(ObjectSuffix): lib/crypto/slow-hash.cpp $(IntermediateDirectory)/lib_crypto_slow-hash.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/crypto/slow-hash.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_crypto_slow-hash.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_crypto_slow-hash.cpp$(DependSuffix): lib/crypto/slow-hash.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_crypto_slow-hash.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_crypto_slow-hash.cpp$(DependSuffix) -MM lib/crypto/slow-hash.cpp

$(IntermediateDirectory)/lib_crypto_slow-hash.cpp$(PreprocessSuffix): lib/crypto/slow-hash.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_crypto_slow-hash.cpp$(PreprocessSuffix) lib/crypto/slow-hash.cpp

$(IntermediateDirectory)/lib_crypto_aesb.c$(ObjectSuffix): lib/crypto/aesb.c $(IntermediateDirectory)/lib_crypto_aesb.c$(DependSuffix)
	$(CC) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/crypto/aesb.c" $(CFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_crypto_aesb.c$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_crypto_aesb.c$(DependSuffix): lib/crypto/aesb.c
	@$(CC) $(CFLAGS) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_crypto_aesb.c$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_crypto_aesb.c$(DependSuffix) -MM lib/crypto/aesb.c

$(IntermediateDirectory)/lib_crypto_aesb.c$(PreprocessSuffix): lib/crypto/aesb.c
	$(CC) $(CFLAGS) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_crypto_aesb.c$(PreprocessSuffix) lib/crypto/aesb.c

$(IntermediateDirectory)/lib_crypto_chacha8.c$(ObjectSuffix): lib/crypto/chacha8.c $(IntermediateDirectory)/lib_crypto_chacha8.c$(DependSuffix)
	$(CC) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/crypto/chacha8.c" $(CFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_crypto_chacha8.c$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_crypto_chacha8.c$(DependSuffix): lib/crypto/chacha8.c
	@$(CC) $(CFLAGS) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_crypto_chacha8.c$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_crypto_chacha8.c$(DependSuffix) -MM lib/crypto/chacha8.c

$(IntermediateDirectory)/lib_crypto_chacha8.c$(PreprocessSuffix): lib/crypto/chacha8.c
	$(CC) $(CFLAGS) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_crypto_chacha8.c$(PreprocessSuffix) lib/crypto/chacha8.c

$(IntermediateDirectory)/lib_crypto_groestl.c$(ObjectSuffix): lib/crypto/groestl.c $(IntermediateDirectory)/lib_crypto_groestl.c$(DependSuffix)
	$(CC) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/crypto/groestl.c" $(CFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_crypto_groestl.c$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_crypto_groestl.c$(DependSuffix): lib/crypto/groestl.c
	@$(CC) $(CFLAGS) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_crypto_groestl.c$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_crypto_groestl.c$(DependSuffix) -MM lib/crypto/groestl.c

$(IntermediateDirectory)/lib_crypto_groestl.c$(PreprocessSuffix): lib/crypto/groestl.c
	$(CC) $(CFLAGS) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_crypto_groestl.c$(PreprocessSuffix) lib/crypto/groestl.c

$(IntermediateDirectory)/lib_crypto_keccak.c$(ObjectSuffix): lib/crypto/keccak.c $(IntermediateDirectory)/lib_crypto_keccak.c$(DependSuffix)
	$(CC) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/crypto/keccak.c" $(CFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_crypto_keccak.c$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_crypto_keccak.c$(DependSuffix): lib/crypto/keccak.c
	@$(CC) $(CFLAGS) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_crypto_keccak.c$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_crypto_keccak.c$(DependSuffix) -MM lib/crypto/keccak.c

$(IntermediateDirectory)/lib_crypto_keccak.c$(PreprocessSuffix): lib/crypto/keccak.c
	$(CC) $(CFLAGS) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_crypto_keccak.c$(PreprocessSuffix) lib/crypto/keccak.c

$(IntermediateDirectory)/lib_crypto_hash-extra-jh.c$(ObjectSuffix): lib/crypto/hash-extra-jh.c $(IntermediateDirectory)/lib_crypto_hash-extra-jh.c$(DependSuffix)
	$(CC) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/crypto/hash-extra-jh.c" $(CFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_crypto_hash-extra-jh.c$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_crypto_hash-extra-jh.c$(DependSuffix): lib/crypto/hash-extra-jh.c
	@$(CC) $(CFLAGS) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_crypto_hash-extra-jh.c$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_crypto_hash-extra-jh.c$(DependSuffix) -MM lib/crypto/hash-extra-jh.c

$(IntermediateDirectory)/lib_crypto_hash-extra-jh.c$(PreprocessSuffix): lib/crypto/hash-extra-jh.c
	$(CC) $(CFLAGS) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_crypto_hash-extra-jh.c$(PreprocessSuffix) lib/crypto/hash-extra-jh.c

$(IntermediateDirectory)/lib_crypto_hash-extra-blake.c$(ObjectSuffix): lib/crypto/hash-extra-blake.c $(IntermediateDirectory)/lib_crypto_hash-extra-blake.c$(DependSuffix)
	$(CC) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/crypto/hash-extra-blake.c" $(CFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_crypto_hash-extra-blake.c$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_crypto_hash-extra-blake.c$(DependSuffix): lib/crypto/hash-extra-blake.c
	@$(CC) $(CFLAGS) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_crypto_hash-extra-blake.c$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_crypto_hash-extra-blake.c$(DependSuffix) -MM lib/crypto/hash-extra-blake.c

$(IntermediateDirectory)/lib_crypto_hash-extra-blake.c$(PreprocessSuffix): lib/crypto/hash-extra-blake.c
	$(CC) $(CFLAGS) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_crypto_hash-extra-blake.c$(PreprocessSuffix) lib/crypto/hash-extra-blake.c

$(IntermediateDirectory)/lib_crypto_jh.c$(ObjectSuffix): lib/crypto/jh.c $(IntermediateDirectory)/lib_crypto_jh.c$(DependSuffix)
	$(CC) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/crypto/jh.c" $(CFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_crypto_jh.c$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_crypto_jh.c$(DependSuffix): lib/crypto/jh.c
	@$(CC) $(CFLAGS) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_crypto_jh.c$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_crypto_jh.c$(DependSuffix) -MM lib/crypto/jh.c

$(IntermediateDirectory)/lib_crypto_jh.c$(PreprocessSuffix): lib/crypto/jh.c
	$(CC) $(CFLAGS) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_crypto_jh.c$(PreprocessSuffix) lib/crypto/jh.c

$(IntermediateDirectory)/lib_crypto_cn_slow_hash_soft.cpp$(ObjectSuffix): lib/crypto/cn_slow_hash_soft.cpp $(IntermediateDirectory)/lib_crypto_cn_slow_hash_soft.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/crypto/cn_slow_hash_soft.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_crypto_cn_slow_hash_soft.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_crypto_cn_slow_hash_soft.cpp$(DependSuffix): lib/crypto/cn_slow_hash_soft.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_crypto_cn_slow_hash_soft.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_crypto_cn_slow_hash_soft.cpp$(DependSuffix) -MM lib/crypto/cn_slow_hash_soft.cpp

$(IntermediateDirectory)/lib_crypto_cn_slow_hash_soft.cpp$(PreprocessSuffix): lib/crypto/cn_slow_hash_soft.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_crypto_cn_slow_hash_soft.cpp$(PreprocessSuffix) lib/crypto/cn_slow_hash_soft.cpp

$(IntermediateDirectory)/lib_crypto_hash-extra-skein.c$(ObjectSuffix): lib/crypto/hash-extra-skein.c $(IntermediateDirectory)/lib_crypto_hash-extra-skein.c$(DependSuffix)
	$(CC) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/crypto/hash-extra-skein.c" $(CFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_crypto_hash-extra-skein.c$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_crypto_hash-extra-skein.c$(DependSuffix): lib/crypto/hash-extra-skein.c
	@$(CC) $(CFLAGS) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_crypto_hash-extra-skein.c$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_crypto_hash-extra-skein.c$(DependSuffix) -MM lib/crypto/hash-extra-skein.c

$(IntermediateDirectory)/lib_crypto_hash-extra-skein.c$(PreprocessSuffix): lib/crypto/hash-extra-skein.c
	$(CC) $(CFLAGS) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_crypto_hash-extra-skein.c$(PreprocessSuffix) lib/crypto/hash-extra-skein.c

$(IntermediateDirectory)/lib_crypto_tree-hash.c$(ObjectSuffix): lib/crypto/tree-hash.c $(IntermediateDirectory)/lib_crypto_tree-hash.c$(DependSuffix)
	$(CC) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/crypto/tree-hash.c" $(CFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_crypto_tree-hash.c$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_crypto_tree-hash.c$(DependSuffix): lib/crypto/tree-hash.c
	@$(CC) $(CFLAGS) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_crypto_tree-hash.c$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_crypto_tree-hash.c$(DependSuffix) -MM lib/crypto/tree-hash.c

$(IntermediateDirectory)/lib_crypto_tree-hash.c$(PreprocessSuffix): lib/crypto/tree-hash.c
	$(CC) $(CFLAGS) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_crypto_tree-hash.c$(PreprocessSuffix) lib/crypto/tree-hash.c

$(IntermediateDirectory)/lib_crypto_blake256.c$(ObjectSuffix): lib/crypto/blake256.c $(IntermediateDirectory)/lib_crypto_blake256.c$(DependSuffix)
	$(CC) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/crypto/blake256.c" $(CFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_crypto_blake256.c$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_crypto_blake256.c$(DependSuffix): lib/crypto/blake256.c
	@$(CC) $(CFLAGS) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_crypto_blake256.c$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_crypto_blake256.c$(DependSuffix) -MM lib/crypto/blake256.c

$(IntermediateDirectory)/lib_crypto_blake256.c$(PreprocessSuffix): lib/crypto/blake256.c
	$(CC) $(CFLAGS) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_crypto_blake256.c$(PreprocessSuffix) lib/crypto/blake256.c

$(IntermediateDirectory)/lib_crypto_random.c$(ObjectSuffix): lib/crypto/random.c $(IntermediateDirectory)/lib_crypto_random.c$(DependSuffix)
	$(CC) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/crypto/random.c" $(CFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_crypto_random.c$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_crypto_random.c$(DependSuffix): lib/crypto/random.c
	@$(CC) $(CFLAGS) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_crypto_random.c$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_crypto_random.c$(DependSuffix) -MM lib/crypto/random.c

$(IntermediateDirectory)/lib_crypto_random.c$(PreprocessSuffix): lib/crypto/random.c
	$(CC) $(CFLAGS) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_crypto_random.c$(PreprocessSuffix) lib/crypto/random.c

$(IntermediateDirectory)/lib_crypto_crypto-ops-data.c$(ObjectSuffix): lib/crypto/crypto-ops-data.c $(IntermediateDirectory)/lib_crypto_crypto-ops-data.c$(DependSuffix)
	$(CC) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/crypto/crypto-ops-data.c" $(CFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_crypto_crypto-ops-data.c$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_crypto_crypto-ops-data.c$(DependSuffix): lib/crypto/crypto-ops-data.c
	@$(CC) $(CFLAGS) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_crypto_crypto-ops-data.c$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_crypto_crypto-ops-data.c$(DependSuffix) -MM lib/crypto/crypto-ops-data.c

$(IntermediateDirectory)/lib_crypto_crypto-ops-data.c$(PreprocessSuffix): lib/crypto/crypto-ops-data.c
	$(CC) $(CFLAGS) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_crypto_crypto-ops-data.c$(PreprocessSuffix) lib/crypto/crypto-ops-data.c

$(IntermediateDirectory)/lib_crypto_cn_slow_hash_hard_arm.cpp$(ObjectSuffix): lib/crypto/cn_slow_hash_hard_arm.cpp $(IntermediateDirectory)/lib_crypto_cn_slow_hash_hard_arm.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/crypto/cn_slow_hash_hard_arm.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_crypto_cn_slow_hash_hard_arm.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_crypto_cn_slow_hash_hard_arm.cpp$(DependSuffix): lib/crypto/cn_slow_hash_hard_arm.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_crypto_cn_slow_hash_hard_arm.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_crypto_cn_slow_hash_hard_arm.cpp$(DependSuffix) -MM lib/crypto/cn_slow_hash_hard_arm.cpp

$(IntermediateDirectory)/lib_crypto_cn_slow_hash_hard_arm.cpp$(PreprocessSuffix): lib/crypto/cn_slow_hash_hard_arm.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_crypto_cn_slow_hash_hard_arm.cpp$(PreprocessSuffix) lib/crypto/cn_slow_hash_hard_arm.cpp

$(IntermediateDirectory)/lib_crypto_hash.c$(ObjectSuffix): lib/crypto/hash.c $(IntermediateDirectory)/lib_crypto_hash.c$(DependSuffix)
	$(CC) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/crypto/hash.c" $(CFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_crypto_hash.c$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_crypto_hash.c$(DependSuffix): lib/crypto/hash.c
	@$(CC) $(CFLAGS) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_crypto_hash.c$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_crypto_hash.c$(DependSuffix) -MM lib/crypto/hash.c

$(IntermediateDirectory)/lib_crypto_hash.c$(PreprocessSuffix): lib/crypto/hash.c
	$(CC) $(CFLAGS) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_crypto_hash.c$(PreprocessSuffix) lib/crypto/hash.c

$(IntermediateDirectory)/lib_crypto_hash-extra-groestl.c$(ObjectSuffix): lib/crypto/hash-extra-groestl.c $(IntermediateDirectory)/lib_crypto_hash-extra-groestl.c$(DependSuffix)
	$(CC) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/crypto/hash-extra-groestl.c" $(CFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_crypto_hash-extra-groestl.c$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_crypto_hash-extra-groestl.c$(DependSuffix): lib/crypto/hash-extra-groestl.c
	@$(CC) $(CFLAGS) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_crypto_hash-extra-groestl.c$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_crypto_hash-extra-groestl.c$(DependSuffix) -MM lib/crypto/hash-extra-groestl.c

$(IntermediateDirectory)/lib_crypto_hash-extra-groestl.c$(PreprocessSuffix): lib/crypto/hash-extra-groestl.c
	$(CC) $(CFLAGS) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_crypto_hash-extra-groestl.c$(PreprocessSuffix) lib/crypto/hash-extra-groestl.c

$(IntermediateDirectory)/lib_crypto_skein.c$(ObjectSuffix): lib/crypto/skein.c $(IntermediateDirectory)/lib_crypto_skein.c$(DependSuffix)
	$(CC) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/crypto/skein.c" $(CFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_crypto_skein.c$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_crypto_skein.c$(DependSuffix): lib/crypto/skein.c
	@$(CC) $(CFLAGS) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_crypto_skein.c$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_crypto_skein.c$(DependSuffix) -MM lib/crypto/skein.c

$(IntermediateDirectory)/lib_crypto_skein.c$(PreprocessSuffix): lib/crypto/skein.c
	$(CC) $(CFLAGS) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_crypto_skein.c$(PreprocessSuffix) lib/crypto/skein.c

$(IntermediateDirectory)/lib_crypto_crypto.cpp$(ObjectSuffix): lib/crypto/crypto.cpp $(IntermediateDirectory)/lib_crypto_crypto.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/crypto/crypto.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_crypto_crypto.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_crypto_crypto.cpp$(DependSuffix): lib/crypto/crypto.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_crypto_crypto.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_crypto_crypto.cpp$(DependSuffix) -MM lib/crypto/crypto.cpp

$(IntermediateDirectory)/lib_crypto_crypto.cpp$(PreprocessSuffix): lib/crypto/crypto.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_crypto_crypto.cpp$(PreprocessSuffix) lib/crypto/crypto.cpp

$(IntermediateDirectory)/lib_CryptoNoteCore_MinerConfig.cpp$(ObjectSuffix): lib/CryptoNoteCore/MinerConfig.cpp $(IntermediateDirectory)/lib_CryptoNoteCore_MinerConfig.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/CryptoNoteCore/MinerConfig.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_CryptoNoteCore_MinerConfig.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_CryptoNoteCore_MinerConfig.cpp$(DependSuffix): lib/CryptoNoteCore/MinerConfig.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_CryptoNoteCore_MinerConfig.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_CryptoNoteCore_MinerConfig.cpp$(DependSuffix) -MM lib/CryptoNoteCore/MinerConfig.cpp

$(IntermediateDirectory)/lib_CryptoNoteCore_MinerConfig.cpp$(PreprocessSuffix): lib/CryptoNoteCore/MinerConfig.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_CryptoNoteCore_MinerConfig.cpp$(PreprocessSuffix) lib/CryptoNoteCore/MinerConfig.cpp

$(IntermediateDirectory)/lib_CryptoNoteCore_TransactionExtra.cpp$(ObjectSuffix): lib/CryptoNoteCore/TransactionExtra.cpp $(IntermediateDirectory)/lib_CryptoNoteCore_TransactionExtra.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/CryptoNoteCore/TransactionExtra.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_CryptoNoteCore_TransactionExtra.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_CryptoNoteCore_TransactionExtra.cpp$(DependSuffix): lib/CryptoNoteCore/TransactionExtra.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_CryptoNoteCore_TransactionExtra.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_CryptoNoteCore_TransactionExtra.cpp$(DependSuffix) -MM lib/CryptoNoteCore/TransactionExtra.cpp

$(IntermediateDirectory)/lib_CryptoNoteCore_TransactionExtra.cpp$(PreprocessSuffix): lib/CryptoNoteCore/TransactionExtra.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_CryptoNoteCore_TransactionExtra.cpp$(PreprocessSuffix) lib/CryptoNoteCore/TransactionExtra.cpp

$(IntermediateDirectory)/lib_CryptoNoteCore_Difficulty.cpp$(ObjectSuffix): lib/CryptoNoteCore/Difficulty.cpp $(IntermediateDirectory)/lib_CryptoNoteCore_Difficulty.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/CryptoNoteCore/Difficulty.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_CryptoNoteCore_Difficulty.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_CryptoNoteCore_Difficulty.cpp$(DependSuffix): lib/CryptoNoteCore/Difficulty.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_CryptoNoteCore_Difficulty.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_CryptoNoteCore_Difficulty.cpp$(DependSuffix) -MM lib/CryptoNoteCore/Difficulty.cpp

$(IntermediateDirectory)/lib_CryptoNoteCore_Difficulty.cpp$(PreprocessSuffix): lib/CryptoNoteCore/Difficulty.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_CryptoNoteCore_Difficulty.cpp$(PreprocessSuffix) lib/CryptoNoteCore/Difficulty.cpp

$(IntermediateDirectory)/lib_CryptoNoteCore_Blockchain.cpp$(ObjectSuffix): lib/CryptoNoteCore/Blockchain.cpp $(IntermediateDirectory)/lib_CryptoNoteCore_Blockchain.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/CryptoNoteCore/Blockchain.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_CryptoNoteCore_Blockchain.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_CryptoNoteCore_Blockchain.cpp$(DependSuffix): lib/CryptoNoteCore/Blockchain.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_CryptoNoteCore_Blockchain.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_CryptoNoteCore_Blockchain.cpp$(DependSuffix) -MM lib/CryptoNoteCore/Blockchain.cpp

$(IntermediateDirectory)/lib_CryptoNoteCore_Blockchain.cpp$(PreprocessSuffix): lib/CryptoNoteCore/Blockchain.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_CryptoNoteCore_Blockchain.cpp$(PreprocessSuffix) lib/CryptoNoteCore/Blockchain.cpp

$(IntermediateDirectory)/lib_CryptoNoteCore_Transaction.cpp$(ObjectSuffix): lib/CryptoNoteCore/Transaction.cpp $(IntermediateDirectory)/lib_CryptoNoteCore_Transaction.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/CryptoNoteCore/Transaction.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_CryptoNoteCore_Transaction.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_CryptoNoteCore_Transaction.cpp$(DependSuffix): lib/CryptoNoteCore/Transaction.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_CryptoNoteCore_Transaction.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_CryptoNoteCore_Transaction.cpp$(DependSuffix) -MM lib/CryptoNoteCore/Transaction.cpp

$(IntermediateDirectory)/lib_CryptoNoteCore_Transaction.cpp$(PreprocessSuffix): lib/CryptoNoteCore/Transaction.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_CryptoNoteCore_Transaction.cpp$(PreprocessSuffix) lib/CryptoNoteCore/Transaction.cpp

$(IntermediateDirectory)/lib_CryptoNoteCore_CryptoNoteSerialization.cpp$(ObjectSuffix): lib/CryptoNoteCore/CryptoNoteSerialization.cpp $(IntermediateDirectory)/lib_CryptoNoteCore_CryptoNoteSerialization.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/CryptoNoteCore/CryptoNoteSerialization.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_CryptoNoteCore_CryptoNoteSerialization.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_CryptoNoteCore_CryptoNoteSerialization.cpp$(DependSuffix): lib/CryptoNoteCore/CryptoNoteSerialization.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_CryptoNoteCore_CryptoNoteSerialization.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_CryptoNoteCore_CryptoNoteSerialization.cpp$(DependSuffix) -MM lib/CryptoNoteCore/CryptoNoteSerialization.cpp

$(IntermediateDirectory)/lib_CryptoNoteCore_CryptoNoteSerialization.cpp$(PreprocessSuffix): lib/CryptoNoteCore/CryptoNoteSerialization.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_CryptoNoteCore_CryptoNoteSerialization.cpp$(PreprocessSuffix) lib/CryptoNoteCore/CryptoNoteSerialization.cpp

$(IntermediateDirectory)/lib_CryptoNoteCore_CryptoNoteFormatUtils.cpp$(ObjectSuffix): lib/CryptoNoteCore/CryptoNoteFormatUtils.cpp $(IntermediateDirectory)/lib_CryptoNoteCore_CryptoNoteFormatUtils.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/CryptoNoteCore/CryptoNoteFormatUtils.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_CryptoNoteCore_CryptoNoteFormatUtils.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_CryptoNoteCore_CryptoNoteFormatUtils.cpp$(DependSuffix): lib/CryptoNoteCore/CryptoNoteFormatUtils.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_CryptoNoteCore_CryptoNoteFormatUtils.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_CryptoNoteCore_CryptoNoteFormatUtils.cpp$(DependSuffix) -MM lib/CryptoNoteCore/CryptoNoteFormatUtils.cpp

$(IntermediateDirectory)/lib_CryptoNoteCore_CryptoNoteFormatUtils.cpp$(PreprocessSuffix): lib/CryptoNoteCore/CryptoNoteFormatUtils.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_CryptoNoteCore_CryptoNoteFormatUtils.cpp$(PreprocessSuffix) lib/CryptoNoteCore/CryptoNoteFormatUtils.cpp

$(IntermediateDirectory)/lib_CryptoNoteCore_UpgradeDetector.cpp$(ObjectSuffix): lib/CryptoNoteCore/UpgradeDetector.cpp $(IntermediateDirectory)/lib_CryptoNoteCore_UpgradeDetector.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/CryptoNoteCore/UpgradeDetector.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_CryptoNoteCore_UpgradeDetector.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_CryptoNoteCore_UpgradeDetector.cpp$(DependSuffix): lib/CryptoNoteCore/UpgradeDetector.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_CryptoNoteCore_UpgradeDetector.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_CryptoNoteCore_UpgradeDetector.cpp$(DependSuffix) -MM lib/CryptoNoteCore/UpgradeDetector.cpp

$(IntermediateDirectory)/lib_CryptoNoteCore_UpgradeDetector.cpp$(PreprocessSuffix): lib/CryptoNoteCore/UpgradeDetector.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_CryptoNoteCore_UpgradeDetector.cpp$(PreprocessSuffix) lib/CryptoNoteCore/UpgradeDetector.cpp

$(IntermediateDirectory)/lib_CryptoNoteCore_BlockIndex.cpp$(ObjectSuffix): lib/CryptoNoteCore/BlockIndex.cpp $(IntermediateDirectory)/lib_CryptoNoteCore_BlockIndex.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/CryptoNoteCore/BlockIndex.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_CryptoNoteCore_BlockIndex.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_CryptoNoteCore_BlockIndex.cpp$(DependSuffix): lib/CryptoNoteCore/BlockIndex.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_CryptoNoteCore_BlockIndex.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_CryptoNoteCore_BlockIndex.cpp$(DependSuffix) -MM lib/CryptoNoteCore/BlockIndex.cpp

$(IntermediateDirectory)/lib_CryptoNoteCore_BlockIndex.cpp$(PreprocessSuffix): lib/CryptoNoteCore/BlockIndex.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_CryptoNoteCore_BlockIndex.cpp$(PreprocessSuffix) lib/CryptoNoteCore/BlockIndex.cpp

$(IntermediateDirectory)/lib_CryptoNoteCore_BlockchainIndices.cpp$(ObjectSuffix): lib/CryptoNoteCore/BlockchainIndices.cpp $(IntermediateDirectory)/lib_CryptoNoteCore_BlockchainIndices.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/CryptoNoteCore/BlockchainIndices.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_CryptoNoteCore_BlockchainIndices.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_CryptoNoteCore_BlockchainIndices.cpp$(DependSuffix): lib/CryptoNoteCore/BlockchainIndices.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_CryptoNoteCore_BlockchainIndices.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_CryptoNoteCore_BlockchainIndices.cpp$(DependSuffix) -MM lib/CryptoNoteCore/BlockchainIndices.cpp

$(IntermediateDirectory)/lib_CryptoNoteCore_BlockchainIndices.cpp$(PreprocessSuffix): lib/CryptoNoteCore/BlockchainIndices.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_CryptoNoteCore_BlockchainIndices.cpp$(PreprocessSuffix) lib/CryptoNoteCore/BlockchainIndices.cpp

$(IntermediateDirectory)/lib_CryptoNoteCore_CryptoNoteBasicImpl.cpp$(ObjectSuffix): lib/CryptoNoteCore/CryptoNoteBasicImpl.cpp $(IntermediateDirectory)/lib_CryptoNoteCore_CryptoNoteBasicImpl.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/CryptoNoteCore/CryptoNoteBasicImpl.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_CryptoNoteCore_CryptoNoteBasicImpl.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_CryptoNoteCore_CryptoNoteBasicImpl.cpp$(DependSuffix): lib/CryptoNoteCore/CryptoNoteBasicImpl.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_CryptoNoteCore_CryptoNoteBasicImpl.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_CryptoNoteCore_CryptoNoteBasicImpl.cpp$(DependSuffix) -MM lib/CryptoNoteCore/CryptoNoteBasicImpl.cpp

$(IntermediateDirectory)/lib_CryptoNoteCore_CryptoNoteBasicImpl.cpp$(PreprocessSuffix): lib/CryptoNoteCore/CryptoNoteBasicImpl.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_CryptoNoteCore_CryptoNoteBasicImpl.cpp$(PreprocessSuffix) lib/CryptoNoteCore/CryptoNoteBasicImpl.cpp

$(IntermediateDirectory)/lib_CryptoNoteCore_IBlock.cpp$(ObjectSuffix): lib/CryptoNoteCore/IBlock.cpp $(IntermediateDirectory)/lib_CryptoNoteCore_IBlock.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/CryptoNoteCore/IBlock.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_CryptoNoteCore_IBlock.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_CryptoNoteCore_IBlock.cpp$(DependSuffix): lib/CryptoNoteCore/IBlock.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_CryptoNoteCore_IBlock.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_CryptoNoteCore_IBlock.cpp$(DependSuffix) -MM lib/CryptoNoteCore/IBlock.cpp

$(IntermediateDirectory)/lib_CryptoNoteCore_IBlock.cpp$(PreprocessSuffix): lib/CryptoNoteCore/IBlock.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_CryptoNoteCore_IBlock.cpp$(PreprocessSuffix) lib/CryptoNoteCore/IBlock.cpp

$(IntermediateDirectory)/lib_CryptoNoteCore_Core.cpp$(ObjectSuffix): lib/CryptoNoteCore/Core.cpp $(IntermediateDirectory)/lib_CryptoNoteCore_Core.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/CryptoNoteCore/Core.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_CryptoNoteCore_Core.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_CryptoNoteCore_Core.cpp$(DependSuffix): lib/CryptoNoteCore/Core.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_CryptoNoteCore_Core.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_CryptoNoteCore_Core.cpp$(DependSuffix) -MM lib/CryptoNoteCore/Core.cpp

$(IntermediateDirectory)/lib_CryptoNoteCore_Core.cpp$(PreprocessSuffix): lib/CryptoNoteCore/Core.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_CryptoNoteCore_Core.cpp$(PreprocessSuffix) lib/CryptoNoteCore/Core.cpp

$(IntermediateDirectory)/lib_CryptoNoteCore_Currency.cpp$(ObjectSuffix): lib/CryptoNoteCore/Currency.cpp $(IntermediateDirectory)/lib_CryptoNoteCore_Currency.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/CryptoNoteCore/Currency.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_CryptoNoteCore_Currency.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_CryptoNoteCore_Currency.cpp$(DependSuffix): lib/CryptoNoteCore/Currency.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_CryptoNoteCore_Currency.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_CryptoNoteCore_Currency.cpp$(DependSuffix) -MM lib/CryptoNoteCore/Currency.cpp

$(IntermediateDirectory)/lib_CryptoNoteCore_Currency.cpp$(PreprocessSuffix): lib/CryptoNoteCore/Currency.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_CryptoNoteCore_Currency.cpp$(PreprocessSuffix) lib/CryptoNoteCore/Currency.cpp

$(IntermediateDirectory)/lib_CryptoNoteCore_CryptoNoteBasic.cpp$(ObjectSuffix): lib/CryptoNoteCore/CryptoNoteBasic.cpp $(IntermediateDirectory)/lib_CryptoNoteCore_CryptoNoteBasic.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/CryptoNoteCore/CryptoNoteBasic.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_CryptoNoteCore_CryptoNoteBasic.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_CryptoNoteCore_CryptoNoteBasic.cpp$(DependSuffix): lib/CryptoNoteCore/CryptoNoteBasic.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_CryptoNoteCore_CryptoNoteBasic.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_CryptoNoteCore_CryptoNoteBasic.cpp$(DependSuffix) -MM lib/CryptoNoteCore/CryptoNoteBasic.cpp

$(IntermediateDirectory)/lib_CryptoNoteCore_CryptoNoteBasic.cpp$(PreprocessSuffix): lib/CryptoNoteCore/CryptoNoteBasic.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_CryptoNoteCore_CryptoNoteBasic.cpp$(PreprocessSuffix) lib/CryptoNoteCore/CryptoNoteBasic.cpp

$(IntermediateDirectory)/lib_CryptoNoteCore_Miner.cpp$(ObjectSuffix): lib/CryptoNoteCore/Miner.cpp $(IntermediateDirectory)/lib_CryptoNoteCore_Miner.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/CryptoNoteCore/Miner.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_CryptoNoteCore_Miner.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_CryptoNoteCore_Miner.cpp$(DependSuffix): lib/CryptoNoteCore/Miner.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_CryptoNoteCore_Miner.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_CryptoNoteCore_Miner.cpp$(DependSuffix) -MM lib/CryptoNoteCore/Miner.cpp

$(IntermediateDirectory)/lib_CryptoNoteCore_Miner.cpp$(PreprocessSuffix): lib/CryptoNoteCore/Miner.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_CryptoNoteCore_Miner.cpp$(PreprocessSuffix) lib/CryptoNoteCore/Miner.cpp

$(IntermediateDirectory)/lib_CryptoNoteCore_CryptoNoteTools.cpp$(ObjectSuffix): lib/CryptoNoteCore/CryptoNoteTools.cpp $(IntermediateDirectory)/lib_CryptoNoteCore_CryptoNoteTools.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/CryptoNoteCore/CryptoNoteTools.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_CryptoNoteCore_CryptoNoteTools.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_CryptoNoteCore_CryptoNoteTools.cpp$(DependSuffix): lib/CryptoNoteCore/CryptoNoteTools.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_CryptoNoteCore_CryptoNoteTools.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_CryptoNoteCore_CryptoNoteTools.cpp$(DependSuffix) -MM lib/CryptoNoteCore/CryptoNoteTools.cpp

$(IntermediateDirectory)/lib_CryptoNoteCore_CryptoNoteTools.cpp$(PreprocessSuffix): lib/CryptoNoteCore/CryptoNoteTools.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_CryptoNoteCore_CryptoNoteTools.cpp$(PreprocessSuffix) lib/CryptoNoteCore/CryptoNoteTools.cpp

$(IntermediateDirectory)/lib_CryptoNoteCore_TransactionPool.cpp$(ObjectSuffix): lib/CryptoNoteCore/TransactionPool.cpp $(IntermediateDirectory)/lib_CryptoNoteCore_TransactionPool.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/CryptoNoteCore/TransactionPool.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_CryptoNoteCore_TransactionPool.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_CryptoNoteCore_TransactionPool.cpp$(DependSuffix): lib/CryptoNoteCore/TransactionPool.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_CryptoNoteCore_TransactionPool.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_CryptoNoteCore_TransactionPool.cpp$(DependSuffix) -MM lib/CryptoNoteCore/TransactionPool.cpp

$(IntermediateDirectory)/lib_CryptoNoteCore_TransactionPool.cpp$(PreprocessSuffix): lib/CryptoNoteCore/TransactionPool.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_CryptoNoteCore_TransactionPool.cpp$(PreprocessSuffix) lib/CryptoNoteCore/TransactionPool.cpp

$(IntermediateDirectory)/lib_CryptoNoteCore_TransactionPrefixImpl.cpp$(ObjectSuffix): lib/CryptoNoteCore/TransactionPrefixImpl.cpp $(IntermediateDirectory)/lib_CryptoNoteCore_TransactionPrefixImpl.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/CryptoNoteCore/TransactionPrefixImpl.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_CryptoNoteCore_TransactionPrefixImpl.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_CryptoNoteCore_TransactionPrefixImpl.cpp$(DependSuffix): lib/CryptoNoteCore/TransactionPrefixImpl.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_CryptoNoteCore_TransactionPrefixImpl.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_CryptoNoteCore_TransactionPrefixImpl.cpp$(DependSuffix) -MM lib/CryptoNoteCore/TransactionPrefixImpl.cpp

$(IntermediateDirectory)/lib_CryptoNoteCore_TransactionPrefixImpl.cpp$(PreprocessSuffix): lib/CryptoNoteCore/TransactionPrefixImpl.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_CryptoNoteCore_TransactionPrefixImpl.cpp$(PreprocessSuffix) lib/CryptoNoteCore/TransactionPrefixImpl.cpp

$(IntermediateDirectory)/lib_CryptoNoteCore_ITimeProvider.cpp$(ObjectSuffix): lib/CryptoNoteCore/ITimeProvider.cpp $(IntermediateDirectory)/lib_CryptoNoteCore_ITimeProvider.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/CryptoNoteCore/ITimeProvider.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_CryptoNoteCore_ITimeProvider.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_CryptoNoteCore_ITimeProvider.cpp$(DependSuffix): lib/CryptoNoteCore/ITimeProvider.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_CryptoNoteCore_ITimeProvider.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_CryptoNoteCore_ITimeProvider.cpp$(DependSuffix) -MM lib/CryptoNoteCore/ITimeProvider.cpp

$(IntermediateDirectory)/lib_CryptoNoteCore_ITimeProvider.cpp$(PreprocessSuffix): lib/CryptoNoteCore/ITimeProvider.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_CryptoNoteCore_ITimeProvider.cpp$(PreprocessSuffix) lib/CryptoNoteCore/ITimeProvider.cpp

$(IntermediateDirectory)/lib_CryptoNoteCore_Account.cpp$(ObjectSuffix): lib/CryptoNoteCore/Account.cpp $(IntermediateDirectory)/lib_CryptoNoteCore_Account.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/CryptoNoteCore/Account.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_CryptoNoteCore_Account.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_CryptoNoteCore_Account.cpp$(DependSuffix): lib/CryptoNoteCore/Account.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_CryptoNoteCore_Account.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_CryptoNoteCore_Account.cpp$(DependSuffix) -MM lib/CryptoNoteCore/Account.cpp

$(IntermediateDirectory)/lib_CryptoNoteCore_Account.cpp$(PreprocessSuffix): lib/CryptoNoteCore/Account.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_CryptoNoteCore_Account.cpp$(PreprocessSuffix) lib/CryptoNoteCore/Account.cpp

$(IntermediateDirectory)/lib_CryptoNoteCore_BlockchainMessages.cpp$(ObjectSuffix): lib/CryptoNoteCore/BlockchainMessages.cpp $(IntermediateDirectory)/lib_CryptoNoteCore_BlockchainMessages.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/CryptoNoteCore/BlockchainMessages.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_CryptoNoteCore_BlockchainMessages.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_CryptoNoteCore_BlockchainMessages.cpp$(DependSuffix): lib/CryptoNoteCore/BlockchainMessages.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_CryptoNoteCore_BlockchainMessages.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_CryptoNoteCore_BlockchainMessages.cpp$(DependSuffix) -MM lib/CryptoNoteCore/BlockchainMessages.cpp

$(IntermediateDirectory)/lib_CryptoNoteCore_BlockchainMessages.cpp$(PreprocessSuffix): lib/CryptoNoteCore/BlockchainMessages.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_CryptoNoteCore_BlockchainMessages.cpp$(PreprocessSuffix) lib/CryptoNoteCore/BlockchainMessages.cpp

$(IntermediateDirectory)/lib_CryptoNoteCore_TransactionUtils.cpp$(ObjectSuffix): lib/CryptoNoteCore/TransactionUtils.cpp $(IntermediateDirectory)/lib_CryptoNoteCore_TransactionUtils.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/CryptoNoteCore/TransactionUtils.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_CryptoNoteCore_TransactionUtils.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_CryptoNoteCore_TransactionUtils.cpp$(DependSuffix): lib/CryptoNoteCore/TransactionUtils.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_CryptoNoteCore_TransactionUtils.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_CryptoNoteCore_TransactionUtils.cpp$(DependSuffix) -MM lib/CryptoNoteCore/TransactionUtils.cpp

$(IntermediateDirectory)/lib_CryptoNoteCore_TransactionUtils.cpp$(PreprocessSuffix): lib/CryptoNoteCore/TransactionUtils.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_CryptoNoteCore_TransactionUtils.cpp$(PreprocessSuffix) lib/CryptoNoteCore/TransactionUtils.cpp

$(IntermediateDirectory)/lib_CryptoNoteCore_SwappedMap.cpp$(ObjectSuffix): lib/CryptoNoteCore/SwappedMap.cpp $(IntermediateDirectory)/lib_CryptoNoteCore_SwappedMap.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/CryptoNoteCore/SwappedMap.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_CryptoNoteCore_SwappedMap.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_CryptoNoteCore_SwappedMap.cpp$(DependSuffix): lib/CryptoNoteCore/SwappedMap.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_CryptoNoteCore_SwappedMap.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_CryptoNoteCore_SwappedMap.cpp$(DependSuffix) -MM lib/CryptoNoteCore/SwappedMap.cpp

$(IntermediateDirectory)/lib_CryptoNoteCore_SwappedMap.cpp$(PreprocessSuffix): lib/CryptoNoteCore/SwappedMap.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_CryptoNoteCore_SwappedMap.cpp$(PreprocessSuffix) lib/CryptoNoteCore/SwappedMap.cpp

$(IntermediateDirectory)/lib_CryptoNoteCore_SwappedVector.cpp$(ObjectSuffix): lib/CryptoNoteCore/SwappedVector.cpp $(IntermediateDirectory)/lib_CryptoNoteCore_SwappedVector.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/CryptoNoteCore/SwappedVector.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_CryptoNoteCore_SwappedVector.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_CryptoNoteCore_SwappedVector.cpp$(DependSuffix): lib/CryptoNoteCore/SwappedVector.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_CryptoNoteCore_SwappedVector.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_CryptoNoteCore_SwappedVector.cpp$(DependSuffix) -MM lib/CryptoNoteCore/SwappedVector.cpp

$(IntermediateDirectory)/lib_CryptoNoteCore_SwappedVector.cpp$(PreprocessSuffix): lib/CryptoNoteCore/SwappedVector.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_CryptoNoteCore_SwappedVector.cpp$(PreprocessSuffix) lib/CryptoNoteCore/SwappedVector.cpp

$(IntermediateDirectory)/lib_CryptoNoteCore_Checkpoints.cpp$(ObjectSuffix): lib/CryptoNoteCore/Checkpoints.cpp $(IntermediateDirectory)/lib_CryptoNoteCore_Checkpoints.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/CryptoNoteCore/Checkpoints.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_CryptoNoteCore_Checkpoints.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_CryptoNoteCore_Checkpoints.cpp$(DependSuffix): lib/CryptoNoteCore/Checkpoints.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_CryptoNoteCore_Checkpoints.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_CryptoNoteCore_Checkpoints.cpp$(DependSuffix) -MM lib/CryptoNoteCore/Checkpoints.cpp

$(IntermediateDirectory)/lib_CryptoNoteCore_Checkpoints.cpp$(PreprocessSuffix): lib/CryptoNoteCore/Checkpoints.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_CryptoNoteCore_Checkpoints.cpp$(PreprocessSuffix) lib/CryptoNoteCore/Checkpoints.cpp

$(IntermediateDirectory)/lib_CryptoNoteCore_CoreConfig.cpp$(ObjectSuffix): lib/CryptoNoteCore/CoreConfig.cpp $(IntermediateDirectory)/lib_CryptoNoteCore_CoreConfig.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/CryptoNoteCore/CoreConfig.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_CryptoNoteCore_CoreConfig.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_CryptoNoteCore_CoreConfig.cpp$(DependSuffix): lib/CryptoNoteCore/CoreConfig.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_CryptoNoteCore_CoreConfig.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_CryptoNoteCore_CoreConfig.cpp$(DependSuffix) -MM lib/CryptoNoteCore/CoreConfig.cpp

$(IntermediateDirectory)/lib_CryptoNoteCore_CoreConfig.cpp$(PreprocessSuffix): lib/CryptoNoteCore/CoreConfig.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_CryptoNoteCore_CoreConfig.cpp$(PreprocessSuffix) lib/CryptoNoteCore/CoreConfig.cpp

$(IntermediateDirectory)/lib_Rpc_RpcServerConfig.cpp$(ObjectSuffix): lib/Rpc/RpcServerConfig.cpp $(IntermediateDirectory)/lib_Rpc_RpcServerConfig.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Rpc/RpcServerConfig.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Rpc_RpcServerConfig.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Rpc_RpcServerConfig.cpp$(DependSuffix): lib/Rpc/RpcServerConfig.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Rpc_RpcServerConfig.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Rpc_RpcServerConfig.cpp$(DependSuffix) -MM lib/Rpc/RpcServerConfig.cpp

$(IntermediateDirectory)/lib_Rpc_RpcServerConfig.cpp$(PreprocessSuffix): lib/Rpc/RpcServerConfig.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Rpc_RpcServerConfig.cpp$(PreprocessSuffix) lib/Rpc/RpcServerConfig.cpp

$(IntermediateDirectory)/lib_Rpc_JsonRpc.cpp$(ObjectSuffix): lib/Rpc/JsonRpc.cpp $(IntermediateDirectory)/lib_Rpc_JsonRpc.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Rpc/JsonRpc.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Rpc_JsonRpc.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Rpc_JsonRpc.cpp$(DependSuffix): lib/Rpc/JsonRpc.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Rpc_JsonRpc.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Rpc_JsonRpc.cpp$(DependSuffix) -MM lib/Rpc/JsonRpc.cpp

$(IntermediateDirectory)/lib_Rpc_JsonRpc.cpp$(PreprocessSuffix): lib/Rpc/JsonRpc.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Rpc_JsonRpc.cpp$(PreprocessSuffix) lib/Rpc/JsonRpc.cpp

$(IntermediateDirectory)/lib_Rpc_RpcServer.cpp$(ObjectSuffix): lib/Rpc/RpcServer.cpp $(IntermediateDirectory)/lib_Rpc_RpcServer.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Rpc/RpcServer.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Rpc_RpcServer.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Rpc_RpcServer.cpp$(DependSuffix): lib/Rpc/RpcServer.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Rpc_RpcServer.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Rpc_RpcServer.cpp$(DependSuffix) -MM lib/Rpc/RpcServer.cpp

$(IntermediateDirectory)/lib_Rpc_RpcServer.cpp$(PreprocessSuffix): lib/Rpc/RpcServer.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Rpc_RpcServer.cpp$(PreprocessSuffix) lib/Rpc/RpcServer.cpp

$(IntermediateDirectory)/lib_Rpc_HttpServer.cpp$(ObjectSuffix): lib/Rpc/HttpServer.cpp $(IntermediateDirectory)/lib_Rpc_HttpServer.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Rpc/HttpServer.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Rpc_HttpServer.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Rpc_HttpServer.cpp$(DependSuffix): lib/Rpc/HttpServer.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Rpc_HttpServer.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Rpc_HttpServer.cpp$(DependSuffix) -MM lib/Rpc/HttpServer.cpp

$(IntermediateDirectory)/lib_Rpc_HttpServer.cpp$(PreprocessSuffix): lib/Rpc/HttpServer.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Rpc_HttpServer.cpp$(PreprocessSuffix) lib/Rpc/HttpServer.cpp

$(IntermediateDirectory)/lib_Rpc_HttpClient.cpp$(ObjectSuffix): lib/Rpc/HttpClient.cpp $(IntermediateDirectory)/lib_Rpc_HttpClient.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Rpc/HttpClient.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Rpc_HttpClient.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Rpc_HttpClient.cpp$(DependSuffix): lib/Rpc/HttpClient.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Rpc_HttpClient.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Rpc_HttpClient.cpp$(DependSuffix) -MM lib/Rpc/HttpClient.cpp

$(IntermediateDirectory)/lib_Rpc_HttpClient.cpp$(PreprocessSuffix): lib/Rpc/HttpClient.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Rpc_HttpClient.cpp$(PreprocessSuffix) lib/Rpc/HttpClient.cpp

$(IntermediateDirectory)/lib_HTTP_HttpParserErrorCodes.cpp$(ObjectSuffix): lib/HTTP/HttpParserErrorCodes.cpp $(IntermediateDirectory)/lib_HTTP_HttpParserErrorCodes.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/HTTP/HttpParserErrorCodes.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_HTTP_HttpParserErrorCodes.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_HTTP_HttpParserErrorCodes.cpp$(DependSuffix): lib/HTTP/HttpParserErrorCodes.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_HTTP_HttpParserErrorCodes.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_HTTP_HttpParserErrorCodes.cpp$(DependSuffix) -MM lib/HTTP/HttpParserErrorCodes.cpp

$(IntermediateDirectory)/lib_HTTP_HttpParserErrorCodes.cpp$(PreprocessSuffix): lib/HTTP/HttpParserErrorCodes.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_HTTP_HttpParserErrorCodes.cpp$(PreprocessSuffix) lib/HTTP/HttpParserErrorCodes.cpp

$(IntermediateDirectory)/lib_HTTP_HttpParser.cpp$(ObjectSuffix): lib/HTTP/HttpParser.cpp $(IntermediateDirectory)/lib_HTTP_HttpParser.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/HTTP/HttpParser.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_HTTP_HttpParser.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_HTTP_HttpParser.cpp$(DependSuffix): lib/HTTP/HttpParser.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_HTTP_HttpParser.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_HTTP_HttpParser.cpp$(DependSuffix) -MM lib/HTTP/HttpParser.cpp

$(IntermediateDirectory)/lib_HTTP_HttpParser.cpp$(PreprocessSuffix): lib/HTTP/HttpParser.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_HTTP_HttpParser.cpp$(PreprocessSuffix) lib/HTTP/HttpParser.cpp

$(IntermediateDirectory)/lib_HTTP_HttpRequest.cpp$(ObjectSuffix): lib/HTTP/HttpRequest.cpp $(IntermediateDirectory)/lib_HTTP_HttpRequest.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/HTTP/HttpRequest.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_HTTP_HttpRequest.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_HTTP_HttpRequest.cpp$(DependSuffix): lib/HTTP/HttpRequest.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_HTTP_HttpRequest.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_HTTP_HttpRequest.cpp$(DependSuffix) -MM lib/HTTP/HttpRequest.cpp

$(IntermediateDirectory)/lib_HTTP_HttpRequest.cpp$(PreprocessSuffix): lib/HTTP/HttpRequest.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_HTTP_HttpRequest.cpp$(PreprocessSuffix) lib/HTTP/HttpRequest.cpp

$(IntermediateDirectory)/lib_HTTP_HttpResponse.cpp$(ObjectSuffix): lib/HTTP/HttpResponse.cpp $(IntermediateDirectory)/lib_HTTP_HttpResponse.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/HTTP/HttpResponse.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_HTTP_HttpResponse.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_HTTP_HttpResponse.cpp$(DependSuffix): lib/HTTP/HttpResponse.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_HTTP_HttpResponse.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_HTTP_HttpResponse.cpp$(DependSuffix) -MM lib/HTTP/HttpResponse.cpp

$(IntermediateDirectory)/lib_HTTP_HttpResponse.cpp$(PreprocessSuffix): lib/HTTP/HttpResponse.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_HTTP_HttpResponse.cpp$(PreprocessSuffix) lib/HTTP/HttpResponse.cpp

$(IntermediateDirectory)/lib_NodeRpcProxy_NodeRpcProxy.cpp$(ObjectSuffix): lib/NodeRpcProxy/NodeRpcProxy.cpp $(IntermediateDirectory)/lib_NodeRpcProxy_NodeRpcProxy.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/NodeRpcProxy/NodeRpcProxy.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_NodeRpcProxy_NodeRpcProxy.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_NodeRpcProxy_NodeRpcProxy.cpp$(DependSuffix): lib/NodeRpcProxy/NodeRpcProxy.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_NodeRpcProxy_NodeRpcProxy.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_NodeRpcProxy_NodeRpcProxy.cpp$(DependSuffix) -MM lib/NodeRpcProxy/NodeRpcProxy.cpp

$(IntermediateDirectory)/lib_NodeRpcProxy_NodeRpcProxy.cpp$(PreprocessSuffix): lib/NodeRpcProxy/NodeRpcProxy.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_NodeRpcProxy_NodeRpcProxy.cpp$(PreprocessSuffix) lib/NodeRpcProxy/NodeRpcProxy.cpp

$(IntermediateDirectory)/lib_NodeRpcProxy_NodeErrors.cpp$(ObjectSuffix): lib/NodeRpcProxy/NodeErrors.cpp $(IntermediateDirectory)/lib_NodeRpcProxy_NodeErrors.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/NodeRpcProxy/NodeErrors.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_NodeRpcProxy_NodeErrors.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_NodeRpcProxy_NodeErrors.cpp$(DependSuffix): lib/NodeRpcProxy/NodeErrors.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_NodeRpcProxy_NodeErrors.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_NodeRpcProxy_NodeErrors.cpp$(DependSuffix) -MM lib/NodeRpcProxy/NodeErrors.cpp

$(IntermediateDirectory)/lib_NodeRpcProxy_NodeErrors.cpp$(PreprocessSuffix): lib/NodeRpcProxy/NodeErrors.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_NodeRpcProxy_NodeErrors.cpp$(PreprocessSuffix) lib/NodeRpcProxy/NodeErrors.cpp

$(IntermediateDirectory)/lib_Mnemonics_electrum-words.cpp$(ObjectSuffix): lib/Mnemonics/electrum-words.cpp $(IntermediateDirectory)/lib_Mnemonics_electrum-words.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Mnemonics/electrum-words.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Mnemonics_electrum-words.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Mnemonics_electrum-words.cpp$(DependSuffix): lib/Mnemonics/electrum-words.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Mnemonics_electrum-words.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Mnemonics_electrum-words.cpp$(DependSuffix) -MM lib/Mnemonics/electrum-words.cpp

$(IntermediateDirectory)/lib_Mnemonics_electrum-words.cpp$(PreprocessSuffix): lib/Mnemonics/electrum-words.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Mnemonics_electrum-words.cpp$(PreprocessSuffix) lib/Mnemonics/electrum-words.cpp

$(IntermediateDirectory)/lib_Wallet_LegacyKeysImporter.cpp$(ObjectSuffix): lib/Wallet/LegacyKeysImporter.cpp $(IntermediateDirectory)/lib_Wallet_LegacyKeysImporter.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Wallet/LegacyKeysImporter.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Wallet_LegacyKeysImporter.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Wallet_LegacyKeysImporter.cpp$(DependSuffix): lib/Wallet/LegacyKeysImporter.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Wallet_LegacyKeysImporter.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Wallet_LegacyKeysImporter.cpp$(DependSuffix) -MM lib/Wallet/LegacyKeysImporter.cpp

$(IntermediateDirectory)/lib_Wallet_LegacyKeysImporter.cpp$(PreprocessSuffix): lib/Wallet/LegacyKeysImporter.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Wallet_LegacyKeysImporter.cpp$(PreprocessSuffix) lib/Wallet/LegacyKeysImporter.cpp

$(IntermediateDirectory)/lib_Wallet_WalletErrors.cpp$(ObjectSuffix): lib/Wallet/WalletErrors.cpp $(IntermediateDirectory)/lib_Wallet_WalletErrors.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Wallet/WalletErrors.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Wallet_WalletErrors.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Wallet_WalletErrors.cpp$(DependSuffix): lib/Wallet/WalletErrors.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Wallet_WalletErrors.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Wallet_WalletErrors.cpp$(DependSuffix) -MM lib/Wallet/WalletErrors.cpp

$(IntermediateDirectory)/lib_Wallet_WalletErrors.cpp$(PreprocessSuffix): lib/Wallet/WalletErrors.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Wallet_WalletErrors.cpp$(PreprocessSuffix) lib/Wallet/WalletErrors.cpp

$(IntermediateDirectory)/lib_Wallet_WalletGreen.cpp$(ObjectSuffix): lib/Wallet/WalletGreen.cpp $(IntermediateDirectory)/lib_Wallet_WalletGreen.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Wallet/WalletGreen.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Wallet_WalletGreen.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Wallet_WalletGreen.cpp$(DependSuffix): lib/Wallet/WalletGreen.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Wallet_WalletGreen.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Wallet_WalletGreen.cpp$(DependSuffix) -MM lib/Wallet/WalletGreen.cpp

$(IntermediateDirectory)/lib_Wallet_WalletGreen.cpp$(PreprocessSuffix): lib/Wallet/WalletGreen.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Wallet_WalletGreen.cpp$(PreprocessSuffix) lib/Wallet/WalletGreen.cpp

$(IntermediateDirectory)/lib_Wallet_WalletRpcServer.cpp$(ObjectSuffix): lib/Wallet/WalletRpcServer.cpp $(IntermediateDirectory)/lib_Wallet_WalletRpcServer.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Wallet/WalletRpcServer.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Wallet_WalletRpcServer.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Wallet_WalletRpcServer.cpp$(DependSuffix): lib/Wallet/WalletRpcServer.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Wallet_WalletRpcServer.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Wallet_WalletRpcServer.cpp$(DependSuffix) -MM lib/Wallet/WalletRpcServer.cpp

$(IntermediateDirectory)/lib_Wallet_WalletRpcServer.cpp$(PreprocessSuffix): lib/Wallet/WalletRpcServer.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Wallet_WalletRpcServer.cpp$(PreprocessSuffix) lib/Wallet/WalletRpcServer.cpp

$(IntermediateDirectory)/lib_Wallet_WalletUtils.cpp$(ObjectSuffix): lib/Wallet/WalletUtils.cpp $(IntermediateDirectory)/lib_Wallet_WalletUtils.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Wallet/WalletUtils.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Wallet_WalletUtils.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Wallet_WalletUtils.cpp$(DependSuffix): lib/Wallet/WalletUtils.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Wallet_WalletUtils.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Wallet_WalletUtils.cpp$(DependSuffix) -MM lib/Wallet/WalletUtils.cpp

$(IntermediateDirectory)/lib_Wallet_WalletUtils.cpp$(PreprocessSuffix): lib/Wallet/WalletUtils.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Wallet_WalletUtils.cpp$(PreprocessSuffix) lib/Wallet/WalletUtils.cpp

$(IntermediateDirectory)/lib_Wallet_WalletAsyncContextCounter.cpp$(ObjectSuffix): lib/Wallet/WalletAsyncContextCounter.cpp $(IntermediateDirectory)/lib_Wallet_WalletAsyncContextCounter.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Wallet/WalletAsyncContextCounter.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Wallet_WalletAsyncContextCounter.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Wallet_WalletAsyncContextCounter.cpp$(DependSuffix): lib/Wallet/WalletAsyncContextCounter.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Wallet_WalletAsyncContextCounter.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Wallet_WalletAsyncContextCounter.cpp$(DependSuffix) -MM lib/Wallet/WalletAsyncContextCounter.cpp

$(IntermediateDirectory)/lib_Wallet_WalletAsyncContextCounter.cpp$(PreprocessSuffix): lib/Wallet/WalletAsyncContextCounter.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Wallet_WalletAsyncContextCounter.cpp$(PreprocessSuffix) lib/Wallet/WalletAsyncContextCounter.cpp

$(IntermediateDirectory)/lib_Wallet_WalletSerializationV2.cpp$(ObjectSuffix): lib/Wallet/WalletSerializationV2.cpp $(IntermediateDirectory)/lib_Wallet_WalletSerializationV2.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Wallet/WalletSerializationV2.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Wallet_WalletSerializationV2.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Wallet_WalletSerializationV2.cpp$(DependSuffix): lib/Wallet/WalletSerializationV2.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Wallet_WalletSerializationV2.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Wallet_WalletSerializationV2.cpp$(DependSuffix) -MM lib/Wallet/WalletSerializationV2.cpp

$(IntermediateDirectory)/lib_Wallet_WalletSerializationV2.cpp$(PreprocessSuffix): lib/Wallet/WalletSerializationV2.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Wallet_WalletSerializationV2.cpp$(PreprocessSuffix) lib/Wallet/WalletSerializationV2.cpp

$(IntermediateDirectory)/lib_Wallet_WalletSerializationV1.cpp$(ObjectSuffix): lib/Wallet/WalletSerializationV1.cpp $(IntermediateDirectory)/lib_Wallet_WalletSerializationV1.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Wallet/WalletSerializationV1.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Wallet_WalletSerializationV1.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Wallet_WalletSerializationV1.cpp$(DependSuffix): lib/Wallet/WalletSerializationV1.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Wallet_WalletSerializationV1.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Wallet_WalletSerializationV1.cpp$(DependSuffix) -MM lib/Wallet/WalletSerializationV1.cpp

$(IntermediateDirectory)/lib_Wallet_WalletSerializationV1.cpp$(PreprocessSuffix): lib/Wallet/WalletSerializationV1.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Wallet_WalletSerializationV1.cpp$(PreprocessSuffix) lib/Wallet/WalletSerializationV1.cpp

$(IntermediateDirectory)/lib_Common_StringTools.cpp$(ObjectSuffix): lib/Common/StringTools.cpp $(IntermediateDirectory)/lib_Common_StringTools.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Common/StringTools.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Common_StringTools.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Common_StringTools.cpp$(DependSuffix): lib/Common/StringTools.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Common_StringTools.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Common_StringTools.cpp$(DependSuffix) -MM lib/Common/StringTools.cpp

$(IntermediateDirectory)/lib_Common_StringTools.cpp$(PreprocessSuffix): lib/Common/StringTools.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Common_StringTools.cpp$(PreprocessSuffix) lib/Common/StringTools.cpp

$(IntermediateDirectory)/lib_Common_IInputStream.cpp$(ObjectSuffix): lib/Common/IInputStream.cpp $(IntermediateDirectory)/lib_Common_IInputStream.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Common/IInputStream.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Common_IInputStream.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Common_IInputStream.cpp$(DependSuffix): lib/Common/IInputStream.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Common_IInputStream.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Common_IInputStream.cpp$(DependSuffix) -MM lib/Common/IInputStream.cpp

$(IntermediateDirectory)/lib_Common_IInputStream.cpp$(PreprocessSuffix): lib/Common/IInputStream.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Common_IInputStream.cpp$(PreprocessSuffix) lib/Common/IInputStream.cpp

$(IntermediateDirectory)/lib_Common_ScopeExit.cpp$(ObjectSuffix): lib/Common/ScopeExit.cpp $(IntermediateDirectory)/lib_Common_ScopeExit.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Common/ScopeExit.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Common_ScopeExit.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Common_ScopeExit.cpp$(DependSuffix): lib/Common/ScopeExit.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Common_ScopeExit.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Common_ScopeExit.cpp$(DependSuffix) -MM lib/Common/ScopeExit.cpp

$(IntermediateDirectory)/lib_Common_ScopeExit.cpp$(PreprocessSuffix): lib/Common/ScopeExit.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Common_ScopeExit.cpp$(PreprocessSuffix) lib/Common/ScopeExit.cpp

$(IntermediateDirectory)/lib_Common_CommandLine.cpp$(ObjectSuffix): lib/Common/CommandLine.cpp $(IntermediateDirectory)/lib_Common_CommandLine.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Common/CommandLine.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Common_CommandLine.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Common_CommandLine.cpp$(DependSuffix): lib/Common/CommandLine.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Common_CommandLine.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Common_CommandLine.cpp$(DependSuffix) -MM lib/Common/CommandLine.cpp

$(IntermediateDirectory)/lib_Common_CommandLine.cpp$(PreprocessSuffix): lib/Common/CommandLine.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Common_CommandLine.cpp$(PreprocessSuffix) lib/Common/CommandLine.cpp

$(IntermediateDirectory)/lib_Common_JsonValue.cpp$(ObjectSuffix): lib/Common/JsonValue.cpp $(IntermediateDirectory)/lib_Common_JsonValue.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Common/JsonValue.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Common_JsonValue.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Common_JsonValue.cpp$(DependSuffix): lib/Common/JsonValue.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Common_JsonValue.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Common_JsonValue.cpp$(DependSuffix) -MM lib/Common/JsonValue.cpp

$(IntermediateDirectory)/lib_Common_JsonValue.cpp$(PreprocessSuffix): lib/Common/JsonValue.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Common_JsonValue.cpp$(PreprocessSuffix) lib/Common/JsonValue.cpp

$(IntermediateDirectory)/lib_Common_ConsoleHandler.cpp$(ObjectSuffix): lib/Common/ConsoleHandler.cpp $(IntermediateDirectory)/lib_Common_ConsoleHandler.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Common/ConsoleHandler.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Common_ConsoleHandler.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Common_ConsoleHandler.cpp$(DependSuffix): lib/Common/ConsoleHandler.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Common_ConsoleHandler.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Common_ConsoleHandler.cpp$(DependSuffix) -MM lib/Common/ConsoleHandler.cpp

$(IntermediateDirectory)/lib_Common_ConsoleHandler.cpp$(PreprocessSuffix): lib/Common/ConsoleHandler.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Common_ConsoleHandler.cpp$(PreprocessSuffix) lib/Common/ConsoleHandler.cpp

$(IntermediateDirectory)/lib_Common_StringInputStream.cpp$(ObjectSuffix): lib/Common/StringInputStream.cpp $(IntermediateDirectory)/lib_Common_StringInputStream.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Common/StringInputStream.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Common_StringInputStream.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Common_StringInputStream.cpp$(DependSuffix): lib/Common/StringInputStream.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Common_StringInputStream.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Common_StringInputStream.cpp$(DependSuffix) -MM lib/Common/StringInputStream.cpp

$(IntermediateDirectory)/lib_Common_StringInputStream.cpp$(PreprocessSuffix): lib/Common/StringInputStream.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Common_StringInputStream.cpp$(PreprocessSuffix) lib/Common/StringInputStream.cpp

$(IntermediateDirectory)/lib_Common_StdInputStream.cpp$(ObjectSuffix): lib/Common/StdInputStream.cpp $(IntermediateDirectory)/lib_Common_StdInputStream.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Common/StdInputStream.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Common_StdInputStream.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Common_StdInputStream.cpp$(DependSuffix): lib/Common/StdInputStream.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Common_StdInputStream.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Common_StdInputStream.cpp$(DependSuffix) -MM lib/Common/StdInputStream.cpp

$(IntermediateDirectory)/lib_Common_StdInputStream.cpp$(PreprocessSuffix): lib/Common/StdInputStream.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Common_StdInputStream.cpp$(PreprocessSuffix) lib/Common/StdInputStream.cpp

$(IntermediateDirectory)/lib_Common_ConsoleTools.cpp$(ObjectSuffix): lib/Common/ConsoleTools.cpp $(IntermediateDirectory)/lib_Common_ConsoleTools.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Common/ConsoleTools.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Common_ConsoleTools.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Common_ConsoleTools.cpp$(DependSuffix): lib/Common/ConsoleTools.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Common_ConsoleTools.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Common_ConsoleTools.cpp$(DependSuffix) -MM lib/Common/ConsoleTools.cpp

$(IntermediateDirectory)/lib_Common_ConsoleTools.cpp$(PreprocessSuffix): lib/Common/ConsoleTools.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Common_ConsoleTools.cpp$(PreprocessSuffix) lib/Common/ConsoleTools.cpp

$(IntermediateDirectory)/lib_Common_Base58.cpp$(ObjectSuffix): lib/Common/Base58.cpp $(IntermediateDirectory)/lib_Common_Base58.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Common/Base58.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Common_Base58.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Common_Base58.cpp$(DependSuffix): lib/Common/Base58.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Common_Base58.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Common_Base58.cpp$(DependSuffix) -MM lib/Common/Base58.cpp

$(IntermediateDirectory)/lib_Common_Base58.cpp$(PreprocessSuffix): lib/Common/Base58.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Common_Base58.cpp$(PreprocessSuffix) lib/Common/Base58.cpp

$(IntermediateDirectory)/lib_Common_Math.cpp$(ObjectSuffix): lib/Common/Math.cpp $(IntermediateDirectory)/lib_Common_Math.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Common/Math.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Common_Math.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Common_Math.cpp$(DependSuffix): lib/Common/Math.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Common_Math.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Common_Math.cpp$(DependSuffix) -MM lib/Common/Math.cpp

$(IntermediateDirectory)/lib_Common_Math.cpp$(PreprocessSuffix): lib/Common/Math.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Common_Math.cpp$(PreprocessSuffix) lib/Common/Math.cpp

$(IntermediateDirectory)/lib_Common_StringView.cpp$(ObjectSuffix): lib/Common/StringView.cpp $(IntermediateDirectory)/lib_Common_StringView.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Common/StringView.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Common_StringView.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Common_StringView.cpp$(DependSuffix): lib/Common/StringView.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Common_StringView.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Common_StringView.cpp$(DependSuffix) -MM lib/Common/StringView.cpp

$(IntermediateDirectory)/lib_Common_StringView.cpp$(PreprocessSuffix): lib/Common/StringView.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Common_StringView.cpp$(PreprocessSuffix) lib/Common/StringView.cpp

$(IntermediateDirectory)/lib_Common_StringOutputStream.cpp$(ObjectSuffix): lib/Common/StringOutputStream.cpp $(IntermediateDirectory)/lib_Common_StringOutputStream.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Common/StringOutputStream.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Common_StringOutputStream.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Common_StringOutputStream.cpp$(DependSuffix): lib/Common/StringOutputStream.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Common_StringOutputStream.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Common_StringOutputStream.cpp$(DependSuffix) -MM lib/Common/StringOutputStream.cpp

$(IntermediateDirectory)/lib_Common_StringOutputStream.cpp$(PreprocessSuffix): lib/Common/StringOutputStream.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Common_StringOutputStream.cpp$(PreprocessSuffix) lib/Common/StringOutputStream.cpp

$(IntermediateDirectory)/lib_Common_VectorOutputStream.cpp$(ObjectSuffix): lib/Common/VectorOutputStream.cpp $(IntermediateDirectory)/lib_Common_VectorOutputStream.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Common/VectorOutputStream.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Common_VectorOutputStream.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Common_VectorOutputStream.cpp$(DependSuffix): lib/Common/VectorOutputStream.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Common_VectorOutputStream.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Common_VectorOutputStream.cpp$(DependSuffix) -MM lib/Common/VectorOutputStream.cpp

$(IntermediateDirectory)/lib_Common_VectorOutputStream.cpp$(PreprocessSuffix): lib/Common/VectorOutputStream.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Common_VectorOutputStream.cpp$(PreprocessSuffix) lib/Common/VectorOutputStream.cpp

$(IntermediateDirectory)/lib_Common_FileMappedVector.cpp$(ObjectSuffix): lib/Common/FileMappedVector.cpp $(IntermediateDirectory)/lib_Common_FileMappedVector.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Common/FileMappedVector.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Common_FileMappedVector.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Common_FileMappedVector.cpp$(DependSuffix): lib/Common/FileMappedVector.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Common_FileMappedVector.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Common_FileMappedVector.cpp$(DependSuffix) -MM lib/Common/FileMappedVector.cpp

$(IntermediateDirectory)/lib_Common_FileMappedVector.cpp$(PreprocessSuffix): lib/Common/FileMappedVector.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Common_FileMappedVector.cpp$(PreprocessSuffix) lib/Common/FileMappedVector.cpp

$(IntermediateDirectory)/lib_Common_StdOutputStream.cpp$(ObjectSuffix): lib/Common/StdOutputStream.cpp $(IntermediateDirectory)/lib_Common_StdOutputStream.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Common/StdOutputStream.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Common_StdOutputStream.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Common_StdOutputStream.cpp$(DependSuffix): lib/Common/StdOutputStream.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Common_StdOutputStream.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Common_StdOutputStream.cpp$(DependSuffix) -MM lib/Common/StdOutputStream.cpp

$(IntermediateDirectory)/lib_Common_StdOutputStream.cpp$(PreprocessSuffix): lib/Common/StdOutputStream.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Common_StdOutputStream.cpp$(PreprocessSuffix) lib/Common/StdOutputStream.cpp

$(IntermediateDirectory)/lib_Common_DnsTools.cpp$(ObjectSuffix): lib/Common/DnsTools.cpp $(IntermediateDirectory)/lib_Common_DnsTools.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Common/DnsTools.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Common_DnsTools.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Common_DnsTools.cpp$(DependSuffix): lib/Common/DnsTools.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Common_DnsTools.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Common_DnsTools.cpp$(DependSuffix) -MM lib/Common/DnsTools.cpp

$(IntermediateDirectory)/lib_Common_DnsTools.cpp$(PreprocessSuffix): lib/Common/DnsTools.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Common_DnsTools.cpp$(PreprocessSuffix) lib/Common/DnsTools.cpp

$(IntermediateDirectory)/lib_Common_StringUtils.cpp$(ObjectSuffix): lib/Common/StringUtils.cpp $(IntermediateDirectory)/lib_Common_StringUtils.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Common/StringUtils.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Common_StringUtils.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Common_StringUtils.cpp$(DependSuffix): lib/Common/StringUtils.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Common_StringUtils.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Common_StringUtils.cpp$(DependSuffix) -MM lib/Common/StringUtils.cpp

$(IntermediateDirectory)/lib_Common_StringUtils.cpp$(PreprocessSuffix): lib/Common/StringUtils.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Common_StringUtils.cpp$(PreprocessSuffix) lib/Common/StringUtils.cpp

$(IntermediateDirectory)/lib_Common_BlockingQueue.cpp$(ObjectSuffix): lib/Common/BlockingQueue.cpp $(IntermediateDirectory)/lib_Common_BlockingQueue.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Common/BlockingQueue.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Common_BlockingQueue.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Common_BlockingQueue.cpp$(DependSuffix): lib/Common/BlockingQueue.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Common_BlockingQueue.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Common_BlockingQueue.cpp$(DependSuffix) -MM lib/Common/BlockingQueue.cpp

$(IntermediateDirectory)/lib_Common_BlockingQueue.cpp$(PreprocessSuffix): lib/Common/BlockingQueue.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Common_BlockingQueue.cpp$(PreprocessSuffix) lib/Common/BlockingQueue.cpp

$(IntermediateDirectory)/lib_Common_StreamTools.cpp$(ObjectSuffix): lib/Common/StreamTools.cpp $(IntermediateDirectory)/lib_Common_StreamTools.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Common/StreamTools.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Common_StreamTools.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Common_StreamTools.cpp$(DependSuffix): lib/Common/StreamTools.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Common_StreamTools.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Common_StreamTools.cpp$(DependSuffix) -MM lib/Common/StreamTools.cpp

$(IntermediateDirectory)/lib_Common_StreamTools.cpp$(PreprocessSuffix): lib/Common/StreamTools.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Common_StreamTools.cpp$(PreprocessSuffix) lib/Common/StreamTools.cpp

$(IntermediateDirectory)/lib_Common_Util.cpp$(ObjectSuffix): lib/Common/Util.cpp $(IntermediateDirectory)/lib_Common_Util.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Common/Util.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Common_Util.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Common_Util.cpp$(DependSuffix): lib/Common/Util.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Common_Util.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Common_Util.cpp$(DependSuffix) -MM lib/Common/Util.cpp

$(IntermediateDirectory)/lib_Common_Util.cpp$(PreprocessSuffix): lib/Common/Util.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Common_Util.cpp$(PreprocessSuffix) lib/Common/Util.cpp

$(IntermediateDirectory)/lib_Common_SignalHandler.cpp$(ObjectSuffix): lib/Common/SignalHandler.cpp $(IntermediateDirectory)/lib_Common_SignalHandler.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Common/SignalHandler.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Common_SignalHandler.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Common_SignalHandler.cpp$(DependSuffix): lib/Common/SignalHandler.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Common_SignalHandler.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Common_SignalHandler.cpp$(DependSuffix) -MM lib/Common/SignalHandler.cpp

$(IntermediateDirectory)/lib_Common_SignalHandler.cpp$(PreprocessSuffix): lib/Common/SignalHandler.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Common_SignalHandler.cpp$(PreprocessSuffix) lib/Common/SignalHandler.cpp

$(IntermediateDirectory)/lib_Common_MemoryInputStream.cpp$(ObjectSuffix): lib/Common/MemoryInputStream.cpp $(IntermediateDirectory)/lib_Common_MemoryInputStream.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Common/MemoryInputStream.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Common_MemoryInputStream.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Common_MemoryInputStream.cpp$(DependSuffix): lib/Common/MemoryInputStream.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Common_MemoryInputStream.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Common_MemoryInputStream.cpp$(DependSuffix) -MM lib/Common/MemoryInputStream.cpp

$(IntermediateDirectory)/lib_Common_MemoryInputStream.cpp$(PreprocessSuffix): lib/Common/MemoryInputStream.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Common_MemoryInputStream.cpp$(PreprocessSuffix) lib/Common/MemoryInputStream.cpp

$(IntermediateDirectory)/lib_Common_PathTools.cpp$(ObjectSuffix): lib/Common/PathTools.cpp $(IntermediateDirectory)/lib_Common_PathTools.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Common/PathTools.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Common_PathTools.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Common_PathTools.cpp$(DependSuffix): lib/Common/PathTools.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Common_PathTools.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Common_PathTools.cpp$(DependSuffix) -MM lib/Common/PathTools.cpp

$(IntermediateDirectory)/lib_Common_PathTools.cpp$(PreprocessSuffix): lib/Common/PathTools.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Common_PathTools.cpp$(PreprocessSuffix) lib/Common/PathTools.cpp

$(IntermediateDirectory)/lib_Common_IOutputStream.cpp$(ObjectSuffix): lib/Common/IOutputStream.cpp $(IntermediateDirectory)/lib_Common_IOutputStream.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Common/IOutputStream.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Common_IOutputStream.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Common_IOutputStream.cpp$(DependSuffix): lib/Common/IOutputStream.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Common_IOutputStream.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Common_IOutputStream.cpp$(DependSuffix) -MM lib/Common/IOutputStream.cpp

$(IntermediateDirectory)/lib_Common_IOutputStream.cpp$(PreprocessSuffix): lib/Common/IOutputStream.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Common_IOutputStream.cpp$(PreprocessSuffix) lib/Common/IOutputStream.cpp

$(IntermediateDirectory)/lib_Logging_FileLogger.cpp$(ObjectSuffix): lib/Logging/FileLogger.cpp $(IntermediateDirectory)/lib_Logging_FileLogger.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Logging/FileLogger.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Logging_FileLogger.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Logging_FileLogger.cpp$(DependSuffix): lib/Logging/FileLogger.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Logging_FileLogger.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Logging_FileLogger.cpp$(DependSuffix) -MM lib/Logging/FileLogger.cpp

$(IntermediateDirectory)/lib_Logging_FileLogger.cpp$(PreprocessSuffix): lib/Logging/FileLogger.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Logging_FileLogger.cpp$(PreprocessSuffix) lib/Logging/FileLogger.cpp

$(IntermediateDirectory)/lib_Logging_LoggerGroup.cpp$(ObjectSuffix): lib/Logging/LoggerGroup.cpp $(IntermediateDirectory)/lib_Logging_LoggerGroup.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Logging/LoggerGroup.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Logging_LoggerGroup.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Logging_LoggerGroup.cpp$(DependSuffix): lib/Logging/LoggerGroup.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Logging_LoggerGroup.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Logging_LoggerGroup.cpp$(DependSuffix) -MM lib/Logging/LoggerGroup.cpp

$(IntermediateDirectory)/lib_Logging_LoggerGroup.cpp$(PreprocessSuffix): lib/Logging/LoggerGroup.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Logging_LoggerGroup.cpp$(PreprocessSuffix) lib/Logging/LoggerGroup.cpp

$(IntermediateDirectory)/lib_Logging_LoggerManager.cpp$(ObjectSuffix): lib/Logging/LoggerManager.cpp $(IntermediateDirectory)/lib_Logging_LoggerManager.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Logging/LoggerManager.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Logging_LoggerManager.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Logging_LoggerManager.cpp$(DependSuffix): lib/Logging/LoggerManager.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Logging_LoggerManager.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Logging_LoggerManager.cpp$(DependSuffix) -MM lib/Logging/LoggerManager.cpp

$(IntermediateDirectory)/lib_Logging_LoggerManager.cpp$(PreprocessSuffix): lib/Logging/LoggerManager.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Logging_LoggerManager.cpp$(PreprocessSuffix) lib/Logging/LoggerManager.cpp

$(IntermediateDirectory)/lib_Logging_ILogger.cpp$(ObjectSuffix): lib/Logging/ILogger.cpp $(IntermediateDirectory)/lib_Logging_ILogger.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Logging/ILogger.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Logging_ILogger.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Logging_ILogger.cpp$(DependSuffix): lib/Logging/ILogger.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Logging_ILogger.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Logging_ILogger.cpp$(DependSuffix) -MM lib/Logging/ILogger.cpp

$(IntermediateDirectory)/lib_Logging_ILogger.cpp$(PreprocessSuffix): lib/Logging/ILogger.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Logging_ILogger.cpp$(PreprocessSuffix) lib/Logging/ILogger.cpp

$(IntermediateDirectory)/lib_Logging_StreamLogger.cpp$(ObjectSuffix): lib/Logging/StreamLogger.cpp $(IntermediateDirectory)/lib_Logging_StreamLogger.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Logging/StreamLogger.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Logging_StreamLogger.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Logging_StreamLogger.cpp$(DependSuffix): lib/Logging/StreamLogger.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Logging_StreamLogger.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Logging_StreamLogger.cpp$(DependSuffix) -MM lib/Logging/StreamLogger.cpp

$(IntermediateDirectory)/lib_Logging_StreamLogger.cpp$(PreprocessSuffix): lib/Logging/StreamLogger.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Logging_StreamLogger.cpp$(PreprocessSuffix) lib/Logging/StreamLogger.cpp

$(IntermediateDirectory)/lib_Logging_LoggerRef.cpp$(ObjectSuffix): lib/Logging/LoggerRef.cpp $(IntermediateDirectory)/lib_Logging_LoggerRef.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Logging/LoggerRef.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Logging_LoggerRef.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Logging_LoggerRef.cpp$(DependSuffix): lib/Logging/LoggerRef.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Logging_LoggerRef.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Logging_LoggerRef.cpp$(DependSuffix) -MM lib/Logging/LoggerRef.cpp

$(IntermediateDirectory)/lib_Logging_LoggerRef.cpp$(PreprocessSuffix): lib/Logging/LoggerRef.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Logging_LoggerRef.cpp$(PreprocessSuffix) lib/Logging/LoggerRef.cpp

$(IntermediateDirectory)/lib_Logging_LoggerMessage.cpp$(ObjectSuffix): lib/Logging/LoggerMessage.cpp $(IntermediateDirectory)/lib_Logging_LoggerMessage.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Logging/LoggerMessage.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Logging_LoggerMessage.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Logging_LoggerMessage.cpp$(DependSuffix): lib/Logging/LoggerMessage.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Logging_LoggerMessage.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Logging_LoggerMessage.cpp$(DependSuffix) -MM lib/Logging/LoggerMessage.cpp

$(IntermediateDirectory)/lib_Logging_LoggerMessage.cpp$(PreprocessSuffix): lib/Logging/LoggerMessage.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Logging_LoggerMessage.cpp$(PreprocessSuffix) lib/Logging/LoggerMessage.cpp

$(IntermediateDirectory)/lib_Logging_CommonLogger.cpp$(ObjectSuffix): lib/Logging/CommonLogger.cpp $(IntermediateDirectory)/lib_Logging_CommonLogger.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Logging/CommonLogger.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Logging_CommonLogger.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Logging_CommonLogger.cpp$(DependSuffix): lib/Logging/CommonLogger.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Logging_CommonLogger.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Logging_CommonLogger.cpp$(DependSuffix) -MM lib/Logging/CommonLogger.cpp

$(IntermediateDirectory)/lib_Logging_CommonLogger.cpp$(PreprocessSuffix): lib/Logging/CommonLogger.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Logging_CommonLogger.cpp$(PreprocessSuffix) lib/Logging/CommonLogger.cpp

$(IntermediateDirectory)/lib_Logging_ConsoleLogger.cpp$(ObjectSuffix): lib/Logging/ConsoleLogger.cpp $(IntermediateDirectory)/lib_Logging_ConsoleLogger.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Logging/ConsoleLogger.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Logging_ConsoleLogger.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Logging_ConsoleLogger.cpp$(DependSuffix): lib/Logging/ConsoleLogger.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Logging_ConsoleLogger.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Logging_ConsoleLogger.cpp$(DependSuffix) -MM lib/Logging/ConsoleLogger.cpp

$(IntermediateDirectory)/lib_Logging_ConsoleLogger.cpp$(PreprocessSuffix): lib/Logging/ConsoleLogger.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Logging_ConsoleLogger.cpp$(PreprocessSuffix) lib/Logging/ConsoleLogger.cpp

$(IntermediateDirectory)/lib_WalletLegacy_KeysStorage.cpp$(ObjectSuffix): lib/WalletLegacy/KeysStorage.cpp $(IntermediateDirectory)/lib_WalletLegacy_KeysStorage.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/WalletLegacy/KeysStorage.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_WalletLegacy_KeysStorage.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_WalletLegacy_KeysStorage.cpp$(DependSuffix): lib/WalletLegacy/KeysStorage.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_WalletLegacy_KeysStorage.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_WalletLegacy_KeysStorage.cpp$(DependSuffix) -MM lib/WalletLegacy/KeysStorage.cpp

$(IntermediateDirectory)/lib_WalletLegacy_KeysStorage.cpp$(PreprocessSuffix): lib/WalletLegacy/KeysStorage.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_WalletLegacy_KeysStorage.cpp$(PreprocessSuffix) lib/WalletLegacy/KeysStorage.cpp

$(IntermediateDirectory)/lib_WalletLegacy_WalletTransactionSender.cpp$(ObjectSuffix): lib/WalletLegacy/WalletTransactionSender.cpp $(IntermediateDirectory)/lib_WalletLegacy_WalletTransactionSender.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/WalletLegacy/WalletTransactionSender.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_WalletLegacy_WalletTransactionSender.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_WalletLegacy_WalletTransactionSender.cpp$(DependSuffix): lib/WalletLegacy/WalletTransactionSender.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_WalletLegacy_WalletTransactionSender.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_WalletLegacy_WalletTransactionSender.cpp$(DependSuffix) -MM lib/WalletLegacy/WalletTransactionSender.cpp

$(IntermediateDirectory)/lib_WalletLegacy_WalletTransactionSender.cpp$(PreprocessSuffix): lib/WalletLegacy/WalletTransactionSender.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_WalletLegacy_WalletTransactionSender.cpp$(PreprocessSuffix) lib/WalletLegacy/WalletTransactionSender.cpp

$(IntermediateDirectory)/lib_WalletLegacy_WalletUserTransactionsCache.cpp$(ObjectSuffix): lib/WalletLegacy/WalletUserTransactionsCache.cpp $(IntermediateDirectory)/lib_WalletLegacy_WalletUserTransactionsCache.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/WalletLegacy/WalletUserTransactionsCache.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_WalletLegacy_WalletUserTransactionsCache.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_WalletLegacy_WalletUserTransactionsCache.cpp$(DependSuffix): lib/WalletLegacy/WalletUserTransactionsCache.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_WalletLegacy_WalletUserTransactionsCache.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_WalletLegacy_WalletUserTransactionsCache.cpp$(DependSuffix) -MM lib/WalletLegacy/WalletUserTransactionsCache.cpp

$(IntermediateDirectory)/lib_WalletLegacy_WalletUserTransactionsCache.cpp$(PreprocessSuffix): lib/WalletLegacy/WalletUserTransactionsCache.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_WalletLegacy_WalletUserTransactionsCache.cpp$(PreprocessSuffix) lib/WalletLegacy/WalletUserTransactionsCache.cpp

$(IntermediateDirectory)/lib_WalletLegacy_WalletHelper.cpp$(ObjectSuffix): lib/WalletLegacy/WalletHelper.cpp $(IntermediateDirectory)/lib_WalletLegacy_WalletHelper.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/WalletLegacy/WalletHelper.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_WalletLegacy_WalletHelper.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_WalletLegacy_WalletHelper.cpp$(DependSuffix): lib/WalletLegacy/WalletHelper.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_WalletLegacy_WalletHelper.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_WalletLegacy_WalletHelper.cpp$(DependSuffix) -MM lib/WalletLegacy/WalletHelper.cpp

$(IntermediateDirectory)/lib_WalletLegacy_WalletHelper.cpp$(PreprocessSuffix): lib/WalletLegacy/WalletHelper.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_WalletLegacy_WalletHelper.cpp$(PreprocessSuffix) lib/WalletLegacy/WalletHelper.cpp

$(IntermediateDirectory)/lib_WalletLegacy_WalletUnconfirmedTransactions.cpp$(ObjectSuffix): lib/WalletLegacy/WalletUnconfirmedTransactions.cpp $(IntermediateDirectory)/lib_WalletLegacy_WalletUnconfirmedTransactions.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/WalletLegacy/WalletUnconfirmedTransactions.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_WalletLegacy_WalletUnconfirmedTransactions.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_WalletLegacy_WalletUnconfirmedTransactions.cpp$(DependSuffix): lib/WalletLegacy/WalletUnconfirmedTransactions.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_WalletLegacy_WalletUnconfirmedTransactions.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_WalletLegacy_WalletUnconfirmedTransactions.cpp$(DependSuffix) -MM lib/WalletLegacy/WalletUnconfirmedTransactions.cpp

$(IntermediateDirectory)/lib_WalletLegacy_WalletUnconfirmedTransactions.cpp$(PreprocessSuffix): lib/WalletLegacy/WalletUnconfirmedTransactions.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_WalletLegacy_WalletUnconfirmedTransactions.cpp$(PreprocessSuffix) lib/WalletLegacy/WalletUnconfirmedTransactions.cpp

$(IntermediateDirectory)/lib_WalletLegacy_WalletLegacySerialization.cpp$(ObjectSuffix): lib/WalletLegacy/WalletLegacySerialization.cpp $(IntermediateDirectory)/lib_WalletLegacy_WalletLegacySerialization.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/WalletLegacy/WalletLegacySerialization.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_WalletLegacy_WalletLegacySerialization.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_WalletLegacy_WalletLegacySerialization.cpp$(DependSuffix): lib/WalletLegacy/WalletLegacySerialization.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_WalletLegacy_WalletLegacySerialization.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_WalletLegacy_WalletLegacySerialization.cpp$(DependSuffix) -MM lib/WalletLegacy/WalletLegacySerialization.cpp

$(IntermediateDirectory)/lib_WalletLegacy_WalletLegacySerialization.cpp$(PreprocessSuffix): lib/WalletLegacy/WalletLegacySerialization.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_WalletLegacy_WalletLegacySerialization.cpp$(PreprocessSuffix) lib/WalletLegacy/WalletLegacySerialization.cpp

$(IntermediateDirectory)/lib_WalletLegacy_WalletLegacySerializer.cpp$(ObjectSuffix): lib/WalletLegacy/WalletLegacySerializer.cpp $(IntermediateDirectory)/lib_WalletLegacy_WalletLegacySerializer.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/WalletLegacy/WalletLegacySerializer.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_WalletLegacy_WalletLegacySerializer.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_WalletLegacy_WalletLegacySerializer.cpp$(DependSuffix): lib/WalletLegacy/WalletLegacySerializer.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_WalletLegacy_WalletLegacySerializer.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_WalletLegacy_WalletLegacySerializer.cpp$(DependSuffix) -MM lib/WalletLegacy/WalletLegacySerializer.cpp

$(IntermediateDirectory)/lib_WalletLegacy_WalletLegacySerializer.cpp$(PreprocessSuffix): lib/WalletLegacy/WalletLegacySerializer.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_WalletLegacy_WalletLegacySerializer.cpp$(PreprocessSuffix) lib/WalletLegacy/WalletLegacySerializer.cpp

$(IntermediateDirectory)/lib_WalletLegacy_WalletLegacy.cpp$(ObjectSuffix): lib/WalletLegacy/WalletLegacy.cpp $(IntermediateDirectory)/lib_WalletLegacy_WalletLegacy.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/WalletLegacy/WalletLegacy.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_WalletLegacy_WalletLegacy.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_WalletLegacy_WalletLegacy.cpp$(DependSuffix): lib/WalletLegacy/WalletLegacy.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_WalletLegacy_WalletLegacy.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_WalletLegacy_WalletLegacy.cpp$(DependSuffix) -MM lib/WalletLegacy/WalletLegacy.cpp

$(IntermediateDirectory)/lib_WalletLegacy_WalletLegacy.cpp$(PreprocessSuffix): lib/WalletLegacy/WalletLegacy.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_WalletLegacy_WalletLegacy.cpp$(PreprocessSuffix) lib/WalletLegacy/WalletLegacy.cpp

$(IntermediateDirectory)/lib_InProcessNode_InProcessNodeErrors.cpp$(ObjectSuffix): lib/InProcessNode/InProcessNodeErrors.cpp $(IntermediateDirectory)/lib_InProcessNode_InProcessNodeErrors.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/InProcessNode/InProcessNodeErrors.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_InProcessNode_InProcessNodeErrors.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_InProcessNode_InProcessNodeErrors.cpp$(DependSuffix): lib/InProcessNode/InProcessNodeErrors.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_InProcessNode_InProcessNodeErrors.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_InProcessNode_InProcessNodeErrors.cpp$(DependSuffix) -MM lib/InProcessNode/InProcessNodeErrors.cpp

$(IntermediateDirectory)/lib_InProcessNode_InProcessNodeErrors.cpp$(PreprocessSuffix): lib/InProcessNode/InProcessNodeErrors.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_InProcessNode_InProcessNodeErrors.cpp$(PreprocessSuffix) lib/InProcessNode/InProcessNodeErrors.cpp

$(IntermediateDirectory)/lib_InProcessNode_InProcessNode.cpp$(ObjectSuffix): lib/InProcessNode/InProcessNode.cpp $(IntermediateDirectory)/lib_InProcessNode_InProcessNode.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/InProcessNode/InProcessNode.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_InProcessNode_InProcessNode.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_InProcessNode_InProcessNode.cpp$(DependSuffix): lib/InProcessNode/InProcessNode.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_InProcessNode_InProcessNode.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_InProcessNode_InProcessNode.cpp$(DependSuffix) -MM lib/InProcessNode/InProcessNode.cpp

$(IntermediateDirectory)/lib_InProcessNode_InProcessNode.cpp$(PreprocessSuffix): lib/InProcessNode/InProcessNode.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_InProcessNode_InProcessNode.cpp$(PreprocessSuffix) lib/InProcessNode/InProcessNode.cpp

$(IntermediateDirectory)/lib_Transfers_TransfersConsumer.cpp$(ObjectSuffix): lib/Transfers/TransfersConsumer.cpp $(IntermediateDirectory)/lib_Transfers_TransfersConsumer.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Transfers/TransfersConsumer.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Transfers_TransfersConsumer.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Transfers_TransfersConsumer.cpp$(DependSuffix): lib/Transfers/TransfersConsumer.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Transfers_TransfersConsumer.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Transfers_TransfersConsumer.cpp$(DependSuffix) -MM lib/Transfers/TransfersConsumer.cpp

$(IntermediateDirectory)/lib_Transfers_TransfersConsumer.cpp$(PreprocessSuffix): lib/Transfers/TransfersConsumer.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Transfers_TransfersConsumer.cpp$(PreprocessSuffix) lib/Transfers/TransfersConsumer.cpp

$(IntermediateDirectory)/lib_Transfers_BlockchainSynchronizer.cpp$(ObjectSuffix): lib/Transfers/BlockchainSynchronizer.cpp $(IntermediateDirectory)/lib_Transfers_BlockchainSynchronizer.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Transfers/BlockchainSynchronizer.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Transfers_BlockchainSynchronizer.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Transfers_BlockchainSynchronizer.cpp$(DependSuffix): lib/Transfers/BlockchainSynchronizer.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Transfers_BlockchainSynchronizer.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Transfers_BlockchainSynchronizer.cpp$(DependSuffix) -MM lib/Transfers/BlockchainSynchronizer.cpp

$(IntermediateDirectory)/lib_Transfers_BlockchainSynchronizer.cpp$(PreprocessSuffix): lib/Transfers/BlockchainSynchronizer.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Transfers_BlockchainSynchronizer.cpp$(PreprocessSuffix) lib/Transfers/BlockchainSynchronizer.cpp

$(IntermediateDirectory)/lib_Transfers_TransfersSynchronizer.cpp$(ObjectSuffix): lib/Transfers/TransfersSynchronizer.cpp $(IntermediateDirectory)/lib_Transfers_TransfersSynchronizer.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Transfers/TransfersSynchronizer.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Transfers_TransfersSynchronizer.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Transfers_TransfersSynchronizer.cpp$(DependSuffix): lib/Transfers/TransfersSynchronizer.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Transfers_TransfersSynchronizer.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Transfers_TransfersSynchronizer.cpp$(DependSuffix) -MM lib/Transfers/TransfersSynchronizer.cpp

$(IntermediateDirectory)/lib_Transfers_TransfersSynchronizer.cpp$(PreprocessSuffix): lib/Transfers/TransfersSynchronizer.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Transfers_TransfersSynchronizer.cpp$(PreprocessSuffix) lib/Transfers/TransfersSynchronizer.cpp

$(IntermediateDirectory)/lib_Transfers_TransfersContainer.cpp$(ObjectSuffix): lib/Transfers/TransfersContainer.cpp $(IntermediateDirectory)/lib_Transfers_TransfersContainer.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Transfers/TransfersContainer.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Transfers_TransfersContainer.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Transfers_TransfersContainer.cpp$(DependSuffix): lib/Transfers/TransfersContainer.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Transfers_TransfersContainer.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Transfers_TransfersContainer.cpp$(DependSuffix) -MM lib/Transfers/TransfersContainer.cpp

$(IntermediateDirectory)/lib_Transfers_TransfersContainer.cpp$(PreprocessSuffix): lib/Transfers/TransfersContainer.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Transfers_TransfersContainer.cpp$(PreprocessSuffix) lib/Transfers/TransfersContainer.cpp

$(IntermediateDirectory)/lib_Transfers_SynchronizationState.cpp$(ObjectSuffix): lib/Transfers/SynchronizationState.cpp $(IntermediateDirectory)/lib_Transfers_SynchronizationState.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Transfers/SynchronizationState.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Transfers_SynchronizationState.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Transfers_SynchronizationState.cpp$(DependSuffix): lib/Transfers/SynchronizationState.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Transfers_SynchronizationState.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Transfers_SynchronizationState.cpp$(DependSuffix) -MM lib/Transfers/SynchronizationState.cpp

$(IntermediateDirectory)/lib_Transfers_SynchronizationState.cpp$(PreprocessSuffix): lib/Transfers/SynchronizationState.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Transfers_SynchronizationState.cpp$(PreprocessSuffix) lib/Transfers/SynchronizationState.cpp

$(IntermediateDirectory)/lib_Transfers_TransfersSubscription.cpp$(ObjectSuffix): lib/Transfers/TransfersSubscription.cpp $(IntermediateDirectory)/lib_Transfers_TransfersSubscription.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Transfers/TransfersSubscription.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Transfers_TransfersSubscription.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Transfers_TransfersSubscription.cpp$(DependSuffix): lib/Transfers/TransfersSubscription.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Transfers_TransfersSubscription.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Transfers_TransfersSubscription.cpp$(DependSuffix) -MM lib/Transfers/TransfersSubscription.cpp

$(IntermediateDirectory)/lib_Transfers_TransfersSubscription.cpp$(PreprocessSuffix): lib/Transfers/TransfersSubscription.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Transfers_TransfersSubscription.cpp$(PreprocessSuffix) lib/Transfers/TransfersSubscription.cpp

$(IntermediateDirectory)/lib_JsonRpcServer_JsonRpcServer.cpp$(ObjectSuffix): lib/JsonRpcServer/JsonRpcServer.cpp $(IntermediateDirectory)/lib_JsonRpcServer_JsonRpcServer.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/JsonRpcServer/JsonRpcServer.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_JsonRpcServer_JsonRpcServer.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_JsonRpcServer_JsonRpcServer.cpp$(DependSuffix): lib/JsonRpcServer/JsonRpcServer.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_JsonRpcServer_JsonRpcServer.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_JsonRpcServer_JsonRpcServer.cpp$(DependSuffix) -MM lib/JsonRpcServer/JsonRpcServer.cpp

$(IntermediateDirectory)/lib_JsonRpcServer_JsonRpcServer.cpp$(PreprocessSuffix): lib/JsonRpcServer/JsonRpcServer.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_JsonRpcServer_JsonRpcServer.cpp$(PreprocessSuffix) lib/JsonRpcServer/JsonRpcServer.cpp

$(IntermediateDirectory)/lib_CryptoNoteProtocol_CryptoNoteProtocolHandler.cpp$(ObjectSuffix): lib/CryptoNoteProtocol/CryptoNoteProtocolHandler.cpp $(IntermediateDirectory)/lib_CryptoNoteProtocol_CryptoNoteProtocolHandler.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/CryptoNoteProtocol/CryptoNoteProtocolHandler.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_CryptoNoteProtocol_CryptoNoteProtocolHandler.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_CryptoNoteProtocol_CryptoNoteProtocolHandler.cpp$(DependSuffix): lib/CryptoNoteProtocol/CryptoNoteProtocolHandler.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_CryptoNoteProtocol_CryptoNoteProtocolHandler.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_CryptoNoteProtocol_CryptoNoteProtocolHandler.cpp$(DependSuffix) -MM lib/CryptoNoteProtocol/CryptoNoteProtocolHandler.cpp

$(IntermediateDirectory)/lib_CryptoNoteProtocol_CryptoNoteProtocolHandler.cpp$(PreprocessSuffix): lib/CryptoNoteProtocol/CryptoNoteProtocolHandler.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_CryptoNoteProtocol_CryptoNoteProtocolHandler.cpp$(PreprocessSuffix) lib/CryptoNoteProtocol/CryptoNoteProtocolHandler.cpp

$(IntermediateDirectory)/lib_PaymentGate_NodeFactory.cpp$(ObjectSuffix): lib/PaymentGate/NodeFactory.cpp $(IntermediateDirectory)/lib_PaymentGate_NodeFactory.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/PaymentGate/NodeFactory.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_PaymentGate_NodeFactory.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_PaymentGate_NodeFactory.cpp$(DependSuffix): lib/PaymentGate/NodeFactory.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_PaymentGate_NodeFactory.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_PaymentGate_NodeFactory.cpp$(DependSuffix) -MM lib/PaymentGate/NodeFactory.cpp

$(IntermediateDirectory)/lib_PaymentGate_NodeFactory.cpp$(PreprocessSuffix): lib/PaymentGate/NodeFactory.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_PaymentGate_NodeFactory.cpp$(PreprocessSuffix) lib/PaymentGate/NodeFactory.cpp

$(IntermediateDirectory)/lib_PaymentGate_WalletServiceErrorCategory.cpp$(ObjectSuffix): lib/PaymentGate/WalletServiceErrorCategory.cpp $(IntermediateDirectory)/lib_PaymentGate_WalletServiceErrorCategory.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/PaymentGate/WalletServiceErrorCategory.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_PaymentGate_WalletServiceErrorCategory.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_PaymentGate_WalletServiceErrorCategory.cpp$(DependSuffix): lib/PaymentGate/WalletServiceErrorCategory.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_PaymentGate_WalletServiceErrorCategory.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_PaymentGate_WalletServiceErrorCategory.cpp$(DependSuffix) -MM lib/PaymentGate/WalletServiceErrorCategory.cpp

$(IntermediateDirectory)/lib_PaymentGate_WalletServiceErrorCategory.cpp$(PreprocessSuffix): lib/PaymentGate/WalletServiceErrorCategory.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_PaymentGate_WalletServiceErrorCategory.cpp$(PreprocessSuffix) lib/PaymentGate/WalletServiceErrorCategory.cpp

$(IntermediateDirectory)/lib_PaymentGate_PaymentServiceJsonRpcMessages.cpp$(ObjectSuffix): lib/PaymentGate/PaymentServiceJsonRpcMessages.cpp $(IntermediateDirectory)/lib_PaymentGate_PaymentServiceJsonRpcMessages.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/PaymentGate/PaymentServiceJsonRpcMessages.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_PaymentGate_PaymentServiceJsonRpcMessages.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_PaymentGate_PaymentServiceJsonRpcMessages.cpp$(DependSuffix): lib/PaymentGate/PaymentServiceJsonRpcMessages.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_PaymentGate_PaymentServiceJsonRpcMessages.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_PaymentGate_PaymentServiceJsonRpcMessages.cpp$(DependSuffix) -MM lib/PaymentGate/PaymentServiceJsonRpcMessages.cpp

$(IntermediateDirectory)/lib_PaymentGate_PaymentServiceJsonRpcMessages.cpp$(PreprocessSuffix): lib/PaymentGate/PaymentServiceJsonRpcMessages.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_PaymentGate_PaymentServiceJsonRpcMessages.cpp$(PreprocessSuffix) lib/PaymentGate/PaymentServiceJsonRpcMessages.cpp

$(IntermediateDirectory)/lib_PaymentGate_PaymentServiceJsonRpcServer.cpp$(ObjectSuffix): lib/PaymentGate/PaymentServiceJsonRpcServer.cpp $(IntermediateDirectory)/lib_PaymentGate_PaymentServiceJsonRpcServer.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/PaymentGate/PaymentServiceJsonRpcServer.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_PaymentGate_PaymentServiceJsonRpcServer.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_PaymentGate_PaymentServiceJsonRpcServer.cpp$(DependSuffix): lib/PaymentGate/PaymentServiceJsonRpcServer.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_PaymentGate_PaymentServiceJsonRpcServer.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_PaymentGate_PaymentServiceJsonRpcServer.cpp$(DependSuffix) -MM lib/PaymentGate/PaymentServiceJsonRpcServer.cpp

$(IntermediateDirectory)/lib_PaymentGate_PaymentServiceJsonRpcServer.cpp$(PreprocessSuffix): lib/PaymentGate/PaymentServiceJsonRpcServer.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_PaymentGate_PaymentServiceJsonRpcServer.cpp$(PreprocessSuffix) lib/PaymentGate/PaymentServiceJsonRpcServer.cpp

$(IntermediateDirectory)/lib_PaymentGate_WalletService.cpp$(ObjectSuffix): lib/PaymentGate/WalletService.cpp $(IntermediateDirectory)/lib_PaymentGate_WalletService.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/PaymentGate/WalletService.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_PaymentGate_WalletService.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_PaymentGate_WalletService.cpp$(DependSuffix): lib/PaymentGate/WalletService.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_PaymentGate_WalletService.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_PaymentGate_WalletService.cpp$(DependSuffix) -MM lib/PaymentGate/WalletService.cpp

$(IntermediateDirectory)/lib_PaymentGate_WalletService.cpp$(PreprocessSuffix): lib/PaymentGate/WalletService.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_PaymentGate_WalletService.cpp$(PreprocessSuffix) lib/PaymentGate/WalletService.cpp

$(IntermediateDirectory)/lib_P2p_P2pContext.cpp$(ObjectSuffix): lib/P2p/P2pContext.cpp $(IntermediateDirectory)/lib_P2p_P2pContext.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/P2p/P2pContext.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_P2p_P2pContext.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_P2p_P2pContext.cpp$(DependSuffix): lib/P2p/P2pContext.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_P2p_P2pContext.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_P2p_P2pContext.cpp$(DependSuffix) -MM lib/P2p/P2pContext.cpp

$(IntermediateDirectory)/lib_P2p_P2pContext.cpp$(PreprocessSuffix): lib/P2p/P2pContext.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_P2p_P2pContext.cpp$(PreprocessSuffix) lib/P2p/P2pContext.cpp

$(IntermediateDirectory)/lib_P2p_PeerListManager.cpp$(ObjectSuffix): lib/P2p/PeerListManager.cpp $(IntermediateDirectory)/lib_P2p_PeerListManager.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/P2p/PeerListManager.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_P2p_PeerListManager.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_P2p_PeerListManager.cpp$(DependSuffix): lib/P2p/PeerListManager.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_P2p_PeerListManager.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_P2p_PeerListManager.cpp$(DependSuffix) -MM lib/P2p/PeerListManager.cpp

$(IntermediateDirectory)/lib_P2p_PeerListManager.cpp$(PreprocessSuffix): lib/P2p/PeerListManager.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_P2p_PeerListManager.cpp$(PreprocessSuffix) lib/P2p/PeerListManager.cpp

$(IntermediateDirectory)/lib_P2p_P2pConnectionProxy.cpp$(ObjectSuffix): lib/P2p/P2pConnectionProxy.cpp $(IntermediateDirectory)/lib_P2p_P2pConnectionProxy.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/P2p/P2pConnectionProxy.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_P2p_P2pConnectionProxy.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_P2p_P2pConnectionProxy.cpp$(DependSuffix): lib/P2p/P2pConnectionProxy.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_P2p_P2pConnectionProxy.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_P2p_P2pConnectionProxy.cpp$(DependSuffix) -MM lib/P2p/P2pConnectionProxy.cpp

$(IntermediateDirectory)/lib_P2p_P2pConnectionProxy.cpp$(PreprocessSuffix): lib/P2p/P2pConnectionProxy.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_P2p_P2pConnectionProxy.cpp$(PreprocessSuffix) lib/P2p/P2pConnectionProxy.cpp

$(IntermediateDirectory)/lib_P2p_IP2pNodeInternal.cpp$(ObjectSuffix): lib/P2p/IP2pNodeInternal.cpp $(IntermediateDirectory)/lib_P2p_IP2pNodeInternal.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/P2p/IP2pNodeInternal.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_P2p_IP2pNodeInternal.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_P2p_IP2pNodeInternal.cpp$(DependSuffix): lib/P2p/IP2pNodeInternal.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_P2p_IP2pNodeInternal.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_P2p_IP2pNodeInternal.cpp$(DependSuffix) -MM lib/P2p/IP2pNodeInternal.cpp

$(IntermediateDirectory)/lib_P2p_IP2pNodeInternal.cpp$(PreprocessSuffix): lib/P2p/IP2pNodeInternal.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_P2p_IP2pNodeInternal.cpp$(PreprocessSuffix) lib/P2p/IP2pNodeInternal.cpp

$(IntermediateDirectory)/lib_P2p_LevinProtocol.cpp$(ObjectSuffix): lib/P2p/LevinProtocol.cpp $(IntermediateDirectory)/lib_P2p_LevinProtocol.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/P2p/LevinProtocol.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_P2p_LevinProtocol.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_P2p_LevinProtocol.cpp$(DependSuffix): lib/P2p/LevinProtocol.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_P2p_LevinProtocol.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_P2p_LevinProtocol.cpp$(DependSuffix) -MM lib/P2p/LevinProtocol.cpp

$(IntermediateDirectory)/lib_P2p_LevinProtocol.cpp$(PreprocessSuffix): lib/P2p/LevinProtocol.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_P2p_LevinProtocol.cpp$(PreprocessSuffix) lib/P2p/LevinProtocol.cpp

$(IntermediateDirectory)/lib_P2p_NetNode.cpp$(ObjectSuffix): lib/P2p/NetNode.cpp $(IntermediateDirectory)/lib_P2p_NetNode.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/P2p/NetNode.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_P2p_NetNode.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_P2p_NetNode.cpp$(DependSuffix): lib/P2p/NetNode.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_P2p_NetNode.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_P2p_NetNode.cpp$(DependSuffix) -MM lib/P2p/NetNode.cpp

$(IntermediateDirectory)/lib_P2p_NetNode.cpp$(PreprocessSuffix): lib/P2p/NetNode.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_P2p_NetNode.cpp$(PreprocessSuffix) lib/P2p/NetNode.cpp

$(IntermediateDirectory)/lib_P2p_NetNodeConfig.cpp$(ObjectSuffix): lib/P2p/NetNodeConfig.cpp $(IntermediateDirectory)/lib_P2p_NetNodeConfig.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/P2p/NetNodeConfig.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_P2p_NetNodeConfig.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_P2p_NetNodeConfig.cpp$(DependSuffix): lib/P2p/NetNodeConfig.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_P2p_NetNodeConfig.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_P2p_NetNodeConfig.cpp$(DependSuffix) -MM lib/P2p/NetNodeConfig.cpp

$(IntermediateDirectory)/lib_P2p_NetNodeConfig.cpp$(PreprocessSuffix): lib/P2p/NetNodeConfig.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_P2p_NetNodeConfig.cpp$(PreprocessSuffix) lib/P2p/NetNodeConfig.cpp

$(IntermediateDirectory)/lib_P2p_P2pInterfaces.cpp$(ObjectSuffix): lib/P2p/P2pInterfaces.cpp $(IntermediateDirectory)/lib_P2p_P2pInterfaces.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/P2p/P2pInterfaces.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_P2p_P2pInterfaces.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_P2p_P2pInterfaces.cpp$(DependSuffix): lib/P2p/P2pInterfaces.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_P2p_P2pInterfaces.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_P2p_P2pInterfaces.cpp$(DependSuffix) -MM lib/P2p/P2pInterfaces.cpp

$(IntermediateDirectory)/lib_P2p_P2pInterfaces.cpp$(PreprocessSuffix): lib/P2p/P2pInterfaces.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_P2p_P2pInterfaces.cpp$(PreprocessSuffix) lib/P2p/P2pInterfaces.cpp

$(IntermediateDirectory)/lib_P2p_P2pNode.cpp$(ObjectSuffix): lib/P2p/P2pNode.cpp $(IntermediateDirectory)/lib_P2p_P2pNode.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/P2p/P2pNode.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_P2p_P2pNode.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_P2p_P2pNode.cpp$(DependSuffix): lib/P2p/P2pNode.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_P2p_P2pNode.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_P2p_P2pNode.cpp$(DependSuffix) -MM lib/P2p/P2pNode.cpp

$(IntermediateDirectory)/lib_P2p_P2pNode.cpp$(PreprocessSuffix): lib/P2p/P2pNode.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_P2p_P2pNode.cpp$(PreprocessSuffix) lib/P2p/P2pNode.cpp

$(IntermediateDirectory)/lib_P2p_P2pNodeConfig.cpp$(ObjectSuffix): lib/P2p/P2pNodeConfig.cpp $(IntermediateDirectory)/lib_P2p_P2pNodeConfig.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/P2p/P2pNodeConfig.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_P2p_P2pNodeConfig.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_P2p_P2pNodeConfig.cpp$(DependSuffix): lib/P2p/P2pNodeConfig.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_P2p_P2pNodeConfig.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_P2p_P2pNodeConfig.cpp$(DependSuffix) -MM lib/P2p/P2pNodeConfig.cpp

$(IntermediateDirectory)/lib_P2p_P2pNodeConfig.cpp$(PreprocessSuffix): lib/P2p/P2pNodeConfig.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_P2p_P2pNodeConfig.cpp$(PreprocessSuffix) lib/P2p/P2pNodeConfig.cpp

$(IntermediateDirectory)/lib_P2p_P2pContextOwner.cpp$(ObjectSuffix): lib/P2p/P2pContextOwner.cpp $(IntermediateDirectory)/lib_P2p_P2pContextOwner.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/P2p/P2pContextOwner.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_P2p_P2pContextOwner.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_P2p_P2pContextOwner.cpp$(DependSuffix): lib/P2p/P2pContextOwner.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_P2p_P2pContextOwner.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_P2p_P2pContextOwner.cpp$(DependSuffix) -MM lib/P2p/P2pContextOwner.cpp

$(IntermediateDirectory)/lib_P2p_P2pContextOwner.cpp$(PreprocessSuffix): lib/P2p/P2pContextOwner.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_P2p_P2pContextOwner.cpp$(PreprocessSuffix) lib/P2p/P2pContextOwner.cpp

$(IntermediateDirectory)/lib_Serialization_SerializationOverloads.cpp$(ObjectSuffix): lib/Serialization/SerializationOverloads.cpp $(IntermediateDirectory)/lib_Serialization_SerializationOverloads.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Serialization/SerializationOverloads.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Serialization_SerializationOverloads.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Serialization_SerializationOverloads.cpp$(DependSuffix): lib/Serialization/SerializationOverloads.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Serialization_SerializationOverloads.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Serialization_SerializationOverloads.cpp$(DependSuffix) -MM lib/Serialization/SerializationOverloads.cpp

$(IntermediateDirectory)/lib_Serialization_SerializationOverloads.cpp$(PreprocessSuffix): lib/Serialization/SerializationOverloads.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Serialization_SerializationOverloads.cpp$(PreprocessSuffix) lib/Serialization/SerializationOverloads.cpp

$(IntermediateDirectory)/lib_Serialization_KVBinaryOutputStreamSerializer.cpp$(ObjectSuffix): lib/Serialization/KVBinaryOutputStreamSerializer.cpp $(IntermediateDirectory)/lib_Serialization_KVBinaryOutputStreamSerializer.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Serialization/KVBinaryOutputStreamSerializer.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Serialization_KVBinaryOutputStreamSerializer.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Serialization_KVBinaryOutputStreamSerializer.cpp$(DependSuffix): lib/Serialization/KVBinaryOutputStreamSerializer.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Serialization_KVBinaryOutputStreamSerializer.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Serialization_KVBinaryOutputStreamSerializer.cpp$(DependSuffix) -MM lib/Serialization/KVBinaryOutputStreamSerializer.cpp

$(IntermediateDirectory)/lib_Serialization_KVBinaryOutputStreamSerializer.cpp$(PreprocessSuffix): lib/Serialization/KVBinaryOutputStreamSerializer.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Serialization_KVBinaryOutputStreamSerializer.cpp$(PreprocessSuffix) lib/Serialization/KVBinaryOutputStreamSerializer.cpp

$(IntermediateDirectory)/lib_Serialization_JsonInputStreamSerializer.cpp$(ObjectSuffix): lib/Serialization/JsonInputStreamSerializer.cpp $(IntermediateDirectory)/lib_Serialization_JsonInputStreamSerializer.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Serialization/JsonInputStreamSerializer.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Serialization_JsonInputStreamSerializer.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Serialization_JsonInputStreamSerializer.cpp$(DependSuffix): lib/Serialization/JsonInputStreamSerializer.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Serialization_JsonInputStreamSerializer.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Serialization_JsonInputStreamSerializer.cpp$(DependSuffix) -MM lib/Serialization/JsonInputStreamSerializer.cpp

$(IntermediateDirectory)/lib_Serialization_JsonInputStreamSerializer.cpp$(PreprocessSuffix): lib/Serialization/JsonInputStreamSerializer.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Serialization_JsonInputStreamSerializer.cpp$(PreprocessSuffix) lib/Serialization/JsonInputStreamSerializer.cpp

$(IntermediateDirectory)/lib_Serialization_JsonOutputStreamSerializer.cpp$(ObjectSuffix): lib/Serialization/JsonOutputStreamSerializer.cpp $(IntermediateDirectory)/lib_Serialization_JsonOutputStreamSerializer.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Serialization/JsonOutputStreamSerializer.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Serialization_JsonOutputStreamSerializer.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Serialization_JsonOutputStreamSerializer.cpp$(DependSuffix): lib/Serialization/JsonOutputStreamSerializer.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Serialization_JsonOutputStreamSerializer.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Serialization_JsonOutputStreamSerializer.cpp$(DependSuffix) -MM lib/Serialization/JsonOutputStreamSerializer.cpp

$(IntermediateDirectory)/lib_Serialization_JsonOutputStreamSerializer.cpp$(PreprocessSuffix): lib/Serialization/JsonOutputStreamSerializer.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Serialization_JsonOutputStreamSerializer.cpp$(PreprocessSuffix) lib/Serialization/JsonOutputStreamSerializer.cpp

$(IntermediateDirectory)/lib_Serialization_JsonInputValueSerializer.cpp$(ObjectSuffix): lib/Serialization/JsonInputValueSerializer.cpp $(IntermediateDirectory)/lib_Serialization_JsonInputValueSerializer.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Serialization/JsonInputValueSerializer.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Serialization_JsonInputValueSerializer.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Serialization_JsonInputValueSerializer.cpp$(DependSuffix): lib/Serialization/JsonInputValueSerializer.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Serialization_JsonInputValueSerializer.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Serialization_JsonInputValueSerializer.cpp$(DependSuffix) -MM lib/Serialization/JsonInputValueSerializer.cpp

$(IntermediateDirectory)/lib_Serialization_JsonInputValueSerializer.cpp$(PreprocessSuffix): lib/Serialization/JsonInputValueSerializer.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Serialization_JsonInputValueSerializer.cpp$(PreprocessSuffix) lib/Serialization/JsonInputValueSerializer.cpp

$(IntermediateDirectory)/lib_Serialization_BinaryOutputStreamSerializer.cpp$(ObjectSuffix): lib/Serialization/BinaryOutputStreamSerializer.cpp $(IntermediateDirectory)/lib_Serialization_BinaryOutputStreamSerializer.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Serialization/BinaryOutputStreamSerializer.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Serialization_BinaryOutputStreamSerializer.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Serialization_BinaryOutputStreamSerializer.cpp$(DependSuffix): lib/Serialization/BinaryOutputStreamSerializer.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Serialization_BinaryOutputStreamSerializer.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Serialization_BinaryOutputStreamSerializer.cpp$(DependSuffix) -MM lib/Serialization/BinaryOutputStreamSerializer.cpp

$(IntermediateDirectory)/lib_Serialization_BinaryOutputStreamSerializer.cpp$(PreprocessSuffix): lib/Serialization/BinaryOutputStreamSerializer.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Serialization_BinaryOutputStreamSerializer.cpp$(PreprocessSuffix) lib/Serialization/BinaryOutputStreamSerializer.cpp

$(IntermediateDirectory)/lib_Serialization_BlockchainExplorerDataSerialization.cpp$(ObjectSuffix): lib/Serialization/BlockchainExplorerDataSerialization.cpp $(IntermediateDirectory)/lib_Serialization_BlockchainExplorerDataSerialization.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Serialization/BlockchainExplorerDataSerialization.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Serialization_BlockchainExplorerDataSerialization.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Serialization_BlockchainExplorerDataSerialization.cpp$(DependSuffix): lib/Serialization/BlockchainExplorerDataSerialization.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Serialization_BlockchainExplorerDataSerialization.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Serialization_BlockchainExplorerDataSerialization.cpp$(DependSuffix) -MM lib/Serialization/BlockchainExplorerDataSerialization.cpp

$(IntermediateDirectory)/lib_Serialization_BlockchainExplorerDataSerialization.cpp$(PreprocessSuffix): lib/Serialization/BlockchainExplorerDataSerialization.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Serialization_BlockchainExplorerDataSerialization.cpp$(PreprocessSuffix) lib/Serialization/BlockchainExplorerDataSerialization.cpp

$(IntermediateDirectory)/lib_Serialization_KVBinaryInputStreamSerializer.cpp$(ObjectSuffix): lib/Serialization/KVBinaryInputStreamSerializer.cpp $(IntermediateDirectory)/lib_Serialization_KVBinaryInputStreamSerializer.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Serialization/KVBinaryInputStreamSerializer.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Serialization_KVBinaryInputStreamSerializer.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Serialization_KVBinaryInputStreamSerializer.cpp$(DependSuffix): lib/Serialization/KVBinaryInputStreamSerializer.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Serialization_KVBinaryInputStreamSerializer.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Serialization_KVBinaryInputStreamSerializer.cpp$(DependSuffix) -MM lib/Serialization/KVBinaryInputStreamSerializer.cpp

$(IntermediateDirectory)/lib_Serialization_KVBinaryInputStreamSerializer.cpp$(PreprocessSuffix): lib/Serialization/KVBinaryInputStreamSerializer.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Serialization_KVBinaryInputStreamSerializer.cpp$(PreprocessSuffix) lib/Serialization/KVBinaryInputStreamSerializer.cpp

$(IntermediateDirectory)/lib_Serialization_MemoryStream.cpp$(ObjectSuffix): lib/Serialization/MemoryStream.cpp $(IntermediateDirectory)/lib_Serialization_MemoryStream.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Serialization/MemoryStream.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Serialization_MemoryStream.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Serialization_MemoryStream.cpp$(DependSuffix): lib/Serialization/MemoryStream.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Serialization_MemoryStream.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Serialization_MemoryStream.cpp$(DependSuffix) -MM lib/Serialization/MemoryStream.cpp

$(IntermediateDirectory)/lib_Serialization_MemoryStream.cpp$(PreprocessSuffix): lib/Serialization/MemoryStream.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Serialization_MemoryStream.cpp$(PreprocessSuffix) lib/Serialization/MemoryStream.cpp

$(IntermediateDirectory)/lib_Serialization_BinaryInputStreamSerializer.cpp$(ObjectSuffix): lib/Serialization/BinaryInputStreamSerializer.cpp $(IntermediateDirectory)/lib_Serialization_BinaryInputStreamSerializer.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Serialization/BinaryInputStreamSerializer.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Serialization_BinaryInputStreamSerializer.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Serialization_BinaryInputStreamSerializer.cpp$(DependSuffix): lib/Serialization/BinaryInputStreamSerializer.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Serialization_BinaryInputStreamSerializer.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Serialization_BinaryInputStreamSerializer.cpp$(DependSuffix) -MM lib/Serialization/BinaryInputStreamSerializer.cpp

$(IntermediateDirectory)/lib_Serialization_BinaryInputStreamSerializer.cpp$(PreprocessSuffix): lib/Serialization/BinaryInputStreamSerializer.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Serialization_BinaryInputStreamSerializer.cpp$(PreprocessSuffix) lib/Serialization/BinaryInputStreamSerializer.cpp

$(IntermediateDirectory)/lib_System_Ipv4Address.cpp$(ObjectSuffix): lib/System/Ipv4Address.cpp $(IntermediateDirectory)/lib_System_Ipv4Address.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/System/Ipv4Address.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_System_Ipv4Address.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_System_Ipv4Address.cpp$(DependSuffix): lib/System/Ipv4Address.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_System_Ipv4Address.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_System_Ipv4Address.cpp$(DependSuffix) -MM lib/System/Ipv4Address.cpp

$(IntermediateDirectory)/lib_System_Ipv4Address.cpp$(PreprocessSuffix): lib/System/Ipv4Address.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_System_Ipv4Address.cpp$(PreprocessSuffix) lib/System/Ipv4Address.cpp

$(IntermediateDirectory)/lib_System_ContextGroup.cpp$(ObjectSuffix): lib/System/ContextGroup.cpp $(IntermediateDirectory)/lib_System_ContextGroup.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/System/ContextGroup.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_System_ContextGroup.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_System_ContextGroup.cpp$(DependSuffix): lib/System/ContextGroup.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_System_ContextGroup.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_System_ContextGroup.cpp$(DependSuffix) -MM lib/System/ContextGroup.cpp

$(IntermediateDirectory)/lib_System_ContextGroup.cpp$(PreprocessSuffix): lib/System/ContextGroup.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_System_ContextGroup.cpp$(PreprocessSuffix) lib/System/ContextGroup.cpp

$(IntermediateDirectory)/lib_System_RemoteEventLock.cpp$(ObjectSuffix): lib/System/RemoteEventLock.cpp $(IntermediateDirectory)/lib_System_RemoteEventLock.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/System/RemoteEventLock.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_System_RemoteEventLock.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_System_RemoteEventLock.cpp$(DependSuffix): lib/System/RemoteEventLock.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_System_RemoteEventLock.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_System_RemoteEventLock.cpp$(DependSuffix) -MM lib/System/RemoteEventLock.cpp

$(IntermediateDirectory)/lib_System_RemoteEventLock.cpp$(PreprocessSuffix): lib/System/RemoteEventLock.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_System_RemoteEventLock.cpp$(PreprocessSuffix) lib/System/RemoteEventLock.cpp

$(IntermediateDirectory)/lib_System_ContextGroupTimeout.cpp$(ObjectSuffix): lib/System/ContextGroupTimeout.cpp $(IntermediateDirectory)/lib_System_ContextGroupTimeout.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/System/ContextGroupTimeout.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_System_ContextGroupTimeout.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_System_ContextGroupTimeout.cpp$(DependSuffix): lib/System/ContextGroupTimeout.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_System_ContextGroupTimeout.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_System_ContextGroupTimeout.cpp$(DependSuffix) -MM lib/System/ContextGroupTimeout.cpp

$(IntermediateDirectory)/lib_System_ContextGroupTimeout.cpp$(PreprocessSuffix): lib/System/ContextGroupTimeout.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_System_ContextGroupTimeout.cpp$(PreprocessSuffix) lib/System/ContextGroupTimeout.cpp

$(IntermediateDirectory)/lib_System_TcpStream.cpp$(ObjectSuffix): lib/System/TcpStream.cpp $(IntermediateDirectory)/lib_System_TcpStream.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/System/TcpStream.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_System_TcpStream.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_System_TcpStream.cpp$(DependSuffix): lib/System/TcpStream.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_System_TcpStream.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_System_TcpStream.cpp$(DependSuffix) -MM lib/System/TcpStream.cpp

$(IntermediateDirectory)/lib_System_TcpStream.cpp$(PreprocessSuffix): lib/System/TcpStream.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_System_TcpStream.cpp$(PreprocessSuffix) lib/System/TcpStream.cpp

$(IntermediateDirectory)/lib_System_InterruptedException.cpp$(ObjectSuffix): lib/System/InterruptedException.cpp $(IntermediateDirectory)/lib_System_InterruptedException.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/System/InterruptedException.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_System_InterruptedException.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_System_InterruptedException.cpp$(DependSuffix): lib/System/InterruptedException.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_System_InterruptedException.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_System_InterruptedException.cpp$(DependSuffix) -MM lib/System/InterruptedException.cpp

$(IntermediateDirectory)/lib_System_InterruptedException.cpp$(PreprocessSuffix): lib/System/InterruptedException.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_System_InterruptedException.cpp$(PreprocessSuffix) lib/System/InterruptedException.cpp

$(IntermediateDirectory)/lib_System_Event.cpp$(ObjectSuffix): lib/System/Event.cpp $(IntermediateDirectory)/lib_System_Event.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/System/Event.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_System_Event.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_System_Event.cpp$(DependSuffix): lib/System/Event.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_System_Event.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_System_Event.cpp$(DependSuffix) -MM lib/System/Event.cpp

$(IntermediateDirectory)/lib_System_Event.cpp$(PreprocessSuffix): lib/System/Event.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_System_Event.cpp$(PreprocessSuffix) lib/System/Event.cpp

$(IntermediateDirectory)/lib_System_EventLock.cpp$(ObjectSuffix): lib/System/EventLock.cpp $(IntermediateDirectory)/lib_System_EventLock.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/System/EventLock.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_System_EventLock.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_System_EventLock.cpp$(DependSuffix): lib/System/EventLock.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_System_EventLock.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_System_EventLock.cpp$(DependSuffix) -MM lib/System/EventLock.cpp

$(IntermediateDirectory)/lib_System_EventLock.cpp$(PreprocessSuffix): lib/System/EventLock.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_System_EventLock.cpp$(PreprocessSuffix) lib/System/EventLock.cpp

$(IntermediateDirectory)/lib_BlockchainExplorer_BlockchainExplorerDataBuilder.cpp$(ObjectSuffix): lib/BlockchainExplorer/BlockchainExplorerDataBuilder.cpp $(IntermediateDirectory)/lib_BlockchainExplorer_BlockchainExplorerDataBuilder.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/BlockchainExplorer/BlockchainExplorerDataBuilder.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_BlockchainExplorer_BlockchainExplorerDataBuilder.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_BlockchainExplorer_BlockchainExplorerDataBuilder.cpp$(DependSuffix): lib/BlockchainExplorer/BlockchainExplorerDataBuilder.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_BlockchainExplorer_BlockchainExplorerDataBuilder.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_BlockchainExplorer_BlockchainExplorerDataBuilder.cpp$(DependSuffix) -MM lib/BlockchainExplorer/BlockchainExplorerDataBuilder.cpp

$(IntermediateDirectory)/lib_BlockchainExplorer_BlockchainExplorerDataBuilder.cpp$(PreprocessSuffix): lib/BlockchainExplorer/BlockchainExplorerDataBuilder.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_BlockchainExplorer_BlockchainExplorerDataBuilder.cpp$(PreprocessSuffix) lib/BlockchainExplorer/BlockchainExplorerDataBuilder.cpp

$(IntermediateDirectory)/lib_BlockchainExplorer_BlockchainExplorer.cpp$(ObjectSuffix): lib/BlockchainExplorer/BlockchainExplorer.cpp $(IntermediateDirectory)/lib_BlockchainExplorer_BlockchainExplorer.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/BlockchainExplorer/BlockchainExplorer.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_BlockchainExplorer_BlockchainExplorer.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_BlockchainExplorer_BlockchainExplorer.cpp$(DependSuffix): lib/BlockchainExplorer/BlockchainExplorer.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_BlockchainExplorer_BlockchainExplorer.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_BlockchainExplorer_BlockchainExplorer.cpp$(DependSuffix) -MM lib/BlockchainExplorer/BlockchainExplorer.cpp

$(IntermediateDirectory)/lib_BlockchainExplorer_BlockchainExplorer.cpp$(PreprocessSuffix): lib/BlockchainExplorer/BlockchainExplorer.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_BlockchainExplorer_BlockchainExplorer.cpp$(PreprocessSuffix) lib/BlockchainExplorer/BlockchainExplorer.cpp

$(IntermediateDirectory)/lib_BlockchainExplorer_BlockchainExplorerErrors.cpp$(ObjectSuffix): lib/BlockchainExplorer/BlockchainExplorerErrors.cpp $(IntermediateDirectory)/lib_BlockchainExplorer_BlockchainExplorerErrors.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/BlockchainExplorer/BlockchainExplorerErrors.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_BlockchainExplorer_BlockchainExplorerErrors.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_BlockchainExplorer_BlockchainExplorerErrors.cpp$(DependSuffix): lib/BlockchainExplorer/BlockchainExplorerErrors.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_BlockchainExplorer_BlockchainExplorerErrors.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_BlockchainExplorer_BlockchainExplorerErrors.cpp$(DependSuffix) -MM lib/BlockchainExplorer/BlockchainExplorerErrors.cpp

$(IntermediateDirectory)/lib_BlockchainExplorer_BlockchainExplorerErrors.cpp$(PreprocessSuffix): lib/BlockchainExplorer/BlockchainExplorerErrors.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_BlockchainExplorer_BlockchainExplorerErrors.cpp$(PreprocessSuffix) lib/BlockchainExplorer/BlockchainExplorerErrors.cpp

$(IntermediateDirectory)/src_Miner_MiningConfig.cpp$(ObjectSuffix): src/Miner/MiningConfig.cpp $(IntermediateDirectory)/src_Miner_MiningConfig.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/src/Miner/MiningConfig.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/src_Miner_MiningConfig.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/src_Miner_MiningConfig.cpp$(DependSuffix): src/Miner/MiningConfig.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/src_Miner_MiningConfig.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/src_Miner_MiningConfig.cpp$(DependSuffix) -MM src/Miner/MiningConfig.cpp

$(IntermediateDirectory)/src_Miner_MiningConfig.cpp$(PreprocessSuffix): src/Miner/MiningConfig.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/src_Miner_MiningConfig.cpp$(PreprocessSuffix) src/Miner/MiningConfig.cpp

$(IntermediateDirectory)/src_Miner_MinerManager.cpp$(ObjectSuffix): src/Miner/MinerManager.cpp $(IntermediateDirectory)/src_Miner_MinerManager.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/src/Miner/MinerManager.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/src_Miner_MinerManager.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/src_Miner_MinerManager.cpp$(DependSuffix): src/Miner/MinerManager.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/src_Miner_MinerManager.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/src_Miner_MinerManager.cpp$(DependSuffix) -MM src/Miner/MinerManager.cpp

$(IntermediateDirectory)/src_Miner_MinerManager.cpp$(PreprocessSuffix): src/Miner/MinerManager.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/src_Miner_MinerManager.cpp$(PreprocessSuffix) src/Miner/MinerManager.cpp

$(IntermediateDirectory)/src_Miner_Miner.cpp$(ObjectSuffix): src/Miner/Miner.cpp $(IntermediateDirectory)/src_Miner_Miner.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/src/Miner/Miner.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/src_Miner_Miner.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/src_Miner_Miner.cpp$(DependSuffix): src/Miner/Miner.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/src_Miner_Miner.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/src_Miner_Miner.cpp$(DependSuffix) -MM src/Miner/Miner.cpp

$(IntermediateDirectory)/src_Miner_Miner.cpp$(PreprocessSuffix): src/Miner/Miner.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/src_Miner_Miner.cpp$(PreprocessSuffix) src/Miner/Miner.cpp

$(IntermediateDirectory)/src_Miner_BlockchainMonitor.cpp$(ObjectSuffix): src/Miner/BlockchainMonitor.cpp $(IntermediateDirectory)/src_Miner_BlockchainMonitor.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/src/Miner/BlockchainMonitor.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/src_Miner_BlockchainMonitor.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/src_Miner_BlockchainMonitor.cpp$(DependSuffix): src/Miner/BlockchainMonitor.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/src_Miner_BlockchainMonitor.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/src_Miner_BlockchainMonitor.cpp$(DependSuffix) -MM src/Miner/BlockchainMonitor.cpp

$(IntermediateDirectory)/src_Miner_BlockchainMonitor.cpp$(PreprocessSuffix): src/Miner/BlockchainMonitor.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/src_Miner_BlockchainMonitor.cpp$(PreprocessSuffix) src/Miner/BlockchainMonitor.cpp

$(IntermediateDirectory)/src_Miner_main.cpp$(ObjectSuffix): src/Miner/main.cpp $(IntermediateDirectory)/src_Miner_main.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/src/Miner/main.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/src_Miner_main.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/src_Miner_main.cpp$(DependSuffix): src/Miner/main.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/src_Miner_main.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/src_Miner_main.cpp$(DependSuffix) -MM src/Miner/main.cpp

$(IntermediateDirectory)/src_Miner_main.cpp$(PreprocessSuffix): src/Miner/main.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/src_Miner_main.cpp$(PreprocessSuffix) src/Miner/main.cpp

$(IntermediateDirectory)/src_Daemon_DaemonCommandsHandler.cpp$(ObjectSuffix): src/Daemon/DaemonCommandsHandler.cpp $(IntermediateDirectory)/src_Daemon_DaemonCommandsHandler.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/src/Daemon/DaemonCommandsHandler.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/src_Daemon_DaemonCommandsHandler.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/src_Daemon_DaemonCommandsHandler.cpp$(DependSuffix): src/Daemon/DaemonCommandsHandler.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/src_Daemon_DaemonCommandsHandler.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/src_Daemon_DaemonCommandsHandler.cpp$(DependSuffix) -MM src/Daemon/DaemonCommandsHandler.cpp

$(IntermediateDirectory)/src_Daemon_DaemonCommandsHandler.cpp$(PreprocessSuffix): src/Daemon/DaemonCommandsHandler.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/src_Daemon_DaemonCommandsHandler.cpp$(PreprocessSuffix) src/Daemon/DaemonCommandsHandler.cpp

$(IntermediateDirectory)/src_Daemon_Daemon.cpp$(ObjectSuffix): src/Daemon/Daemon.cpp $(IntermediateDirectory)/src_Daemon_Daemon.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/src/Daemon/Daemon.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/src_Daemon_Daemon.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/src_Daemon_Daemon.cpp$(DependSuffix): src/Daemon/Daemon.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/src_Daemon_Daemon.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/src_Daemon_Daemon.cpp$(DependSuffix) -MM src/Daemon/Daemon.cpp

$(IntermediateDirectory)/src_Daemon_Daemon.cpp$(PreprocessSuffix): src/Daemon/Daemon.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/src_Daemon_Daemon.cpp$(PreprocessSuffix) src/Daemon/Daemon.cpp

$(IntermediateDirectory)/src_ConnectivityTool_ConnectivityTool.cpp$(ObjectSuffix): src/ConnectivityTool/ConnectivityTool.cpp $(IntermediateDirectory)/src_ConnectivityTool_ConnectivityTool.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/src/ConnectivityTool/ConnectivityTool.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/src_ConnectivityTool_ConnectivityTool.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/src_ConnectivityTool_ConnectivityTool.cpp$(DependSuffix): src/ConnectivityTool/ConnectivityTool.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/src_ConnectivityTool_ConnectivityTool.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/src_ConnectivityTool_ConnectivityTool.cpp$(DependSuffix) -MM src/ConnectivityTool/ConnectivityTool.cpp

$(IntermediateDirectory)/src_ConnectivityTool_ConnectivityTool.cpp$(PreprocessSuffix): src/ConnectivityTool/ConnectivityTool.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/src_ConnectivityTool_ConnectivityTool.cpp$(PreprocessSuffix) src/ConnectivityTool/ConnectivityTool.cpp

$(IntermediateDirectory)/src_PaymentGateService_PaymentGateService.cpp$(ObjectSuffix): src/PaymentGateService/PaymentGateService.cpp $(IntermediateDirectory)/src_PaymentGateService_PaymentGateService.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/src/PaymentGateService/PaymentGateService.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/src_PaymentGateService_PaymentGateService.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/src_PaymentGateService_PaymentGateService.cpp$(DependSuffix): src/PaymentGateService/PaymentGateService.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/src_PaymentGateService_PaymentGateService.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/src_PaymentGateService_PaymentGateService.cpp$(DependSuffix) -MM src/PaymentGateService/PaymentGateService.cpp

$(IntermediateDirectory)/src_PaymentGateService_PaymentGateService.cpp$(PreprocessSuffix): src/PaymentGateService/PaymentGateService.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/src_PaymentGateService_PaymentGateService.cpp$(PreprocessSuffix) src/PaymentGateService/PaymentGateService.cpp

$(IntermediateDirectory)/src_PaymentGateService_RpcNodeConfiguration.cpp$(ObjectSuffix): src/PaymentGateService/RpcNodeConfiguration.cpp $(IntermediateDirectory)/src_PaymentGateService_RpcNodeConfiguration.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/src/PaymentGateService/RpcNodeConfiguration.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/src_PaymentGateService_RpcNodeConfiguration.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/src_PaymentGateService_RpcNodeConfiguration.cpp$(DependSuffix): src/PaymentGateService/RpcNodeConfiguration.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/src_PaymentGateService_RpcNodeConfiguration.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/src_PaymentGateService_RpcNodeConfiguration.cpp$(DependSuffix) -MM src/PaymentGateService/RpcNodeConfiguration.cpp

$(IntermediateDirectory)/src_PaymentGateService_RpcNodeConfiguration.cpp$(PreprocessSuffix): src/PaymentGateService/RpcNodeConfiguration.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/src_PaymentGateService_RpcNodeConfiguration.cpp$(PreprocessSuffix) src/PaymentGateService/RpcNodeConfiguration.cpp

$(IntermediateDirectory)/src_PaymentGateService_PaymentServiceConfiguration.cpp$(ObjectSuffix): src/PaymentGateService/PaymentServiceConfiguration.cpp $(IntermediateDirectory)/src_PaymentGateService_PaymentServiceConfiguration.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/src/PaymentGateService/PaymentServiceConfiguration.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/src_PaymentGateService_PaymentServiceConfiguration.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/src_PaymentGateService_PaymentServiceConfiguration.cpp$(DependSuffix): src/PaymentGateService/PaymentServiceConfiguration.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/src_PaymentGateService_PaymentServiceConfiguration.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/src_PaymentGateService_PaymentServiceConfiguration.cpp$(DependSuffix) -MM src/PaymentGateService/PaymentServiceConfiguration.cpp

$(IntermediateDirectory)/src_PaymentGateService_PaymentServiceConfiguration.cpp$(PreprocessSuffix): src/PaymentGateService/PaymentServiceConfiguration.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/src_PaymentGateService_PaymentServiceConfiguration.cpp$(PreprocessSuffix) src/PaymentGateService/PaymentServiceConfiguration.cpp

$(IntermediateDirectory)/src_PaymentGateService_ConfigurationManager.cpp$(ObjectSuffix): src/PaymentGateService/ConfigurationManager.cpp $(IntermediateDirectory)/src_PaymentGateService_ConfigurationManager.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/src/PaymentGateService/ConfigurationManager.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/src_PaymentGateService_ConfigurationManager.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/src_PaymentGateService_ConfigurationManager.cpp$(DependSuffix): src/PaymentGateService/ConfigurationManager.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/src_PaymentGateService_ConfigurationManager.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/src_PaymentGateService_ConfigurationManager.cpp$(DependSuffix) -MM src/PaymentGateService/ConfigurationManager.cpp

$(IntermediateDirectory)/src_PaymentGateService_ConfigurationManager.cpp$(PreprocessSuffix): src/PaymentGateService/ConfigurationManager.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/src_PaymentGateService_ConfigurationManager.cpp$(PreprocessSuffix) src/PaymentGateService/ConfigurationManager.cpp

$(IntermediateDirectory)/src_PaymentGateService_main.cpp$(ObjectSuffix): src/PaymentGateService/main.cpp $(IntermediateDirectory)/src_PaymentGateService_main.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/src/PaymentGateService/main.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/src_PaymentGateService_main.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/src_PaymentGateService_main.cpp$(DependSuffix): src/PaymentGateService/main.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/src_PaymentGateService_main.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/src_PaymentGateService_main.cpp$(DependSuffix) -MM src/PaymentGateService/main.cpp

$(IntermediateDirectory)/src_PaymentGateService_main.cpp$(PreprocessSuffix): src/PaymentGateService/main.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/src_PaymentGateService_main.cpp$(PreprocessSuffix) src/PaymentGateService/main.cpp

$(IntermediateDirectory)/src_SimpleWallet_SimpleWallet.cpp$(ObjectSuffix): src/SimpleWallet/SimpleWallet.cpp $(IntermediateDirectory)/src_SimpleWallet_SimpleWallet.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/src/SimpleWallet/SimpleWallet.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/src_SimpleWallet_SimpleWallet.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/src_SimpleWallet_SimpleWallet.cpp$(DependSuffix): src/SimpleWallet/SimpleWallet.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/src_SimpleWallet_SimpleWallet.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/src_SimpleWallet_SimpleWallet.cpp$(DependSuffix) -MM src/SimpleWallet/SimpleWallet.cpp

$(IntermediateDirectory)/src_SimpleWallet_SimpleWallet.cpp$(PreprocessSuffix): src/SimpleWallet/SimpleWallet.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/src_SimpleWallet_SimpleWallet.cpp$(PreprocessSuffix) src/SimpleWallet/SimpleWallet.cpp

$(IntermediateDirectory)/src_SimpleWallet_PasswordContainer.cpp$(ObjectSuffix): src/SimpleWallet/PasswordContainer.cpp $(IntermediateDirectory)/src_SimpleWallet_PasswordContainer.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/src/SimpleWallet/PasswordContainer.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/src_SimpleWallet_PasswordContainer.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/src_SimpleWallet_PasswordContainer.cpp$(DependSuffix): src/SimpleWallet/PasswordContainer.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/src_SimpleWallet_PasswordContainer.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/src_SimpleWallet_PasswordContainer.cpp$(DependSuffix) -MM src/SimpleWallet/PasswordContainer.cpp

$(IntermediateDirectory)/src_SimpleWallet_PasswordContainer.cpp$(PreprocessSuffix): src/SimpleWallet/PasswordContainer.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/src_SimpleWallet_PasswordContainer.cpp$(PreprocessSuffix) src/SimpleWallet/PasswordContainer.cpp

$(IntermediateDirectory)/build_CMakeFiles_feature_tests.cxx$(ObjectSuffix): build/CMakeFiles/feature_tests.cxx $(IntermediateDirectory)/build_CMakeFiles_feature_tests.cxx$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/build/CMakeFiles/feature_tests.cxx" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/build_CMakeFiles_feature_tests.cxx$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/build_CMakeFiles_feature_tests.cxx$(DependSuffix): build/CMakeFiles/feature_tests.cxx
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/build_CMakeFiles_feature_tests.cxx$(ObjectSuffix) -MF$(IntermediateDirectory)/build_CMakeFiles_feature_tests.cxx$(DependSuffix) -MM build/CMakeFiles/feature_tests.cxx

$(IntermediateDirectory)/build_CMakeFiles_feature_tests.cxx$(PreprocessSuffix): build/CMakeFiles/feature_tests.cxx
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/build_CMakeFiles_feature_tests.cxx$(PreprocessSuffix) build/CMakeFiles/feature_tests.cxx

$(IntermediateDirectory)/build_CMakeFiles_feature_tests.c$(ObjectSuffix): build/CMakeFiles/feature_tests.c $(IntermediateDirectory)/build_CMakeFiles_feature_tests.c$(DependSuffix)
	$(CC) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/build/CMakeFiles/feature_tests.c" $(CFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/build_CMakeFiles_feature_tests.c$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/build_CMakeFiles_feature_tests.c$(DependSuffix): build/CMakeFiles/feature_tests.c
	@$(CC) $(CFLAGS) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/build_CMakeFiles_feature_tests.c$(ObjectSuffix) -MF$(IntermediateDirectory)/build_CMakeFiles_feature_tests.c$(DependSuffix) -MM build/CMakeFiles/feature_tests.c

$(IntermediateDirectory)/build_CMakeFiles_feature_tests.c$(PreprocessSuffix): build/CMakeFiles/feature_tests.c
	$(CC) $(CFLAGS) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/build_CMakeFiles_feature_tests.c$(PreprocessSuffix) build/CMakeFiles/feature_tests.c

$(IntermediateDirectory)/tests_HashTests_main.cpp$(ObjectSuffix): tests/HashTests/main.cpp $(IntermediateDirectory)/tests_HashTests_main.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/HashTests/main.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_HashTests_main.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_HashTests_main.cpp$(DependSuffix): tests/HashTests/main.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_HashTests_main.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_HashTests_main.cpp$(DependSuffix) -MM tests/HashTests/main.cpp

$(IntermediateDirectory)/tests_HashTests_main.cpp$(PreprocessSuffix): tests/HashTests/main.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_HashTests_main.cpp$(PreprocessSuffix) tests/HashTests/main.cpp

$(IntermediateDirectory)/tests_SystemTests_EventLockTests.cpp$(ObjectSuffix): tests/SystemTests/EventLockTests.cpp $(IntermediateDirectory)/tests_SystemTests_EventLockTests.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/SystemTests/EventLockTests.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_SystemTests_EventLockTests.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_SystemTests_EventLockTests.cpp$(DependSuffix): tests/SystemTests/EventLockTests.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_SystemTests_EventLockTests.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_SystemTests_EventLockTests.cpp$(DependSuffix) -MM tests/SystemTests/EventLockTests.cpp

$(IntermediateDirectory)/tests_SystemTests_EventLockTests.cpp$(PreprocessSuffix): tests/SystemTests/EventLockTests.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_SystemTests_EventLockTests.cpp$(PreprocessSuffix) tests/SystemTests/EventLockTests.cpp

$(IntermediateDirectory)/tests_SystemTests_EventTests.cpp$(ObjectSuffix): tests/SystemTests/EventTests.cpp $(IntermediateDirectory)/tests_SystemTests_EventTests.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/SystemTests/EventTests.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_SystemTests_EventTests.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_SystemTests_EventTests.cpp$(DependSuffix): tests/SystemTests/EventTests.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_SystemTests_EventTests.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_SystemTests_EventTests.cpp$(DependSuffix) -MM tests/SystemTests/EventTests.cpp

$(IntermediateDirectory)/tests_SystemTests_EventTests.cpp$(PreprocessSuffix): tests/SystemTests/EventTests.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_SystemTests_EventTests.cpp$(PreprocessSuffix) tests/SystemTests/EventTests.cpp

$(IntermediateDirectory)/tests_SystemTests_TcpConnectorTests.cpp$(ObjectSuffix): tests/SystemTests/TcpConnectorTests.cpp $(IntermediateDirectory)/tests_SystemTests_TcpConnectorTests.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/SystemTests/TcpConnectorTests.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_SystemTests_TcpConnectorTests.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_SystemTests_TcpConnectorTests.cpp$(DependSuffix): tests/SystemTests/TcpConnectorTests.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_SystemTests_TcpConnectorTests.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_SystemTests_TcpConnectorTests.cpp$(DependSuffix) -MM tests/SystemTests/TcpConnectorTests.cpp

$(IntermediateDirectory)/tests_SystemTests_TcpConnectorTests.cpp$(PreprocessSuffix): tests/SystemTests/TcpConnectorTests.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_SystemTests_TcpConnectorTests.cpp$(PreprocessSuffix) tests/SystemTests/TcpConnectorTests.cpp

$(IntermediateDirectory)/tests_SystemTests_TimerTests.cpp$(ObjectSuffix): tests/SystemTests/TimerTests.cpp $(IntermediateDirectory)/tests_SystemTests_TimerTests.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/SystemTests/TimerTests.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_SystemTests_TimerTests.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_SystemTests_TimerTests.cpp$(DependSuffix): tests/SystemTests/TimerTests.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_SystemTests_TimerTests.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_SystemTests_TimerTests.cpp$(DependSuffix) -MM tests/SystemTests/TimerTests.cpp

$(IntermediateDirectory)/tests_SystemTests_TimerTests.cpp$(PreprocessSuffix): tests/SystemTests/TimerTests.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_SystemTests_TimerTests.cpp$(PreprocessSuffix) tests/SystemTests/TimerTests.cpp

$(IntermediateDirectory)/tests_SystemTests_OperationTimeoutTests.cpp$(ObjectSuffix): tests/SystemTests/OperationTimeoutTests.cpp $(IntermediateDirectory)/tests_SystemTests_OperationTimeoutTests.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/SystemTests/OperationTimeoutTests.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_SystemTests_OperationTimeoutTests.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_SystemTests_OperationTimeoutTests.cpp$(DependSuffix): tests/SystemTests/OperationTimeoutTests.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_SystemTests_OperationTimeoutTests.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_SystemTests_OperationTimeoutTests.cpp$(DependSuffix) -MM tests/SystemTests/OperationTimeoutTests.cpp

$(IntermediateDirectory)/tests_SystemTests_OperationTimeoutTests.cpp$(PreprocessSuffix): tests/SystemTests/OperationTimeoutTests.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_SystemTests_OperationTimeoutTests.cpp$(PreprocessSuffix) tests/SystemTests/OperationTimeoutTests.cpp

$(IntermediateDirectory)/tests_SystemTests_ContextTests.cpp$(ObjectSuffix): tests/SystemTests/ContextTests.cpp $(IntermediateDirectory)/tests_SystemTests_ContextTests.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/SystemTests/ContextTests.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_SystemTests_ContextTests.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_SystemTests_ContextTests.cpp$(DependSuffix): tests/SystemTests/ContextTests.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_SystemTests_ContextTests.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_SystemTests_ContextTests.cpp$(DependSuffix) -MM tests/SystemTests/ContextTests.cpp

$(IntermediateDirectory)/tests_SystemTests_ContextTests.cpp$(PreprocessSuffix): tests/SystemTests/ContextTests.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_SystemTests_ContextTests.cpp$(PreprocessSuffix) tests/SystemTests/ContextTests.cpp

$(IntermediateDirectory)/tests_SystemTests_ErrorMessageTests.cpp$(ObjectSuffix): tests/SystemTests/ErrorMessageTests.cpp $(IntermediateDirectory)/tests_SystemTests_ErrorMessageTests.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/SystemTests/ErrorMessageTests.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_SystemTests_ErrorMessageTests.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_SystemTests_ErrorMessageTests.cpp$(DependSuffix): tests/SystemTests/ErrorMessageTests.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_SystemTests_ErrorMessageTests.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_SystemTests_ErrorMessageTests.cpp$(DependSuffix) -MM tests/SystemTests/ErrorMessageTests.cpp

$(IntermediateDirectory)/tests_SystemTests_ErrorMessageTests.cpp$(PreprocessSuffix): tests/SystemTests/ErrorMessageTests.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_SystemTests_ErrorMessageTests.cpp$(PreprocessSuffix) tests/SystemTests/ErrorMessageTests.cpp

$(IntermediateDirectory)/tests_SystemTests_Ipv4ResolverTests.cpp$(ObjectSuffix): tests/SystemTests/Ipv4ResolverTests.cpp $(IntermediateDirectory)/tests_SystemTests_Ipv4ResolverTests.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/SystemTests/Ipv4ResolverTests.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_SystemTests_Ipv4ResolverTests.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_SystemTests_Ipv4ResolverTests.cpp$(DependSuffix): tests/SystemTests/Ipv4ResolverTests.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_SystemTests_Ipv4ResolverTests.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_SystemTests_Ipv4ResolverTests.cpp$(DependSuffix) -MM tests/SystemTests/Ipv4ResolverTests.cpp

$(IntermediateDirectory)/tests_SystemTests_Ipv4ResolverTests.cpp$(PreprocessSuffix): tests/SystemTests/Ipv4ResolverTests.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_SystemTests_Ipv4ResolverTests.cpp$(PreprocessSuffix) tests/SystemTests/Ipv4ResolverTests.cpp

$(IntermediateDirectory)/tests_SystemTests_RemoteContextTests.cpp$(ObjectSuffix): tests/SystemTests/RemoteContextTests.cpp $(IntermediateDirectory)/tests_SystemTests_RemoteContextTests.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/SystemTests/RemoteContextTests.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_SystemTests_RemoteContextTests.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_SystemTests_RemoteContextTests.cpp$(DependSuffix): tests/SystemTests/RemoteContextTests.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_SystemTests_RemoteContextTests.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_SystemTests_RemoteContextTests.cpp$(DependSuffix) -MM tests/SystemTests/RemoteContextTests.cpp

$(IntermediateDirectory)/tests_SystemTests_RemoteContextTests.cpp$(PreprocessSuffix): tests/SystemTests/RemoteContextTests.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_SystemTests_RemoteContextTests.cpp$(PreprocessSuffix) tests/SystemTests/RemoteContextTests.cpp

$(IntermediateDirectory)/tests_SystemTests_TcpConnectionTests.cpp$(ObjectSuffix): tests/SystemTests/TcpConnectionTests.cpp $(IntermediateDirectory)/tests_SystemTests_TcpConnectionTests.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/SystemTests/TcpConnectionTests.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_SystemTests_TcpConnectionTests.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_SystemTests_TcpConnectionTests.cpp$(DependSuffix): tests/SystemTests/TcpConnectionTests.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_SystemTests_TcpConnectionTests.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_SystemTests_TcpConnectionTests.cpp$(DependSuffix) -MM tests/SystemTests/TcpConnectionTests.cpp

$(IntermediateDirectory)/tests_SystemTests_TcpConnectionTests.cpp$(PreprocessSuffix): tests/SystemTests/TcpConnectionTests.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_SystemTests_TcpConnectionTests.cpp$(PreprocessSuffix) tests/SystemTests/TcpConnectionTests.cpp

$(IntermediateDirectory)/tests_SystemTests_ContextGroupTimeoutTests.cpp$(ObjectSuffix): tests/SystemTests/ContextGroupTimeoutTests.cpp $(IntermediateDirectory)/tests_SystemTests_ContextGroupTimeoutTests.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/SystemTests/ContextGroupTimeoutTests.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_SystemTests_ContextGroupTimeoutTests.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_SystemTests_ContextGroupTimeoutTests.cpp$(DependSuffix): tests/SystemTests/ContextGroupTimeoutTests.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_SystemTests_ContextGroupTimeoutTests.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_SystemTests_ContextGroupTimeoutTests.cpp$(DependSuffix) -MM tests/SystemTests/ContextGroupTimeoutTests.cpp

$(IntermediateDirectory)/tests_SystemTests_ContextGroupTimeoutTests.cpp$(PreprocessSuffix): tests/SystemTests/ContextGroupTimeoutTests.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_SystemTests_ContextGroupTimeoutTests.cpp$(PreprocessSuffix) tests/SystemTests/ContextGroupTimeoutTests.cpp

$(IntermediateDirectory)/tests_SystemTests_ContextGroupTests.cpp$(ObjectSuffix): tests/SystemTests/ContextGroupTests.cpp $(IntermediateDirectory)/tests_SystemTests_ContextGroupTests.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/SystemTests/ContextGroupTests.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_SystemTests_ContextGroupTests.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_SystemTests_ContextGroupTests.cpp$(DependSuffix): tests/SystemTests/ContextGroupTests.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_SystemTests_ContextGroupTests.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_SystemTests_ContextGroupTests.cpp$(DependSuffix) -MM tests/SystemTests/ContextGroupTests.cpp

$(IntermediateDirectory)/tests_SystemTests_ContextGroupTests.cpp$(PreprocessSuffix): tests/SystemTests/ContextGroupTests.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_SystemTests_ContextGroupTests.cpp$(PreprocessSuffix) tests/SystemTests/ContextGroupTests.cpp

$(IntermediateDirectory)/tests_SystemTests_Ipv4AddressTests.cpp$(ObjectSuffix): tests/SystemTests/Ipv4AddressTests.cpp $(IntermediateDirectory)/tests_SystemTests_Ipv4AddressTests.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/SystemTests/Ipv4AddressTests.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_SystemTests_Ipv4AddressTests.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_SystemTests_Ipv4AddressTests.cpp$(DependSuffix): tests/SystemTests/Ipv4AddressTests.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_SystemTests_Ipv4AddressTests.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_SystemTests_Ipv4AddressTests.cpp$(DependSuffix) -MM tests/SystemTests/Ipv4AddressTests.cpp

$(IntermediateDirectory)/tests_SystemTests_Ipv4AddressTests.cpp$(PreprocessSuffix): tests/SystemTests/Ipv4AddressTests.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_SystemTests_Ipv4AddressTests.cpp$(PreprocessSuffix) tests/SystemTests/Ipv4AddressTests.cpp

$(IntermediateDirectory)/tests_SystemTests_TcpListenerTests.cpp$(ObjectSuffix): tests/SystemTests/TcpListenerTests.cpp $(IntermediateDirectory)/tests_SystemTests_TcpListenerTests.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/SystemTests/TcpListenerTests.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_SystemTests_TcpListenerTests.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_SystemTests_TcpListenerTests.cpp$(DependSuffix): tests/SystemTests/TcpListenerTests.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_SystemTests_TcpListenerTests.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_SystemTests_TcpListenerTests.cpp$(DependSuffix) -MM tests/SystemTests/TcpListenerTests.cpp

$(IntermediateDirectory)/tests_SystemTests_TcpListenerTests.cpp$(PreprocessSuffix): tests/SystemTests/TcpListenerTests.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_SystemTests_TcpListenerTests.cpp$(PreprocessSuffix) tests/SystemTests/TcpListenerTests.cpp

$(IntermediateDirectory)/tests_SystemTests_main.cpp$(ObjectSuffix): tests/SystemTests/main.cpp $(IntermediateDirectory)/tests_SystemTests_main.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/SystemTests/main.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_SystemTests_main.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_SystemTests_main.cpp$(DependSuffix): tests/SystemTests/main.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_SystemTests_main.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_SystemTests_main.cpp$(DependSuffix) -MM tests/SystemTests/main.cpp

$(IntermediateDirectory)/tests_SystemTests_main.cpp$(PreprocessSuffix): tests/SystemTests/main.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_SystemTests_main.cpp$(PreprocessSuffix) tests/SystemTests/main.cpp

$(IntermediateDirectory)/tests_SystemTests_DispatcherTests.cpp$(ObjectSuffix): tests/SystemTests/DispatcherTests.cpp $(IntermediateDirectory)/tests_SystemTests_DispatcherTests.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/SystemTests/DispatcherTests.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_SystemTests_DispatcherTests.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_SystemTests_DispatcherTests.cpp$(DependSuffix): tests/SystemTests/DispatcherTests.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_SystemTests_DispatcherTests.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_SystemTests_DispatcherTests.cpp$(DependSuffix) -MM tests/SystemTests/DispatcherTests.cpp

$(IntermediateDirectory)/tests_SystemTests_DispatcherTests.cpp$(PreprocessSuffix): tests/SystemTests/DispatcherTests.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_SystemTests_DispatcherTests.cpp$(PreprocessSuffix) tests/SystemTests/DispatcherTests.cpp

$(IntermediateDirectory)/tests_IntegrationTests_WalletLegacyTests.cpp$(ObjectSuffix): tests/IntegrationTests/WalletLegacyTests.cpp $(IntermediateDirectory)/tests_IntegrationTests_WalletLegacyTests.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/IntegrationTests/WalletLegacyTests.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_IntegrationTests_WalletLegacyTests.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_IntegrationTests_WalletLegacyTests.cpp$(DependSuffix): tests/IntegrationTests/WalletLegacyTests.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_IntegrationTests_WalletLegacyTests.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_IntegrationTests_WalletLegacyTests.cpp$(DependSuffix) -MM tests/IntegrationTests/WalletLegacyTests.cpp

$(IntermediateDirectory)/tests_IntegrationTests_WalletLegacyTests.cpp$(PreprocessSuffix): tests/IntegrationTests/WalletLegacyTests.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_IntegrationTests_WalletLegacyTests.cpp$(PreprocessSuffix) tests/IntegrationTests/WalletLegacyTests.cpp

$(IntermediateDirectory)/tests_IntegrationTests_MultiVersion.cpp$(ObjectSuffix): tests/IntegrationTests/MultiVersion.cpp $(IntermediateDirectory)/tests_IntegrationTests_MultiVersion.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/IntegrationTests/MultiVersion.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_IntegrationTests_MultiVersion.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_IntegrationTests_MultiVersion.cpp$(DependSuffix): tests/IntegrationTests/MultiVersion.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_IntegrationTests_MultiVersion.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_IntegrationTests_MultiVersion.cpp$(DependSuffix) -MM tests/IntegrationTests/MultiVersion.cpp

$(IntermediateDirectory)/tests_IntegrationTests_MultiVersion.cpp$(PreprocessSuffix): tests/IntegrationTests/MultiVersion.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_IntegrationTests_MultiVersion.cpp$(PreprocessSuffix) tests/IntegrationTests/MultiVersion.cpp

$(IntermediateDirectory)/tests_IntegrationTests_Node.cpp$(ObjectSuffix): tests/IntegrationTests/Node.cpp $(IntermediateDirectory)/tests_IntegrationTests_Node.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/IntegrationTests/Node.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_IntegrationTests_Node.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_IntegrationTests_Node.cpp$(DependSuffix): tests/IntegrationTests/Node.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_IntegrationTests_Node.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_IntegrationTests_Node.cpp$(DependSuffix) -MM tests/IntegrationTests/Node.cpp

$(IntermediateDirectory)/tests_IntegrationTests_Node.cpp$(PreprocessSuffix): tests/IntegrationTests/Node.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_IntegrationTests_Node.cpp$(PreprocessSuffix) tests/IntegrationTests/Node.cpp

$(IntermediateDirectory)/tests_IntegrationTests_IntegrationTests.cpp$(ObjectSuffix): tests/IntegrationTests/IntegrationTests.cpp $(IntermediateDirectory)/tests_IntegrationTests_IntegrationTests.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/IntegrationTests/IntegrationTests.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_IntegrationTests_IntegrationTests.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_IntegrationTests_IntegrationTests.cpp$(DependSuffix): tests/IntegrationTests/IntegrationTests.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_IntegrationTests_IntegrationTests.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_IntegrationTests_IntegrationTests.cpp$(DependSuffix) -MM tests/IntegrationTests/IntegrationTests.cpp

$(IntermediateDirectory)/tests_IntegrationTests_IntegrationTests.cpp$(PreprocessSuffix): tests/IntegrationTests/IntegrationTests.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_IntegrationTests_IntegrationTests.cpp$(PreprocessSuffix) tests/IntegrationTests/IntegrationTests.cpp

$(IntermediateDirectory)/tests_IntegrationTests_main.cpp$(ObjectSuffix): tests/IntegrationTests/main.cpp $(IntermediateDirectory)/tests_IntegrationTests_main.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/IntegrationTests/main.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_IntegrationTests_main.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_IntegrationTests_main.cpp$(DependSuffix): tests/IntegrationTests/main.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_IntegrationTests_main.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_IntegrationTests_main.cpp$(DependSuffix) -MM tests/IntegrationTests/main.cpp

$(IntermediateDirectory)/tests_IntegrationTests_main.cpp$(PreprocessSuffix): tests/IntegrationTests/main.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_IntegrationTests_main.cpp$(PreprocessSuffix) tests/IntegrationTests/main.cpp

$(IntermediateDirectory)/tests_CryptoTests_crypto-ops.c$(ObjectSuffix): tests/CryptoTests/crypto-ops.c $(IntermediateDirectory)/tests_CryptoTests_crypto-ops.c$(DependSuffix)
	$(CC) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/CryptoTests/crypto-ops.c" $(CFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_CryptoTests_crypto-ops.c$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_CryptoTests_crypto-ops.c$(DependSuffix): tests/CryptoTests/crypto-ops.c
	@$(CC) $(CFLAGS) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_CryptoTests_crypto-ops.c$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_CryptoTests_crypto-ops.c$(DependSuffix) -MM tests/CryptoTests/crypto-ops.c

$(IntermediateDirectory)/tests_CryptoTests_crypto-ops.c$(PreprocessSuffix): tests/CryptoTests/crypto-ops.c
	$(CC) $(CFLAGS) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_CryptoTests_crypto-ops.c$(PreprocessSuffix) tests/CryptoTests/crypto-ops.c

$(IntermediateDirectory)/tests_CryptoTests_random.c$(ObjectSuffix): tests/CryptoTests/random.c $(IntermediateDirectory)/tests_CryptoTests_random.c$(DependSuffix)
	$(CC) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/CryptoTests/random.c" $(CFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_CryptoTests_random.c$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_CryptoTests_random.c$(DependSuffix): tests/CryptoTests/random.c
	@$(CC) $(CFLAGS) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_CryptoTests_random.c$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_CryptoTests_random.c$(DependSuffix) -MM tests/CryptoTests/random.c

$(IntermediateDirectory)/tests_CryptoTests_random.c$(PreprocessSuffix): tests/CryptoTests/random.c
	$(CC) $(CFLAGS) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_CryptoTests_random.c$(PreprocessSuffix) tests/CryptoTests/random.c

$(IntermediateDirectory)/tests_CryptoTests_crypto-ops-data.c$(ObjectSuffix): tests/CryptoTests/crypto-ops-data.c $(IntermediateDirectory)/tests_CryptoTests_crypto-ops-data.c$(DependSuffix)
	$(CC) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/CryptoTests/crypto-ops-data.c" $(CFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_CryptoTests_crypto-ops-data.c$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_CryptoTests_crypto-ops-data.c$(DependSuffix): tests/CryptoTests/crypto-ops-data.c
	@$(CC) $(CFLAGS) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_CryptoTests_crypto-ops-data.c$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_CryptoTests_crypto-ops-data.c$(DependSuffix) -MM tests/CryptoTests/crypto-ops-data.c

$(IntermediateDirectory)/tests_CryptoTests_crypto-ops-data.c$(PreprocessSuffix): tests/CryptoTests/crypto-ops-data.c
	$(CC) $(CFLAGS) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_CryptoTests_crypto-ops-data.c$(PreprocessSuffix) tests/CryptoTests/crypto-ops-data.c

$(IntermediateDirectory)/tests_CryptoTests_main.cpp$(ObjectSuffix): tests/CryptoTests/main.cpp $(IntermediateDirectory)/tests_CryptoTests_main.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/CryptoTests/main.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_CryptoTests_main.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_CryptoTests_main.cpp$(DependSuffix): tests/CryptoTests/main.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_CryptoTests_main.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_CryptoTests_main.cpp$(DependSuffix) -MM tests/CryptoTests/main.cpp

$(IntermediateDirectory)/tests_CryptoTests_main.cpp$(PreprocessSuffix): tests/CryptoTests/main.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_CryptoTests_main.cpp$(PreprocessSuffix) tests/CryptoTests/main.cpp

$(IntermediateDirectory)/tests_CryptoTests_hash.c$(ObjectSuffix): tests/CryptoTests/hash.c $(IntermediateDirectory)/tests_CryptoTests_hash.c$(DependSuffix)
	$(CC) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/CryptoTests/hash.c" $(CFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_CryptoTests_hash.c$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_CryptoTests_hash.c$(DependSuffix): tests/CryptoTests/hash.c
	@$(CC) $(CFLAGS) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_CryptoTests_hash.c$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_CryptoTests_hash.c$(DependSuffix) -MM tests/CryptoTests/hash.c

$(IntermediateDirectory)/tests_CryptoTests_hash.c$(PreprocessSuffix): tests/CryptoTests/hash.c
	$(CC) $(CFLAGS) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_CryptoTests_hash.c$(PreprocessSuffix) tests/CryptoTests/hash.c

$(IntermediateDirectory)/tests_CryptoTests_crypto.cpp$(ObjectSuffix): tests/CryptoTests/crypto.cpp $(IntermediateDirectory)/tests_CryptoTests_crypto.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/CryptoTests/crypto.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_CryptoTests_crypto.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_CryptoTests_crypto.cpp$(DependSuffix): tests/CryptoTests/crypto.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_CryptoTests_crypto.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_CryptoTests_crypto.cpp$(DependSuffix) -MM tests/CryptoTests/crypto.cpp

$(IntermediateDirectory)/tests_CryptoTests_crypto.cpp$(PreprocessSuffix): tests/CryptoTests/crypto.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_CryptoTests_crypto.cpp$(PreprocessSuffix) tests/CryptoTests/crypto.cpp

$(IntermediateDirectory)/tests_TestGenerator_TestGenerator.cpp$(ObjectSuffix): tests/TestGenerator/TestGenerator.cpp $(IntermediateDirectory)/tests_TestGenerator_TestGenerator.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/TestGenerator/TestGenerator.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_TestGenerator_TestGenerator.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_TestGenerator_TestGenerator.cpp$(DependSuffix): tests/TestGenerator/TestGenerator.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_TestGenerator_TestGenerator.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_TestGenerator_TestGenerator.cpp$(DependSuffix) -MM tests/TestGenerator/TestGenerator.cpp

$(IntermediateDirectory)/tests_TestGenerator_TestGenerator.cpp$(PreprocessSuffix): tests/TestGenerator/TestGenerator.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_TestGenerator_TestGenerator.cpp$(PreprocessSuffix) tests/TestGenerator/TestGenerator.cpp

$(IntermediateDirectory)/tests_PerformanceTests_main.cpp$(ObjectSuffix): tests/PerformanceTests/main.cpp $(IntermediateDirectory)/tests_PerformanceTests_main.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/PerformanceTests/main.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_PerformanceTests_main.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_PerformanceTests_main.cpp$(DependSuffix): tests/PerformanceTests/main.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_PerformanceTests_main.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_PerformanceTests_main.cpp$(DependSuffix) -MM tests/PerformanceTests/main.cpp

$(IntermediateDirectory)/tests_PerformanceTests_main.cpp$(PreprocessSuffix): tests/PerformanceTests/main.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_PerformanceTests_main.cpp$(PreprocessSuffix) tests/PerformanceTests/main.cpp

$(IntermediateDirectory)/tests_NodeRpcProxyTests_NodeRpcProxyTests.cpp$(ObjectSuffix): tests/NodeRpcProxyTests/NodeRpcProxyTests.cpp $(IntermediateDirectory)/tests_NodeRpcProxyTests_NodeRpcProxyTests.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/NodeRpcProxyTests/NodeRpcProxyTests.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_NodeRpcProxyTests_NodeRpcProxyTests.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_NodeRpcProxyTests_NodeRpcProxyTests.cpp$(DependSuffix): tests/NodeRpcProxyTests/NodeRpcProxyTests.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_NodeRpcProxyTests_NodeRpcProxyTests.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_NodeRpcProxyTests_NodeRpcProxyTests.cpp$(DependSuffix) -MM tests/NodeRpcProxyTests/NodeRpcProxyTests.cpp

$(IntermediateDirectory)/tests_NodeRpcProxyTests_NodeRpcProxyTests.cpp$(PreprocessSuffix): tests/NodeRpcProxyTests/NodeRpcProxyTests.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_NodeRpcProxyTests_NodeRpcProxyTests.cpp$(PreprocessSuffix) tests/NodeRpcProxyTests/NodeRpcProxyTests.cpp

$(IntermediateDirectory)/tests_UnitTests_TestTransfersContainer.cpp$(ObjectSuffix): tests/UnitTests/TestTransfersContainer.cpp $(IntermediateDirectory)/tests_UnitTests_TestTransfersContainer.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/UnitTests/TestTransfersContainer.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_UnitTests_TestTransfersContainer.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_UnitTests_TestTransfersContainer.cpp$(DependSuffix): tests/UnitTests/TestTransfersContainer.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_UnitTests_TestTransfersContainer.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_UnitTests_TestTransfersContainer.cpp$(DependSuffix) -MM tests/UnitTests/TestTransfersContainer.cpp

$(IntermediateDirectory)/tests_UnitTests_TestTransfersContainer.cpp$(PreprocessSuffix): tests/UnitTests/TestTransfersContainer.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_UnitTests_TestTransfersContainer.cpp$(PreprocessSuffix) tests/UnitTests/TestTransfersContainer.cpp

$(IntermediateDirectory)/tests_UnitTests_TestPath.cpp$(ObjectSuffix): tests/UnitTests/TestPath.cpp $(IntermediateDirectory)/tests_UnitTests_TestPath.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/UnitTests/TestPath.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_UnitTests_TestPath.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_UnitTests_TestPath.cpp$(DependSuffix): tests/UnitTests/TestPath.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_UnitTests_TestPath.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_UnitTests_TestPath.cpp$(DependSuffix) -MM tests/UnitTests/TestPath.cpp

$(IntermediateDirectory)/tests_UnitTests_TestPath.cpp$(PreprocessSuffix): tests/UnitTests/TestPath.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_UnitTests_TestPath.cpp$(PreprocessSuffix) tests/UnitTests/TestPath.cpp

$(IntermediateDirectory)/tests_UnitTests_BinarySerializationCompatibility.cpp$(ObjectSuffix): tests/UnitTests/BinarySerializationCompatibility.cpp $(IntermediateDirectory)/tests_UnitTests_BinarySerializationCompatibility.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/UnitTests/BinarySerializationCompatibility.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_UnitTests_BinarySerializationCompatibility.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_UnitTests_BinarySerializationCompatibility.cpp$(DependSuffix): tests/UnitTests/BinarySerializationCompatibility.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_UnitTests_BinarySerializationCompatibility.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_UnitTests_BinarySerializationCompatibility.cpp$(DependSuffix) -MM tests/UnitTests/BinarySerializationCompatibility.cpp

$(IntermediateDirectory)/tests_UnitTests_BinarySerializationCompatibility.cpp$(PreprocessSuffix): tests/UnitTests/BinarySerializationCompatibility.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_UnitTests_BinarySerializationCompatibility.cpp$(PreprocessSuffix) tests/UnitTests/BinarySerializationCompatibility.cpp

$(IntermediateDirectory)/tests_UnitTests_TransactionApiHelpers.cpp$(ObjectSuffix): tests/UnitTests/TransactionApiHelpers.cpp $(IntermediateDirectory)/tests_UnitTests_TransactionApiHelpers.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/UnitTests/TransactionApiHelpers.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_UnitTests_TransactionApiHelpers.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_UnitTests_TransactionApiHelpers.cpp$(DependSuffix): tests/UnitTests/TransactionApiHelpers.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_UnitTests_TransactionApiHelpers.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_UnitTests_TransactionApiHelpers.cpp$(DependSuffix) -MM tests/UnitTests/TransactionApiHelpers.cpp

$(IntermediateDirectory)/tests_UnitTests_TransactionApiHelpers.cpp$(PreprocessSuffix): tests/UnitTests/TransactionApiHelpers.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_UnitTests_TransactionApiHelpers.cpp$(PreprocessSuffix) tests/UnitTests/TransactionApiHelpers.cpp

$(IntermediateDirectory)/tests_UnitTests_TestWalletService.cpp$(ObjectSuffix): tests/UnitTests/TestWalletService.cpp $(IntermediateDirectory)/tests_UnitTests_TestWalletService.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/UnitTests/TestWalletService.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_UnitTests_TestWalletService.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_UnitTests_TestWalletService.cpp$(DependSuffix): tests/UnitTests/TestWalletService.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_UnitTests_TestWalletService.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_UnitTests_TestWalletService.cpp$(DependSuffix) -MM tests/UnitTests/TestWalletService.cpp

$(IntermediateDirectory)/tests_UnitTests_TestWalletService.cpp$(PreprocessSuffix): tests/UnitTests/TestWalletService.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_UnitTests_TestWalletService.cpp$(PreprocessSuffix) tests/UnitTests/TestWalletService.cpp

$(IntermediateDirectory)/tests_UnitTests_TestPeerlist.cpp$(ObjectSuffix): tests/UnitTests/TestPeerlist.cpp $(IntermediateDirectory)/tests_UnitTests_TestPeerlist.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/UnitTests/TestPeerlist.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_UnitTests_TestPeerlist.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_UnitTests_TestPeerlist.cpp$(DependSuffix): tests/UnitTests/TestPeerlist.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_UnitTests_TestPeerlist.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_UnitTests_TestPeerlist.cpp$(DependSuffix) -MM tests/UnitTests/TestPeerlist.cpp

$(IntermediateDirectory)/tests_UnitTests_TestPeerlist.cpp$(PreprocessSuffix): tests/UnitTests/TestPeerlist.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_UnitTests_TestPeerlist.cpp$(PreprocessSuffix) tests/UnitTests/TestPeerlist.cpp

$(IntermediateDirectory)/tests_UnitTests_TestFormatUtils.cpp$(ObjectSuffix): tests/UnitTests/TestFormatUtils.cpp $(IntermediateDirectory)/tests_UnitTests_TestFormatUtils.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/UnitTests/TestFormatUtils.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_UnitTests_TestFormatUtils.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_UnitTests_TestFormatUtils.cpp$(DependSuffix): tests/UnitTests/TestFormatUtils.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_UnitTests_TestFormatUtils.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_UnitTests_TestFormatUtils.cpp$(DependSuffix) -MM tests/UnitTests/TestFormatUtils.cpp

$(IntermediateDirectory)/tests_UnitTests_TestFormatUtils.cpp$(PreprocessSuffix): tests/UnitTests/TestFormatUtils.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_UnitTests_TestFormatUtils.cpp$(PreprocessSuffix) tests/UnitTests/TestFormatUtils.cpp

$(IntermediateDirectory)/tests_UnitTests_EventWaiter.cpp$(ObjectSuffix): tests/UnitTests/EventWaiter.cpp $(IntermediateDirectory)/tests_UnitTests_EventWaiter.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/UnitTests/EventWaiter.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_UnitTests_EventWaiter.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_UnitTests_EventWaiter.cpp$(DependSuffix): tests/UnitTests/EventWaiter.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_UnitTests_EventWaiter.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_UnitTests_EventWaiter.cpp$(DependSuffix) -MM tests/UnitTests/EventWaiter.cpp

$(IntermediateDirectory)/tests_UnitTests_EventWaiter.cpp$(PreprocessSuffix): tests/UnitTests/EventWaiter.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_UnitTests_EventWaiter.cpp$(PreprocessSuffix) tests/UnitTests/EventWaiter.cpp

$(IntermediateDirectory)/tests_UnitTests_MulDiv.cpp$(ObjectSuffix): tests/UnitTests/MulDiv.cpp $(IntermediateDirectory)/tests_UnitTests_MulDiv.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/UnitTests/MulDiv.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_UnitTests_MulDiv.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_UnitTests_MulDiv.cpp$(DependSuffix): tests/UnitTests/MulDiv.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_UnitTests_MulDiv.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_UnitTests_MulDiv.cpp$(DependSuffix) -MM tests/UnitTests/MulDiv.cpp

$(IntermediateDirectory)/tests_UnitTests_MulDiv.cpp$(PreprocessSuffix): tests/UnitTests/MulDiv.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_UnitTests_MulDiv.cpp$(PreprocessSuffix) tests/UnitTests/MulDiv.cpp

$(IntermediateDirectory)/tests_UnitTests_ArrayViewTests.cpp$(ObjectSuffix): tests/UnitTests/ArrayViewTests.cpp $(IntermediateDirectory)/tests_UnitTests_ArrayViewTests.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/UnitTests/ArrayViewTests.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_UnitTests_ArrayViewTests.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_UnitTests_ArrayViewTests.cpp$(DependSuffix): tests/UnitTests/ArrayViewTests.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_UnitTests_ArrayViewTests.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_UnitTests_ArrayViewTests.cpp$(DependSuffix) -MM tests/UnitTests/ArrayViewTests.cpp

$(IntermediateDirectory)/tests_UnitTests_ArrayViewTests.cpp$(PreprocessSuffix): tests/UnitTests/ArrayViewTests.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_UnitTests_ArrayViewTests.cpp$(PreprocessSuffix) tests/UnitTests/ArrayViewTests.cpp

$(IntermediateDirectory)/tests_UnitTests_TestTransfersConsumer.cpp$(ObjectSuffix): tests/UnitTests/TestTransfersConsumer.cpp $(IntermediateDirectory)/tests_UnitTests_TestTransfersConsumer.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/UnitTests/TestTransfersConsumer.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_UnitTests_TestTransfersConsumer.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_UnitTests_TestTransfersConsumer.cpp$(DependSuffix): tests/UnitTests/TestTransfersConsumer.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_UnitTests_TestTransfersConsumer.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_UnitTests_TestTransfersConsumer.cpp$(DependSuffix) -MM tests/UnitTests/TestTransfersConsumer.cpp

$(IntermediateDirectory)/tests_UnitTests_TestTransfersConsumer.cpp$(PreprocessSuffix): tests/UnitTests/TestTransfersConsumer.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_UnitTests_TestTransfersConsumer.cpp$(PreprocessSuffix) tests/UnitTests/TestTransfersConsumer.cpp

$(IntermediateDirectory)/tests_UnitTests_Base58.cpp$(ObjectSuffix): tests/UnitTests/Base58.cpp $(IntermediateDirectory)/tests_UnitTests_Base58.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/UnitTests/Base58.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_UnitTests_Base58.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_UnitTests_Base58.cpp$(DependSuffix): tests/UnitTests/Base58.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_UnitTests_Base58.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_UnitTests_Base58.cpp$(DependSuffix) -MM tests/UnitTests/Base58.cpp

$(IntermediateDirectory)/tests_UnitTests_Base58.cpp$(PreprocessSuffix): tests/UnitTests/Base58.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_UnitTests_Base58.cpp$(PreprocessSuffix) tests/UnitTests/Base58.cpp

$(IntermediateDirectory)/tests_UnitTests_TestMessageQueue.cpp$(ObjectSuffix): tests/UnitTests/TestMessageQueue.cpp $(IntermediateDirectory)/tests_UnitTests_TestMessageQueue.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/UnitTests/TestMessageQueue.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_UnitTests_TestMessageQueue.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_UnitTests_TestMessageQueue.cpp$(DependSuffix): tests/UnitTests/TestMessageQueue.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_UnitTests_TestMessageQueue.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_UnitTests_TestMessageQueue.cpp$(DependSuffix) -MM tests/UnitTests/TestMessageQueue.cpp

$(IntermediateDirectory)/tests_UnitTests_TestMessageQueue.cpp$(PreprocessSuffix): tests/UnitTests/TestMessageQueue.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_UnitTests_TestMessageQueue.cpp$(PreprocessSuffix) tests/UnitTests/TestMessageQueue.cpp

$(IntermediateDirectory)/tests_UnitTests_TestTransfersContainerKeyImage.cpp$(ObjectSuffix): tests/UnitTests/TestTransfersContainerKeyImage.cpp $(IntermediateDirectory)/tests_UnitTests_TestTransfersContainerKeyImage.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/UnitTests/TestTransfersContainerKeyImage.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_UnitTests_TestTransfersContainerKeyImage.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_UnitTests_TestTransfersContainerKeyImage.cpp$(DependSuffix): tests/UnitTests/TestTransfersContainerKeyImage.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_UnitTests_TestTransfersContainerKeyImage.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_UnitTests_TestTransfersContainerKeyImage.cpp$(DependSuffix) -MM tests/UnitTests/TestTransfersContainerKeyImage.cpp

$(IntermediateDirectory)/tests_UnitTests_TestTransfersContainerKeyImage.cpp$(PreprocessSuffix): tests/UnitTests/TestTransfersContainerKeyImage.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_UnitTests_TestTransfersContainerKeyImage.cpp$(PreprocessSuffix) tests/UnitTests/TestTransfersContainerKeyImage.cpp

$(IntermediateDirectory)/tests_UnitTests_TestBlockchainGenerator.cpp$(ObjectSuffix): tests/UnitTests/TestBlockchainGenerator.cpp $(IntermediateDirectory)/tests_UnitTests_TestBlockchainGenerator.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/UnitTests/TestBlockchainGenerator.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_UnitTests_TestBlockchainGenerator.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_UnitTests_TestBlockchainGenerator.cpp$(DependSuffix): tests/UnitTests/TestBlockchainGenerator.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_UnitTests_TestBlockchainGenerator.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_UnitTests_TestBlockchainGenerator.cpp$(DependSuffix) -MM tests/UnitTests/TestBlockchainGenerator.cpp

$(IntermediateDirectory)/tests_UnitTests_TestBlockchainGenerator.cpp$(PreprocessSuffix): tests/UnitTests/TestBlockchainGenerator.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_UnitTests_TestBlockchainGenerator.cpp$(PreprocessSuffix) tests/UnitTests/TestBlockchainGenerator.cpp

$(IntermediateDirectory)/tests_UnitTests_TestUpgradeDetector.cpp$(ObjectSuffix): tests/UnitTests/TestUpgradeDetector.cpp $(IntermediateDirectory)/tests_UnitTests_TestUpgradeDetector.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/UnitTests/TestUpgradeDetector.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_UnitTests_TestUpgradeDetector.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_UnitTests_TestUpgradeDetector.cpp$(DependSuffix): tests/UnitTests/TestUpgradeDetector.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_UnitTests_TestUpgradeDetector.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_UnitTests_TestUpgradeDetector.cpp$(DependSuffix) -MM tests/UnitTests/TestUpgradeDetector.cpp

$(IntermediateDirectory)/tests_UnitTests_TestUpgradeDetector.cpp$(PreprocessSuffix): tests/UnitTests/TestUpgradeDetector.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_UnitTests_TestUpgradeDetector.cpp$(PreprocessSuffix) tests/UnitTests/TestUpgradeDetector.cpp

$(IntermediateDirectory)/tests_UnitTests_Shuffle.cpp$(ObjectSuffix): tests/UnitTests/Shuffle.cpp $(IntermediateDirectory)/tests_UnitTests_Shuffle.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/UnitTests/Shuffle.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_UnitTests_Shuffle.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_UnitTests_Shuffle.cpp$(DependSuffix): tests/UnitTests/Shuffle.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_UnitTests_Shuffle.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_UnitTests_Shuffle.cpp$(DependSuffix) -MM tests/UnitTests/Shuffle.cpp

$(IntermediateDirectory)/tests_UnitTests_Shuffle.cpp$(PreprocessSuffix): tests/UnitTests/Shuffle.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_UnitTests_Shuffle.cpp$(PreprocessSuffix) tests/UnitTests/Shuffle.cpp

$(IntermediateDirectory)/tests_UnitTests_TestProtocolPack.cpp$(ObjectSuffix): tests/UnitTests/TestProtocolPack.cpp $(IntermediateDirectory)/tests_UnitTests_TestProtocolPack.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/UnitTests/TestProtocolPack.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_UnitTests_TestProtocolPack.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_UnitTests_TestProtocolPack.cpp$(DependSuffix): tests/UnitTests/TestProtocolPack.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_UnitTests_TestProtocolPack.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_UnitTests_TestProtocolPack.cpp$(DependSuffix) -MM tests/UnitTests/TestProtocolPack.cpp

$(IntermediateDirectory)/tests_UnitTests_TestProtocolPack.cpp$(PreprocessSuffix): tests/UnitTests/TestProtocolPack.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_UnitTests_TestProtocolPack.cpp$(PreprocessSuffix) tests/UnitTests/TestProtocolPack.cpp

$(IntermediateDirectory)/tests_UnitTests_INodeStubs.cpp$(ObjectSuffix): tests/UnitTests/INodeStubs.cpp $(IntermediateDirectory)/tests_UnitTests_INodeStubs.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/UnitTests/INodeStubs.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_UnitTests_INodeStubs.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_UnitTests_INodeStubs.cpp$(DependSuffix): tests/UnitTests/INodeStubs.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_UnitTests_INodeStubs.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_UnitTests_INodeStubs.cpp$(DependSuffix) -MM tests/UnitTests/INodeStubs.cpp

$(IntermediateDirectory)/tests_UnitTests_INodeStubs.cpp$(PreprocessSuffix): tests/UnitTests/INodeStubs.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_UnitTests_INodeStubs.cpp$(PreprocessSuffix) tests/UnitTests/INodeStubs.cpp

$(IntermediateDirectory)/tests_UnitTests_StringViewTests.cpp$(ObjectSuffix): tests/UnitTests/StringViewTests.cpp $(IntermediateDirectory)/tests_UnitTests_StringViewTests.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/UnitTests/StringViewTests.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_UnitTests_StringViewTests.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_UnitTests_StringViewTests.cpp$(DependSuffix): tests/UnitTests/StringViewTests.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_UnitTests_StringViewTests.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_UnitTests_StringViewTests.cpp$(DependSuffix) -MM tests/UnitTests/StringViewTests.cpp

$(IntermediateDirectory)/tests_UnitTests_StringViewTests.cpp$(PreprocessSuffix): tests/UnitTests/StringViewTests.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_UnitTests_StringViewTests.cpp$(PreprocessSuffix) tests/UnitTests/StringViewTests.cpp

$(IntermediateDirectory)/tests_UnitTests_DecomposeAmountIntoDigits.cpp$(ObjectSuffix): tests/UnitTests/DecomposeAmountIntoDigits.cpp $(IntermediateDirectory)/tests_UnitTests_DecomposeAmountIntoDigits.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/UnitTests/DecomposeAmountIntoDigits.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_UnitTests_DecomposeAmountIntoDigits.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_UnitTests_DecomposeAmountIntoDigits.cpp$(DependSuffix): tests/UnitTests/DecomposeAmountIntoDigits.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_UnitTests_DecomposeAmountIntoDigits.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_UnitTests_DecomposeAmountIntoDigits.cpp$(DependSuffix) -MM tests/UnitTests/DecomposeAmountIntoDigits.cpp

$(IntermediateDirectory)/tests_UnitTests_DecomposeAmountIntoDigits.cpp$(PreprocessSuffix): tests/UnitTests/DecomposeAmountIntoDigits.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_UnitTests_DecomposeAmountIntoDigits.cpp$(PreprocessSuffix) tests/UnitTests/DecomposeAmountIntoDigits.cpp

$(IntermediateDirectory)/tests_UnitTests_TestTransfersSubscription.cpp$(ObjectSuffix): tests/UnitTests/TestTransfersSubscription.cpp $(IntermediateDirectory)/tests_UnitTests_TestTransfersSubscription.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/UnitTests/TestTransfersSubscription.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_UnitTests_TestTransfersSubscription.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_UnitTests_TestTransfersSubscription.cpp$(DependSuffix): tests/UnitTests/TestTransfersSubscription.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_UnitTests_TestTransfersSubscription.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_UnitTests_TestTransfersSubscription.cpp$(DependSuffix) -MM tests/UnitTests/TestTransfersSubscription.cpp

$(IntermediateDirectory)/tests_UnitTests_TestTransfersSubscription.cpp$(PreprocessSuffix): tests/UnitTests/TestTransfersSubscription.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_UnitTests_TestTransfersSubscription.cpp$(PreprocessSuffix) tests/UnitTests/TestTransfersSubscription.cpp

$(IntermediateDirectory)/tests_UnitTests_TestBlockchainExplorer.cpp$(ObjectSuffix): tests/UnitTests/TestBlockchainExplorer.cpp $(IntermediateDirectory)/tests_UnitTests_TestBlockchainExplorer.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/UnitTests/TestBlockchainExplorer.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_UnitTests_TestBlockchainExplorer.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_UnitTests_TestBlockchainExplorer.cpp$(DependSuffix): tests/UnitTests/TestBlockchainExplorer.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_UnitTests_TestBlockchainExplorer.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_UnitTests_TestBlockchainExplorer.cpp$(DependSuffix) -MM tests/UnitTests/TestBlockchainExplorer.cpp

$(IntermediateDirectory)/tests_UnitTests_TestBlockchainExplorer.cpp$(PreprocessSuffix): tests/UnitTests/TestBlockchainExplorer.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_UnitTests_TestBlockchainExplorer.cpp$(PreprocessSuffix) tests/UnitTests/TestBlockchainExplorer.cpp

$(IntermediateDirectory)/tests_UnitTests_TestWallet.cpp$(ObjectSuffix): tests/UnitTests/TestWallet.cpp $(IntermediateDirectory)/tests_UnitTests_TestWallet.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/UnitTests/TestWallet.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_UnitTests_TestWallet.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_UnitTests_TestWallet.cpp$(DependSuffix): tests/UnitTests/TestWallet.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_UnitTests_TestWallet.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_UnitTests_TestWallet.cpp$(DependSuffix) -MM tests/UnitTests/TestWallet.cpp

$(IntermediateDirectory)/tests_UnitTests_TestWallet.cpp$(PreprocessSuffix): tests/UnitTests/TestWallet.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_UnitTests_TestWallet.cpp$(PreprocessSuffix) tests/UnitTests/TestWallet.cpp

$(IntermediateDirectory)/tests_UnitTests_TestTransfers.cpp$(ObjectSuffix): tests/UnitTests/TestTransfers.cpp $(IntermediateDirectory)/tests_UnitTests_TestTransfers.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/UnitTests/TestTransfers.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_UnitTests_TestTransfers.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_UnitTests_TestTransfers.cpp$(DependSuffix): tests/UnitTests/TestTransfers.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_UnitTests_TestTransfers.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_UnitTests_TestTransfers.cpp$(DependSuffix) -MM tests/UnitTests/TestTransfers.cpp

$(IntermediateDirectory)/tests_UnitTests_TestTransfers.cpp$(PreprocessSuffix): tests/UnitTests/TestTransfers.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_UnitTests_TestTransfers.cpp$(PreprocessSuffix) tests/UnitTests/TestTransfers.cpp

$(IntermediateDirectory)/tests_UnitTests_TransactionPool.cpp$(ObjectSuffix): tests/UnitTests/TransactionPool.cpp $(IntermediateDirectory)/tests_UnitTests_TransactionPool.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/UnitTests/TransactionPool.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_UnitTests_TransactionPool.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_UnitTests_TransactionPool.cpp$(DependSuffix): tests/UnitTests/TransactionPool.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_UnitTests_TransactionPool.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_UnitTests_TransactionPool.cpp$(DependSuffix) -MM tests/UnitTests/TransactionPool.cpp

$(IntermediateDirectory)/tests_UnitTests_TransactionPool.cpp$(PreprocessSuffix): tests/UnitTests/TransactionPool.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_UnitTests_TransactionPool.cpp$(PreprocessSuffix) tests/UnitTests/TransactionPool.cpp

$(IntermediateDirectory)/tests_UnitTests_TestFileMappedVector.cpp$(ObjectSuffix): tests/UnitTests/TestFileMappedVector.cpp $(IntermediateDirectory)/tests_UnitTests_TestFileMappedVector.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/UnitTests/TestFileMappedVector.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_UnitTests_TestFileMappedVector.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_UnitTests_TestFileMappedVector.cpp$(DependSuffix): tests/UnitTests/TestFileMappedVector.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_UnitTests_TestFileMappedVector.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_UnitTests_TestFileMappedVector.cpp$(DependSuffix) -MM tests/UnitTests/TestFileMappedVector.cpp

$(IntermediateDirectory)/tests_UnitTests_TestFileMappedVector.cpp$(PreprocessSuffix): tests/UnitTests/TestFileMappedVector.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_UnitTests_TestFileMappedVector.cpp$(PreprocessSuffix) tests/UnitTests/TestFileMappedVector.cpp

$(IntermediateDirectory)/tests_UnitTests_TestInprocessNode.cpp$(ObjectSuffix): tests/UnitTests/TestInprocessNode.cpp $(IntermediateDirectory)/tests_UnitTests_TestInprocessNode.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/UnitTests/TestInprocessNode.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_UnitTests_TestInprocessNode.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_UnitTests_TestInprocessNode.cpp$(DependSuffix): tests/UnitTests/TestInprocessNode.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_UnitTests_TestInprocessNode.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_UnitTests_TestInprocessNode.cpp$(DependSuffix) -MM tests/UnitTests/TestInprocessNode.cpp

$(IntermediateDirectory)/tests_UnitTests_TestInprocessNode.cpp$(PreprocessSuffix): tests/UnitTests/TestInprocessNode.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_UnitTests_TestInprocessNode.cpp$(PreprocessSuffix) tests/UnitTests/TestInprocessNode.cpp

$(IntermediateDirectory)/tests_UnitTests_ICryptoNoteProtocolQueryStub.cpp$(ObjectSuffix): tests/UnitTests/ICryptoNoteProtocolQueryStub.cpp $(IntermediateDirectory)/tests_UnitTests_ICryptoNoteProtocolQueryStub.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/UnitTests/ICryptoNoteProtocolQueryStub.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_UnitTests_ICryptoNoteProtocolQueryStub.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_UnitTests_ICryptoNoteProtocolQueryStub.cpp$(DependSuffix): tests/UnitTests/ICryptoNoteProtocolQueryStub.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_UnitTests_ICryptoNoteProtocolQueryStub.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_UnitTests_ICryptoNoteProtocolQueryStub.cpp$(DependSuffix) -MM tests/UnitTests/ICryptoNoteProtocolQueryStub.cpp

$(IntermediateDirectory)/tests_UnitTests_ICryptoNoteProtocolQueryStub.cpp$(PreprocessSuffix): tests/UnitTests/ICryptoNoteProtocolQueryStub.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_UnitTests_ICryptoNoteProtocolQueryStub.cpp$(PreprocessSuffix) tests/UnitTests/ICryptoNoteProtocolQueryStub.cpp

$(IntermediateDirectory)/tests_UnitTests_TestTransactionPoolDetach.cpp$(ObjectSuffix): tests/UnitTests/TestTransactionPoolDetach.cpp $(IntermediateDirectory)/tests_UnitTests_TestTransactionPoolDetach.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/UnitTests/TestTransactionPoolDetach.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_UnitTests_TestTransactionPoolDetach.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_UnitTests_TestTransactionPoolDetach.cpp$(DependSuffix): tests/UnitTests/TestTransactionPoolDetach.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_UnitTests_TestTransactionPoolDetach.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_UnitTests_TestTransactionPoolDetach.cpp$(DependSuffix) -MM tests/UnitTests/TestTransactionPoolDetach.cpp

$(IntermediateDirectory)/tests_UnitTests_TestTransactionPoolDetach.cpp$(PreprocessSuffix): tests/UnitTests/TestTransactionPoolDetach.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_UnitTests_TestTransactionPoolDetach.cpp$(PreprocessSuffix) tests/UnitTests/TestTransactionPoolDetach.cpp

$(IntermediateDirectory)/tests_UnitTests_TransactionApi.cpp$(ObjectSuffix): tests/UnitTests/TransactionApi.cpp $(IntermediateDirectory)/tests_UnitTests_TransactionApi.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/UnitTests/TransactionApi.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_UnitTests_TransactionApi.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_UnitTests_TransactionApi.cpp$(DependSuffix): tests/UnitTests/TransactionApi.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_UnitTests_TransactionApi.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_UnitTests_TransactionApi.cpp$(DependSuffix) -MM tests/UnitTests/TransactionApi.cpp

$(IntermediateDirectory)/tests_UnitTests_TransactionApi.cpp$(PreprocessSuffix): tests/UnitTests/TransactionApi.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_UnitTests_TransactionApi.cpp$(PreprocessSuffix) tests/UnitTests/TransactionApi.cpp

$(IntermediateDirectory)/tests_UnitTests_TestBcS.cpp$(ObjectSuffix): tests/UnitTests/TestBcS.cpp $(IntermediateDirectory)/tests_UnitTests_TestBcS.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/UnitTests/TestBcS.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_UnitTests_TestBcS.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_UnitTests_TestBcS.cpp$(DependSuffix): tests/UnitTests/TestBcS.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_UnitTests_TestBcS.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_UnitTests_TestBcS.cpp$(DependSuffix) -MM tests/UnitTests/TestBcS.cpp

$(IntermediateDirectory)/tests_UnitTests_TestBcS.cpp$(PreprocessSuffix): tests/UnitTests/TestBcS.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_UnitTests_TestBcS.cpp$(PreprocessSuffix) tests/UnitTests/TestBcS.cpp

$(IntermediateDirectory)/tests_UnitTests_TestWalletLegacy.cpp$(ObjectSuffix): tests/UnitTests/TestWalletLegacy.cpp $(IntermediateDirectory)/tests_UnitTests_TestWalletLegacy.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/UnitTests/TestWalletLegacy.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_UnitTests_TestWalletLegacy.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_UnitTests_TestWalletLegacy.cpp$(DependSuffix): tests/UnitTests/TestWalletLegacy.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_UnitTests_TestWalletLegacy.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_UnitTests_TestWalletLegacy.cpp$(DependSuffix) -MM tests/UnitTests/TestWalletLegacy.cpp

$(IntermediateDirectory)/tests_UnitTests_TestWalletLegacy.cpp$(PreprocessSuffix): tests/UnitTests/TestWalletLegacy.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_UnitTests_TestWalletLegacy.cpp$(PreprocessSuffix) tests/UnitTests/TestWalletLegacy.cpp

$(IntermediateDirectory)/tests_UnitTests_BlockReward.cpp$(ObjectSuffix): tests/UnitTests/BlockReward.cpp $(IntermediateDirectory)/tests_UnitTests_BlockReward.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/UnitTests/BlockReward.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_UnitTests_BlockReward.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_UnitTests_BlockReward.cpp$(DependSuffix): tests/UnitTests/BlockReward.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_UnitTests_BlockReward.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_UnitTests_BlockReward.cpp$(DependSuffix) -MM tests/UnitTests/BlockReward.cpp

$(IntermediateDirectory)/tests_UnitTests_BlockReward.cpp$(PreprocessSuffix): tests/UnitTests/BlockReward.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_UnitTests_BlockReward.cpp$(PreprocessSuffix) tests/UnitTests/BlockReward.cpp

$(IntermediateDirectory)/tests_UnitTests_ICoreStub.cpp$(ObjectSuffix): tests/UnitTests/ICoreStub.cpp $(IntermediateDirectory)/tests_UnitTests_ICoreStub.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/UnitTests/ICoreStub.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_UnitTests_ICoreStub.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_UnitTests_ICoreStub.cpp$(DependSuffix): tests/UnitTests/ICoreStub.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_UnitTests_ICoreStub.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_UnitTests_ICoreStub.cpp$(DependSuffix) -MM tests/UnitTests/ICoreStub.cpp

$(IntermediateDirectory)/tests_UnitTests_ICoreStub.cpp$(PreprocessSuffix): tests/UnitTests/ICoreStub.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_UnitTests_ICoreStub.cpp$(PreprocessSuffix) tests/UnitTests/ICoreStub.cpp

$(IntermediateDirectory)/tests_UnitTests_BlockingQueue.cpp$(ObjectSuffix): tests/UnitTests/BlockingQueue.cpp $(IntermediateDirectory)/tests_UnitTests_BlockingQueue.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/UnitTests/BlockingQueue.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_UnitTests_BlockingQueue.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_UnitTests_BlockingQueue.cpp$(DependSuffix): tests/UnitTests/BlockingQueue.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_UnitTests_BlockingQueue.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_UnitTests_BlockingQueue.cpp$(DependSuffix) -MM tests/UnitTests/BlockingQueue.cpp

$(IntermediateDirectory)/tests_UnitTests_BlockingQueue.cpp$(PreprocessSuffix): tests/UnitTests/BlockingQueue.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_UnitTests_BlockingQueue.cpp$(PreprocessSuffix) tests/UnitTests/BlockingQueue.cpp

$(IntermediateDirectory)/tests_UnitTests_Chacha8.cpp$(ObjectSuffix): tests/UnitTests/Chacha8.cpp $(IntermediateDirectory)/tests_UnitTests_Chacha8.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/UnitTests/Chacha8.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_UnitTests_Chacha8.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_UnitTests_Chacha8.cpp$(DependSuffix): tests/UnitTests/Chacha8.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_UnitTests_Chacha8.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_UnitTests_Chacha8.cpp$(DependSuffix) -MM tests/UnitTests/Chacha8.cpp

$(IntermediateDirectory)/tests_UnitTests_Chacha8.cpp$(PreprocessSuffix): tests/UnitTests/Chacha8.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_UnitTests_Chacha8.cpp$(PreprocessSuffix) tests/UnitTests/Chacha8.cpp

$(IntermediateDirectory)/tests_UnitTests_main.cpp$(ObjectSuffix): tests/UnitTests/main.cpp $(IntermediateDirectory)/tests_UnitTests_main.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/UnitTests/main.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_UnitTests_main.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_UnitTests_main.cpp$(DependSuffix): tests/UnitTests/main.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_UnitTests_main.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_UnitTests_main.cpp$(DependSuffix) -MM tests/UnitTests/main.cpp

$(IntermediateDirectory)/tests_UnitTests_main.cpp$(PreprocessSuffix): tests/UnitTests/main.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_UnitTests_main.cpp$(PreprocessSuffix) tests/UnitTests/main.cpp

$(IntermediateDirectory)/tests_UnitTests_StringBufferTests.cpp$(ObjectSuffix): tests/UnitTests/StringBufferTests.cpp $(IntermediateDirectory)/tests_UnitTests_StringBufferTests.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/UnitTests/StringBufferTests.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_UnitTests_StringBufferTests.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_UnitTests_StringBufferTests.cpp$(DependSuffix): tests/UnitTests/StringBufferTests.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_UnitTests_StringBufferTests.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_UnitTests_StringBufferTests.cpp$(DependSuffix) -MM tests/UnitTests/StringBufferTests.cpp

$(IntermediateDirectory)/tests_UnitTests_StringBufferTests.cpp$(PreprocessSuffix): tests/UnitTests/StringBufferTests.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_UnitTests_StringBufferTests.cpp$(PreprocessSuffix) tests/UnitTests/StringBufferTests.cpp

$(IntermediateDirectory)/tests_UnitTests_Serialization.cpp$(ObjectSuffix): tests/UnitTests/Serialization.cpp $(IntermediateDirectory)/tests_UnitTests_Serialization.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/UnitTests/Serialization.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_UnitTests_Serialization.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_UnitTests_Serialization.cpp$(DependSuffix): tests/UnitTests/Serialization.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_UnitTests_Serialization.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_UnitTests_Serialization.cpp$(DependSuffix) -MM tests/UnitTests/Serialization.cpp

$(IntermediateDirectory)/tests_UnitTests_Serialization.cpp$(PreprocessSuffix): tests/UnitTests/Serialization.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_UnitTests_Serialization.cpp$(PreprocessSuffix) tests/UnitTests/Serialization.cpp

$(IntermediateDirectory)/tests_UnitTests_TestCurrency.cpp$(ObjectSuffix): tests/UnitTests/TestCurrency.cpp $(IntermediateDirectory)/tests_UnitTests_TestCurrency.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/UnitTests/TestCurrency.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_UnitTests_TestCurrency.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_UnitTests_TestCurrency.cpp$(DependSuffix): tests/UnitTests/TestCurrency.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_UnitTests_TestCurrency.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_UnitTests_TestCurrency.cpp$(DependSuffix) -MM tests/UnitTests/TestCurrency.cpp

$(IntermediateDirectory)/tests_UnitTests_TestCurrency.cpp$(PreprocessSuffix): tests/UnitTests/TestCurrency.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_UnitTests_TestCurrency.cpp$(PreprocessSuffix) tests/UnitTests/TestCurrency.cpp

$(IntermediateDirectory)/tests_UnitTests_ParseAmount.cpp$(ObjectSuffix): tests/UnitTests/ParseAmount.cpp $(IntermediateDirectory)/tests_UnitTests_ParseAmount.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/UnitTests/ParseAmount.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_UnitTests_ParseAmount.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_UnitTests_ParseAmount.cpp$(DependSuffix): tests/UnitTests/ParseAmount.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_UnitTests_ParseAmount.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_UnitTests_ParseAmount.cpp$(DependSuffix) -MM tests/UnitTests/ParseAmount.cpp

$(IntermediateDirectory)/tests_UnitTests_ParseAmount.cpp$(PreprocessSuffix): tests/UnitTests/ParseAmount.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_UnitTests_ParseAmount.cpp$(PreprocessSuffix) tests/UnitTests/ParseAmount.cpp

$(IntermediateDirectory)/tests_UnitTests_ArrayRefTests.cpp$(ObjectSuffix): tests/UnitTests/ArrayRefTests.cpp $(IntermediateDirectory)/tests_UnitTests_ArrayRefTests.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/UnitTests/ArrayRefTests.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_UnitTests_ArrayRefTests.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_UnitTests_ArrayRefTests.cpp$(DependSuffix): tests/UnitTests/ArrayRefTests.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_UnitTests_ArrayRefTests.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_UnitTests_ArrayRefTests.cpp$(DependSuffix) -MM tests/UnitTests/ArrayRefTests.cpp

$(IntermediateDirectory)/tests_UnitTests_ArrayRefTests.cpp$(PreprocessSuffix): tests/UnitTests/ArrayRefTests.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_UnitTests_ArrayRefTests.cpp$(PreprocessSuffix) tests/UnitTests/ArrayRefTests.cpp

$(IntermediateDirectory)/tests_UnitTests_SerializationKV.cpp$(ObjectSuffix): tests/UnitTests/SerializationKV.cpp $(IntermediateDirectory)/tests_UnitTests_SerializationKV.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/UnitTests/SerializationKV.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_UnitTests_SerializationKV.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_UnitTests_SerializationKV.cpp$(DependSuffix): tests/UnitTests/SerializationKV.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_UnitTests_SerializationKV.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_UnitTests_SerializationKV.cpp$(DependSuffix) -MM tests/UnitTests/SerializationKV.cpp

$(IntermediateDirectory)/tests_UnitTests_SerializationKV.cpp$(PreprocessSuffix): tests/UnitTests/SerializationKV.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_UnitTests_SerializationKV.cpp$(PreprocessSuffix) tests/UnitTests/SerializationKV.cpp

$(IntermediateDirectory)/tests_UnitTests_PaymentGateTests.cpp$(ObjectSuffix): tests/UnitTests/PaymentGateTests.cpp $(IntermediateDirectory)/tests_UnitTests_PaymentGateTests.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/UnitTests/PaymentGateTests.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_UnitTests_PaymentGateTests.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_UnitTests_PaymentGateTests.cpp$(DependSuffix): tests/UnitTests/PaymentGateTests.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_UnitTests_PaymentGateTests.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_UnitTests_PaymentGateTests.cpp$(DependSuffix) -MM tests/UnitTests/PaymentGateTests.cpp

$(IntermediateDirectory)/tests_UnitTests_PaymentGateTests.cpp$(PreprocessSuffix): tests/UnitTests/PaymentGateTests.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_UnitTests_PaymentGateTests.cpp$(PreprocessSuffix) tests/UnitTests/PaymentGateTests.cpp

$(IntermediateDirectory)/tests_UnitTests_Checkpoints.cpp$(ObjectSuffix): tests/UnitTests/Checkpoints.cpp $(IntermediateDirectory)/tests_UnitTests_Checkpoints.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/UnitTests/Checkpoints.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_UnitTests_Checkpoints.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_UnitTests_Checkpoints.cpp$(DependSuffix): tests/UnitTests/Checkpoints.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_UnitTests_Checkpoints.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_UnitTests_Checkpoints.cpp$(DependSuffix) -MM tests/UnitTests/Checkpoints.cpp

$(IntermediateDirectory)/tests_UnitTests_Checkpoints.cpp$(PreprocessSuffix): tests/UnitTests/Checkpoints.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_UnitTests_Checkpoints.cpp$(PreprocessSuffix) tests/UnitTests/Checkpoints.cpp

$(IntermediateDirectory)/tests_UnitTests_TestJsonValue.cpp$(ObjectSuffix): tests/UnitTests/TestJsonValue.cpp $(IntermediateDirectory)/tests_UnitTests_TestJsonValue.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/UnitTests/TestJsonValue.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_UnitTests_TestJsonValue.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_UnitTests_TestJsonValue.cpp$(DependSuffix): tests/UnitTests/TestJsonValue.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_UnitTests_TestJsonValue.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_UnitTests_TestJsonValue.cpp$(DependSuffix) -MM tests/UnitTests/TestJsonValue.cpp

$(IntermediateDirectory)/tests_UnitTests_TestJsonValue.cpp$(PreprocessSuffix): tests/UnitTests/TestJsonValue.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_UnitTests_TestJsonValue.cpp$(PreprocessSuffix) tests/UnitTests/TestJsonValue.cpp

$(IntermediateDirectory)/tests_CoreTests_TransactionBuilder.cpp$(ObjectSuffix): tests/CoreTests/TransactionBuilder.cpp $(IntermediateDirectory)/tests_CoreTests_TransactionBuilder.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/CoreTests/TransactionBuilder.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_CoreTests_TransactionBuilder.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_CoreTests_TransactionBuilder.cpp$(DependSuffix): tests/CoreTests/TransactionBuilder.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_CoreTests_TransactionBuilder.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_CoreTests_TransactionBuilder.cpp$(DependSuffix) -MM tests/CoreTests/TransactionBuilder.cpp

$(IntermediateDirectory)/tests_CoreTests_TransactionBuilder.cpp$(PreprocessSuffix): tests/CoreTests/TransactionBuilder.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_CoreTests_TransactionBuilder.cpp$(PreprocessSuffix) tests/CoreTests/TransactionBuilder.cpp

$(IntermediateDirectory)/tests_CoreTests_ChainSplit1.cpp$(ObjectSuffix): tests/CoreTests/ChainSplit1.cpp $(IntermediateDirectory)/tests_CoreTests_ChainSplit1.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/CoreTests/ChainSplit1.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_CoreTests_ChainSplit1.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_CoreTests_ChainSplit1.cpp$(DependSuffix): tests/CoreTests/ChainSplit1.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_CoreTests_ChainSplit1.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_CoreTests_ChainSplit1.cpp$(DependSuffix) -MM tests/CoreTests/ChainSplit1.cpp

$(IntermediateDirectory)/tests_CoreTests_ChainSplit1.cpp$(PreprocessSuffix): tests/CoreTests/ChainSplit1.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_CoreTests_ChainSplit1.cpp$(PreprocessSuffix) tests/CoreTests/ChainSplit1.cpp

$(IntermediateDirectory)/tests_CoreTests_RingSignature.cpp$(ObjectSuffix): tests/CoreTests/RingSignature.cpp $(IntermediateDirectory)/tests_CoreTests_RingSignature.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/CoreTests/RingSignature.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_CoreTests_RingSignature.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_CoreTests_RingSignature.cpp$(DependSuffix): tests/CoreTests/RingSignature.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_CoreTests_RingSignature.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_CoreTests_RingSignature.cpp$(DependSuffix) -MM tests/CoreTests/RingSignature.cpp

$(IntermediateDirectory)/tests_CoreTests_RingSignature.cpp$(PreprocessSuffix): tests/CoreTests/RingSignature.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_CoreTests_RingSignature.cpp$(PreprocessSuffix) tests/CoreTests/RingSignature.cpp

$(IntermediateDirectory)/tests_CoreTests_ChaingenMain.cpp$(ObjectSuffix): tests/CoreTests/ChaingenMain.cpp $(IntermediateDirectory)/tests_CoreTests_ChaingenMain.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/CoreTests/ChaingenMain.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_CoreTests_ChaingenMain.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_CoreTests_ChaingenMain.cpp$(DependSuffix): tests/CoreTests/ChaingenMain.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_CoreTests_ChaingenMain.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_CoreTests_ChaingenMain.cpp$(DependSuffix) -MM tests/CoreTests/ChaingenMain.cpp

$(IntermediateDirectory)/tests_CoreTests_ChaingenMain.cpp$(PreprocessSuffix): tests/CoreTests/ChaingenMain.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_CoreTests_ChaingenMain.cpp$(PreprocessSuffix) tests/CoreTests/ChaingenMain.cpp

$(IntermediateDirectory)/tests_CoreTests_Upgrade.cpp$(ObjectSuffix): tests/CoreTests/Upgrade.cpp $(IntermediateDirectory)/tests_CoreTests_Upgrade.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/CoreTests/Upgrade.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_CoreTests_Upgrade.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_CoreTests_Upgrade.cpp$(DependSuffix): tests/CoreTests/Upgrade.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_CoreTests_Upgrade.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_CoreTests_Upgrade.cpp$(DependSuffix) -MM tests/CoreTests/Upgrade.cpp

$(IntermediateDirectory)/tests_CoreTests_Upgrade.cpp$(PreprocessSuffix): tests/CoreTests/Upgrade.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_CoreTests_Upgrade.cpp$(PreprocessSuffix) tests/CoreTests/Upgrade.cpp

$(IntermediateDirectory)/tests_CoreTests_TransactionValidation.cpp$(ObjectSuffix): tests/CoreTests/TransactionValidation.cpp $(IntermediateDirectory)/tests_CoreTests_TransactionValidation.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/CoreTests/TransactionValidation.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_CoreTests_TransactionValidation.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_CoreTests_TransactionValidation.cpp$(DependSuffix): tests/CoreTests/TransactionValidation.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_CoreTests_TransactionValidation.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_CoreTests_TransactionValidation.cpp$(DependSuffix) -MM tests/CoreTests/TransactionValidation.cpp

$(IntermediateDirectory)/tests_CoreTests_TransactionValidation.cpp$(PreprocessSuffix): tests/CoreTests/TransactionValidation.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_CoreTests_TransactionValidation.cpp$(PreprocessSuffix) tests/CoreTests/TransactionValidation.cpp

$(IntermediateDirectory)/tests_CoreTests_ChainSwitch1.cpp$(ObjectSuffix): tests/CoreTests/ChainSwitch1.cpp $(IntermediateDirectory)/tests_CoreTests_ChainSwitch1.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/CoreTests/ChainSwitch1.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_CoreTests_ChainSwitch1.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_CoreTests_ChainSwitch1.cpp$(DependSuffix): tests/CoreTests/ChainSwitch1.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_CoreTests_ChainSwitch1.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_CoreTests_ChainSwitch1.cpp$(DependSuffix) -MM tests/CoreTests/ChainSwitch1.cpp

$(IntermediateDirectory)/tests_CoreTests_ChainSwitch1.cpp$(PreprocessSuffix): tests/CoreTests/ChainSwitch1.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_CoreTests_ChainSwitch1.cpp$(PreprocessSuffix) tests/CoreTests/ChainSwitch1.cpp

$(IntermediateDirectory)/tests_CoreTests_DoubleSpend.cpp$(ObjectSuffix): tests/CoreTests/DoubleSpend.cpp $(IntermediateDirectory)/tests_CoreTests_DoubleSpend.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/CoreTests/DoubleSpend.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_CoreTests_DoubleSpend.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_CoreTests_DoubleSpend.cpp$(DependSuffix): tests/CoreTests/DoubleSpend.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_CoreTests_DoubleSpend.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_CoreTests_DoubleSpend.cpp$(DependSuffix) -MM tests/CoreTests/DoubleSpend.cpp

$(IntermediateDirectory)/tests_CoreTests_DoubleSpend.cpp$(PreprocessSuffix): tests/CoreTests/DoubleSpend.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_CoreTests_DoubleSpend.cpp$(PreprocessSuffix) tests/CoreTests/DoubleSpend.cpp

$(IntermediateDirectory)/tests_CoreTests_BlockValidation.cpp$(ObjectSuffix): tests/CoreTests/BlockValidation.cpp $(IntermediateDirectory)/tests_CoreTests_BlockValidation.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/CoreTests/BlockValidation.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_CoreTests_BlockValidation.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_CoreTests_BlockValidation.cpp$(DependSuffix): tests/CoreTests/BlockValidation.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_CoreTests_BlockValidation.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_CoreTests_BlockValidation.cpp$(DependSuffix) -MM tests/CoreTests/BlockValidation.cpp

$(IntermediateDirectory)/tests_CoreTests_BlockValidation.cpp$(PreprocessSuffix): tests/CoreTests/BlockValidation.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_CoreTests_BlockValidation.cpp$(PreprocessSuffix) tests/CoreTests/BlockValidation.cpp

$(IntermediateDirectory)/tests_CoreTests_RandomOuts.cpp$(ObjectSuffix): tests/CoreTests/RandomOuts.cpp $(IntermediateDirectory)/tests_CoreTests_RandomOuts.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/CoreTests/RandomOuts.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_CoreTests_RandomOuts.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_CoreTests_RandomOuts.cpp$(DependSuffix): tests/CoreTests/RandomOuts.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_CoreTests_RandomOuts.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_CoreTests_RandomOuts.cpp$(DependSuffix) -MM tests/CoreTests/RandomOuts.cpp

$(IntermediateDirectory)/tests_CoreTests_RandomOuts.cpp$(PreprocessSuffix): tests/CoreTests/RandomOuts.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_CoreTests_RandomOuts.cpp$(PreprocessSuffix) tests/CoreTests/RandomOuts.cpp

$(IntermediateDirectory)/tests_CoreTests_Chaingen001.cpp$(ObjectSuffix): tests/CoreTests/Chaingen001.cpp $(IntermediateDirectory)/tests_CoreTests_Chaingen001.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/CoreTests/Chaingen001.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_CoreTests_Chaingen001.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_CoreTests_Chaingen001.cpp$(DependSuffix): tests/CoreTests/Chaingen001.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_CoreTests_Chaingen001.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_CoreTests_Chaingen001.cpp$(DependSuffix) -MM tests/CoreTests/Chaingen001.cpp

$(IntermediateDirectory)/tests_CoreTests_Chaingen001.cpp$(PreprocessSuffix): tests/CoreTests/Chaingen001.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_CoreTests_Chaingen001.cpp$(PreprocessSuffix) tests/CoreTests/Chaingen001.cpp

$(IntermediateDirectory)/tests_CoreTests_TransactionTests.cpp$(ObjectSuffix): tests/CoreTests/TransactionTests.cpp $(IntermediateDirectory)/tests_CoreTests_TransactionTests.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/CoreTests/TransactionTests.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_CoreTests_TransactionTests.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_CoreTests_TransactionTests.cpp$(DependSuffix): tests/CoreTests/TransactionTests.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_CoreTests_TransactionTests.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_CoreTests_TransactionTests.cpp$(DependSuffix) -MM tests/CoreTests/TransactionTests.cpp

$(IntermediateDirectory)/tests_CoreTests_TransactionTests.cpp$(PreprocessSuffix): tests/CoreTests/TransactionTests.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_CoreTests_TransactionTests.cpp$(PreprocessSuffix) tests/CoreTests/TransactionTests.cpp

$(IntermediateDirectory)/tests_CoreTests_BlockReward.cpp$(ObjectSuffix): tests/CoreTests/BlockReward.cpp $(IntermediateDirectory)/tests_CoreTests_BlockReward.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/CoreTests/BlockReward.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_CoreTests_BlockReward.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_CoreTests_BlockReward.cpp$(DependSuffix): tests/CoreTests/BlockReward.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_CoreTests_BlockReward.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_CoreTests_BlockReward.cpp$(DependSuffix) -MM tests/CoreTests/BlockReward.cpp

$(IntermediateDirectory)/tests_CoreTests_BlockReward.cpp$(PreprocessSuffix): tests/CoreTests/BlockReward.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_CoreTests_BlockReward.cpp$(PreprocessSuffix) tests/CoreTests/BlockReward.cpp

$(IntermediateDirectory)/tests_CoreTests_IntegerOverflow.cpp$(ObjectSuffix): tests/CoreTests/IntegerOverflow.cpp $(IntermediateDirectory)/tests_CoreTests_IntegerOverflow.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/CoreTests/IntegerOverflow.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_CoreTests_IntegerOverflow.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_CoreTests_IntegerOverflow.cpp$(DependSuffix): tests/CoreTests/IntegerOverflow.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_CoreTests_IntegerOverflow.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_CoreTests_IntegerOverflow.cpp$(DependSuffix) -MM tests/CoreTests/IntegerOverflow.cpp

$(IntermediateDirectory)/tests_CoreTests_IntegerOverflow.cpp$(PreprocessSuffix): tests/CoreTests/IntegerOverflow.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_CoreTests_IntegerOverflow.cpp$(PreprocessSuffix) tests/CoreTests/IntegerOverflow.cpp

$(IntermediateDirectory)/tests_CoreTests_Chaingen.cpp$(ObjectSuffix): tests/CoreTests/Chaingen.cpp $(IntermediateDirectory)/tests_CoreTests_Chaingen.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/CoreTests/Chaingen.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_CoreTests_Chaingen.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_CoreTests_Chaingen.cpp$(DependSuffix): tests/CoreTests/Chaingen.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_CoreTests_Chaingen.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_CoreTests_Chaingen.cpp$(DependSuffix) -MM tests/CoreTests/Chaingen.cpp

$(IntermediateDirectory)/tests_CoreTests_Chaingen.cpp$(PreprocessSuffix): tests/CoreTests/Chaingen.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_CoreTests_Chaingen.cpp$(PreprocessSuffix) tests/CoreTests/Chaingen.cpp

$(IntermediateDirectory)/tests_IntegrationTestLib_TestNetwork.cpp$(ObjectSuffix): tests/IntegrationTestLib/TestNetwork.cpp $(IntermediateDirectory)/tests_IntegrationTestLib_TestNetwork.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/IntegrationTestLib/TestNetwork.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_IntegrationTestLib_TestNetwork.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_IntegrationTestLib_TestNetwork.cpp$(DependSuffix): tests/IntegrationTestLib/TestNetwork.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_IntegrationTestLib_TestNetwork.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_IntegrationTestLib_TestNetwork.cpp$(DependSuffix) -MM tests/IntegrationTestLib/TestNetwork.cpp

$(IntermediateDirectory)/tests_IntegrationTestLib_TestNetwork.cpp$(PreprocessSuffix): tests/IntegrationTestLib/TestNetwork.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_IntegrationTestLib_TestNetwork.cpp$(PreprocessSuffix) tests/IntegrationTestLib/TestNetwork.cpp

$(IntermediateDirectory)/tests_IntegrationTestLib_BaseFunctionalTests.cpp$(ObjectSuffix): tests/IntegrationTestLib/BaseFunctionalTests.cpp $(IntermediateDirectory)/tests_IntegrationTestLib_BaseFunctionalTests.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/IntegrationTestLib/BaseFunctionalTests.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_IntegrationTestLib_BaseFunctionalTests.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_IntegrationTestLib_BaseFunctionalTests.cpp$(DependSuffix): tests/IntegrationTestLib/BaseFunctionalTests.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_IntegrationTestLib_BaseFunctionalTests.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_IntegrationTestLib_BaseFunctionalTests.cpp$(DependSuffix) -MM tests/IntegrationTestLib/BaseFunctionalTests.cpp

$(IntermediateDirectory)/tests_IntegrationTestLib_BaseFunctionalTests.cpp$(PreprocessSuffix): tests/IntegrationTestLib/BaseFunctionalTests.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_IntegrationTestLib_BaseFunctionalTests.cpp$(PreprocessSuffix) tests/IntegrationTestLib/BaseFunctionalTests.cpp

$(IntermediateDirectory)/tests_IntegrationTestLib_RPCTestNode.cpp$(ObjectSuffix): tests/IntegrationTestLib/RPCTestNode.cpp $(IntermediateDirectory)/tests_IntegrationTestLib_RPCTestNode.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/IntegrationTestLib/RPCTestNode.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_IntegrationTestLib_RPCTestNode.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_IntegrationTestLib_RPCTestNode.cpp$(DependSuffix): tests/IntegrationTestLib/RPCTestNode.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_IntegrationTestLib_RPCTestNode.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_IntegrationTestLib_RPCTestNode.cpp$(DependSuffix) -MM tests/IntegrationTestLib/RPCTestNode.cpp

$(IntermediateDirectory)/tests_IntegrationTestLib_RPCTestNode.cpp$(PreprocessSuffix): tests/IntegrationTestLib/RPCTestNode.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_IntegrationTestLib_RPCTestNode.cpp$(PreprocessSuffix) tests/IntegrationTestLib/RPCTestNode.cpp

$(IntermediateDirectory)/tests_IntegrationTestLib_InProcTestNode.cpp$(ObjectSuffix): tests/IntegrationTestLib/InProcTestNode.cpp $(IntermediateDirectory)/tests_IntegrationTestLib_InProcTestNode.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/IntegrationTestLib/InProcTestNode.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_IntegrationTestLib_InProcTestNode.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_IntegrationTestLib_InProcTestNode.cpp$(DependSuffix): tests/IntegrationTestLib/InProcTestNode.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_IntegrationTestLib_InProcTestNode.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_IntegrationTestLib_InProcTestNode.cpp$(DependSuffix) -MM tests/IntegrationTestLib/InProcTestNode.cpp

$(IntermediateDirectory)/tests_IntegrationTestLib_InProcTestNode.cpp$(PreprocessSuffix): tests/IntegrationTestLib/InProcTestNode.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_IntegrationTestLib_InProcTestNode.cpp$(PreprocessSuffix) tests/IntegrationTestLib/InProcTestNode.cpp

$(IntermediateDirectory)/tests_IntegrationTestLib_TestWalletLegacy.cpp$(ObjectSuffix): tests/IntegrationTestLib/TestWalletLegacy.cpp $(IntermediateDirectory)/tests_IntegrationTestLib_TestWalletLegacy.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/IntegrationTestLib/TestWalletLegacy.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_IntegrationTestLib_TestWalletLegacy.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_IntegrationTestLib_TestWalletLegacy.cpp$(DependSuffix): tests/IntegrationTestLib/TestWalletLegacy.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_IntegrationTestLib_TestWalletLegacy.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_IntegrationTestLib_TestWalletLegacy.cpp$(DependSuffix) -MM tests/IntegrationTestLib/TestWalletLegacy.cpp

$(IntermediateDirectory)/tests_IntegrationTestLib_TestWalletLegacy.cpp$(PreprocessSuffix): tests/IntegrationTestLib/TestWalletLegacy.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_IntegrationTestLib_TestWalletLegacy.cpp$(PreprocessSuffix) tests/IntegrationTestLib/TestWalletLegacy.cpp

$(IntermediateDirectory)/tests_IntegrationTestLib_Process.cpp$(ObjectSuffix): tests/IntegrationTestLib/Process.cpp $(IntermediateDirectory)/tests_IntegrationTestLib_Process.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/IntegrationTestLib/Process.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_IntegrationTestLib_Process.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_IntegrationTestLib_Process.cpp$(DependSuffix): tests/IntegrationTestLib/Process.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_IntegrationTestLib_Process.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_IntegrationTestLib_Process.cpp$(DependSuffix) -MM tests/IntegrationTestLib/Process.cpp

$(IntermediateDirectory)/tests_IntegrationTestLib_Process.cpp$(PreprocessSuffix): tests/IntegrationTestLib/Process.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_IntegrationTestLib_Process.cpp$(PreprocessSuffix) tests/IntegrationTestLib/Process.cpp

$(IntermediateDirectory)/tests_IntegrationTestLib_Logger.cpp$(ObjectSuffix): tests/IntegrationTestLib/Logger.cpp $(IntermediateDirectory)/tests_IntegrationTestLib_Logger.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/IntegrationTestLib/Logger.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_IntegrationTestLib_Logger.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_IntegrationTestLib_Logger.cpp$(DependSuffix): tests/IntegrationTestLib/Logger.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_IntegrationTestLib_Logger.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_IntegrationTestLib_Logger.cpp$(DependSuffix) -MM tests/IntegrationTestLib/Logger.cpp

$(IntermediateDirectory)/tests_IntegrationTestLib_Logger.cpp$(PreprocessSuffix): tests/IntegrationTestLib/Logger.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_IntegrationTestLib_Logger.cpp$(PreprocessSuffix) tests/IntegrationTestLib/Logger.cpp

$(IntermediateDirectory)/tests_DifficultyTests_Difficulty.cpp$(ObjectSuffix): tests/DifficultyTests/Difficulty.cpp $(IntermediateDirectory)/tests_DifficultyTests_Difficulty.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/DifficultyTests/Difficulty.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_DifficultyTests_Difficulty.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_DifficultyTests_Difficulty.cpp$(DependSuffix): tests/DifficultyTests/Difficulty.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_DifficultyTests_Difficulty.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_DifficultyTests_Difficulty.cpp$(DependSuffix) -MM tests/DifficultyTests/Difficulty.cpp

$(IntermediateDirectory)/tests_DifficultyTests_Difficulty.cpp$(PreprocessSuffix): tests/DifficultyTests/Difficulty.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_DifficultyTests_Difficulty.cpp$(PreprocessSuffix) tests/DifficultyTests/Difficulty.cpp

$(IntermediateDirectory)/tests_HashTargetTests_HashTarget.cpp$(ObjectSuffix): tests/HashTargetTests/HashTarget.cpp $(IntermediateDirectory)/tests_HashTargetTests_HashTarget.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/HashTargetTests/HashTarget.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_HashTargetTests_HashTarget.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_HashTargetTests_HashTarget.cpp$(DependSuffix): tests/HashTargetTests/HashTarget.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_HashTargetTests_HashTarget.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_HashTargetTests_HashTarget.cpp$(DependSuffix) -MM tests/HashTargetTests/HashTarget.cpp

$(IntermediateDirectory)/tests_HashTargetTests_HashTarget.cpp$(PreprocessSuffix): tests/HashTargetTests/HashTarget.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_HashTargetTests_HashTarget.cpp$(PreprocessSuffix) tests/HashTargetTests/HashTarget.cpp

$(IntermediateDirectory)/tests_TransfersTests_Tests.cpp$(ObjectSuffix): tests/TransfersTests/Tests.cpp $(IntermediateDirectory)/tests_TransfersTests_Tests.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/TransfersTests/Tests.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_TransfersTests_Tests.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_TransfersTests_Tests.cpp$(DependSuffix): tests/TransfersTests/Tests.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_TransfersTests_Tests.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_TransfersTests_Tests.cpp$(DependSuffix) -MM tests/TransfersTests/Tests.cpp

$(IntermediateDirectory)/tests_TransfersTests_Tests.cpp$(PreprocessSuffix): tests/TransfersTests/Tests.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_TransfersTests_Tests.cpp$(PreprocessSuffix) tests/TransfersTests/Tests.cpp

$(IntermediateDirectory)/tests_TransfersTests_main.cpp$(ObjectSuffix): tests/TransfersTests/main.cpp $(IntermediateDirectory)/tests_TransfersTests_main.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/TransfersTests/main.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_TransfersTests_main.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_TransfersTests_main.cpp$(DependSuffix): tests/TransfersTests/main.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_TransfersTests_main.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_TransfersTests_main.cpp$(DependSuffix) -MM tests/TransfersTests/main.cpp

$(IntermediateDirectory)/tests_TransfersTests_main.cpp$(PreprocessSuffix): tests/TransfersTests/main.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_TransfersTests_main.cpp$(PreprocessSuffix) tests/TransfersTests/main.cpp

$(IntermediateDirectory)/tests_TransfersTests_TestTxPoolSync.cpp$(ObjectSuffix): tests/TransfersTests/TestTxPoolSync.cpp $(IntermediateDirectory)/tests_TransfersTests_TestTxPoolSync.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/TransfersTests/TestTxPoolSync.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_TransfersTests_TestTxPoolSync.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_TransfersTests_TestTxPoolSync.cpp$(DependSuffix): tests/TransfersTests/TestTxPoolSync.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_TransfersTests_TestTxPoolSync.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_TransfersTests_TestTxPoolSync.cpp$(DependSuffix) -MM tests/TransfersTests/TestTxPoolSync.cpp

$(IntermediateDirectory)/tests_TransfersTests_TestTxPoolSync.cpp$(PreprocessSuffix): tests/TransfersTests/TestTxPoolSync.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_TransfersTests_TestTxPoolSync.cpp$(PreprocessSuffix) tests/TransfersTests/TestTxPoolSync.cpp

$(IntermediateDirectory)/tests_TransfersTests_TestNodeRpcProxy.cpp$(ObjectSuffix): tests/TransfersTests/TestNodeRpcProxy.cpp $(IntermediateDirectory)/tests_TransfersTests_TestNodeRpcProxy.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/tests/TransfersTests/TestNodeRpcProxy.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/tests_TransfersTests_TestNodeRpcProxy.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/tests_TransfersTests_TestNodeRpcProxy.cpp$(DependSuffix): tests/TransfersTests/TestNodeRpcProxy.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/tests_TransfersTests_TestNodeRpcProxy.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/tests_TransfersTests_TestNodeRpcProxy.cpp$(DependSuffix) -MM tests/TransfersTests/TestNodeRpcProxy.cpp

$(IntermediateDirectory)/tests_TransfersTests_TestNodeRpcProxy.cpp$(PreprocessSuffix): tests/TransfersTests/TestNodeRpcProxy.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/tests_TransfersTests_TestNodeRpcProxy.cpp$(PreprocessSuffix) tests/TransfersTests/TestNodeRpcProxy.cpp

$(IntermediateDirectory)/lib_Platform_Linux_System_ErrorMessage.cpp$(ObjectSuffix): lib/Platform/Linux/System/ErrorMessage.cpp $(IntermediateDirectory)/lib_Platform_Linux_System_ErrorMessage.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Platform/Linux/System/ErrorMessage.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Platform_Linux_System_ErrorMessage.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Platform_Linux_System_ErrorMessage.cpp$(DependSuffix): lib/Platform/Linux/System/ErrorMessage.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Platform_Linux_System_ErrorMessage.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Platform_Linux_System_ErrorMessage.cpp$(DependSuffix) -MM lib/Platform/Linux/System/ErrorMessage.cpp

$(IntermediateDirectory)/lib_Platform_Linux_System_ErrorMessage.cpp$(PreprocessSuffix): lib/Platform/Linux/System/ErrorMessage.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Platform_Linux_System_ErrorMessage.cpp$(PreprocessSuffix) lib/Platform/Linux/System/ErrorMessage.cpp

$(IntermediateDirectory)/lib_Platform_Linux_System_TcpConnection.cpp$(ObjectSuffix): lib/Platform/Linux/System/TcpConnection.cpp $(IntermediateDirectory)/lib_Platform_Linux_System_TcpConnection.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Platform/Linux/System/TcpConnection.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Platform_Linux_System_TcpConnection.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Platform_Linux_System_TcpConnection.cpp$(DependSuffix): lib/Platform/Linux/System/TcpConnection.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Platform_Linux_System_TcpConnection.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Platform_Linux_System_TcpConnection.cpp$(DependSuffix) -MM lib/Platform/Linux/System/TcpConnection.cpp

$(IntermediateDirectory)/lib_Platform_Linux_System_TcpConnection.cpp$(PreprocessSuffix): lib/Platform/Linux/System/TcpConnection.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Platform_Linux_System_TcpConnection.cpp$(PreprocessSuffix) lib/Platform/Linux/System/TcpConnection.cpp

$(IntermediateDirectory)/lib_Platform_Linux_System_TcpListener.cpp$(ObjectSuffix): lib/Platform/Linux/System/TcpListener.cpp $(IntermediateDirectory)/lib_Platform_Linux_System_TcpListener.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Platform/Linux/System/TcpListener.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Platform_Linux_System_TcpListener.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Platform_Linux_System_TcpListener.cpp$(DependSuffix): lib/Platform/Linux/System/TcpListener.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Platform_Linux_System_TcpListener.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Platform_Linux_System_TcpListener.cpp$(DependSuffix) -MM lib/Platform/Linux/System/TcpListener.cpp

$(IntermediateDirectory)/lib_Platform_Linux_System_TcpListener.cpp$(PreprocessSuffix): lib/Platform/Linux/System/TcpListener.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Platform_Linux_System_TcpListener.cpp$(PreprocessSuffix) lib/Platform/Linux/System/TcpListener.cpp

$(IntermediateDirectory)/lib_Platform_Linux_System_Dispatcher.cpp$(ObjectSuffix): lib/Platform/Linux/System/Dispatcher.cpp $(IntermediateDirectory)/lib_Platform_Linux_System_Dispatcher.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Platform/Linux/System/Dispatcher.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Platform_Linux_System_Dispatcher.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Platform_Linux_System_Dispatcher.cpp$(DependSuffix): lib/Platform/Linux/System/Dispatcher.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Platform_Linux_System_Dispatcher.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Platform_Linux_System_Dispatcher.cpp$(DependSuffix) -MM lib/Platform/Linux/System/Dispatcher.cpp

$(IntermediateDirectory)/lib_Platform_Linux_System_Dispatcher.cpp$(PreprocessSuffix): lib/Platform/Linux/System/Dispatcher.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Platform_Linux_System_Dispatcher.cpp$(PreprocessSuffix) lib/Platform/Linux/System/Dispatcher.cpp

$(IntermediateDirectory)/lib_Platform_Linux_System_Ipv4Resolver.cpp$(ObjectSuffix): lib/Platform/Linux/System/Ipv4Resolver.cpp $(IntermediateDirectory)/lib_Platform_Linux_System_Ipv4Resolver.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Platform/Linux/System/Ipv4Resolver.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Platform_Linux_System_Ipv4Resolver.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Platform_Linux_System_Ipv4Resolver.cpp$(DependSuffix): lib/Platform/Linux/System/Ipv4Resolver.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Platform_Linux_System_Ipv4Resolver.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Platform_Linux_System_Ipv4Resolver.cpp$(DependSuffix) -MM lib/Platform/Linux/System/Ipv4Resolver.cpp

$(IntermediateDirectory)/lib_Platform_Linux_System_Ipv4Resolver.cpp$(PreprocessSuffix): lib/Platform/Linux/System/Ipv4Resolver.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Platform_Linux_System_Ipv4Resolver.cpp$(PreprocessSuffix) lib/Platform/Linux/System/Ipv4Resolver.cpp

$(IntermediateDirectory)/lib_Platform_Linux_System_Timer.cpp$(ObjectSuffix): lib/Platform/Linux/System/Timer.cpp $(IntermediateDirectory)/lib_Platform_Linux_System_Timer.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Platform/Linux/System/Timer.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Platform_Linux_System_Timer.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Platform_Linux_System_Timer.cpp$(DependSuffix): lib/Platform/Linux/System/Timer.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Platform_Linux_System_Timer.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Platform_Linux_System_Timer.cpp$(DependSuffix) -MM lib/Platform/Linux/System/Timer.cpp

$(IntermediateDirectory)/lib_Platform_Linux_System_Timer.cpp$(PreprocessSuffix): lib/Platform/Linux/System/Timer.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Platform_Linux_System_Timer.cpp$(PreprocessSuffix) lib/Platform/Linux/System/Timer.cpp

$(IntermediateDirectory)/lib_Platform_Linux_System_TcpConnector.cpp$(ObjectSuffix): lib/Platform/Linux/System/TcpConnector.cpp $(IntermediateDirectory)/lib_Platform_Linux_System_TcpConnector.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Platform/Linux/System/TcpConnector.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Platform_Linux_System_TcpConnector.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Platform_Linux_System_TcpConnector.cpp$(DependSuffix): lib/Platform/Linux/System/TcpConnector.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Platform_Linux_System_TcpConnector.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Platform_Linux_System_TcpConnector.cpp$(DependSuffix) -MM lib/Platform/Linux/System/TcpConnector.cpp

$(IntermediateDirectory)/lib_Platform_Linux_System_TcpConnector.cpp$(PreprocessSuffix): lib/Platform/Linux/System/TcpConnector.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Platform_Linux_System_TcpConnector.cpp$(PreprocessSuffix) lib/Platform/Linux/System/TcpConnector.cpp

$(IntermediateDirectory)/lib_Platform_Posix_System_MemoryMappedFile.cpp$(ObjectSuffix): lib/Platform/Posix/System/MemoryMappedFile.cpp $(IntermediateDirectory)/lib_Platform_Posix_System_MemoryMappedFile.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Platform/Posix/System/MemoryMappedFile.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Platform_Posix_System_MemoryMappedFile.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Platform_Posix_System_MemoryMappedFile.cpp$(DependSuffix): lib/Platform/Posix/System/MemoryMappedFile.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Platform_Posix_System_MemoryMappedFile.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Platform_Posix_System_MemoryMappedFile.cpp$(DependSuffix) -MM lib/Platform/Posix/System/MemoryMappedFile.cpp

$(IntermediateDirectory)/lib_Platform_Posix_System_MemoryMappedFile.cpp$(PreprocessSuffix): lib/Platform/Posix/System/MemoryMappedFile.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Platform_Posix_System_MemoryMappedFile.cpp$(PreprocessSuffix) lib/Platform/Posix/System/MemoryMappedFile.cpp

$(IntermediateDirectory)/lib_Platform_FreeBSD_System_Context.c$(ObjectSuffix): lib/Platform/FreeBSD/System/Context.c $(IntermediateDirectory)/lib_Platform_FreeBSD_System_Context.c$(DependSuffix)
	$(CC) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Platform/FreeBSD/System/Context.c" $(CFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Platform_FreeBSD_System_Context.c$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Platform_FreeBSD_System_Context.c$(DependSuffix): lib/Platform/FreeBSD/System/Context.c
	@$(CC) $(CFLAGS) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Platform_FreeBSD_System_Context.c$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Platform_FreeBSD_System_Context.c$(DependSuffix) -MM lib/Platform/FreeBSD/System/Context.c

$(IntermediateDirectory)/lib_Platform_FreeBSD_System_Context.c$(PreprocessSuffix): lib/Platform/FreeBSD/System/Context.c
	$(CC) $(CFLAGS) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Platform_FreeBSD_System_Context.c$(PreprocessSuffix) lib/Platform/FreeBSD/System/Context.c

$(IntermediateDirectory)/lib_Platform_FreeBSD_System_ErrorMessage.cpp$(ObjectSuffix): lib/Platform/FreeBSD/System/ErrorMessage.cpp $(IntermediateDirectory)/lib_Platform_FreeBSD_System_ErrorMessage.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Platform/FreeBSD/System/ErrorMessage.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Platform_FreeBSD_System_ErrorMessage.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Platform_FreeBSD_System_ErrorMessage.cpp$(DependSuffix): lib/Platform/FreeBSD/System/ErrorMessage.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Platform_FreeBSD_System_ErrorMessage.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Platform_FreeBSD_System_ErrorMessage.cpp$(DependSuffix) -MM lib/Platform/FreeBSD/System/ErrorMessage.cpp

$(IntermediateDirectory)/lib_Platform_FreeBSD_System_ErrorMessage.cpp$(PreprocessSuffix): lib/Platform/FreeBSD/System/ErrorMessage.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Platform_FreeBSD_System_ErrorMessage.cpp$(PreprocessSuffix) lib/Platform/FreeBSD/System/ErrorMessage.cpp

$(IntermediateDirectory)/lib_Platform_FreeBSD_System_TcpConnection.cpp$(ObjectSuffix): lib/Platform/FreeBSD/System/TcpConnection.cpp $(IntermediateDirectory)/lib_Platform_FreeBSD_System_TcpConnection.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Platform/FreeBSD/System/TcpConnection.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Platform_FreeBSD_System_TcpConnection.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Platform_FreeBSD_System_TcpConnection.cpp$(DependSuffix): lib/Platform/FreeBSD/System/TcpConnection.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Platform_FreeBSD_System_TcpConnection.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Platform_FreeBSD_System_TcpConnection.cpp$(DependSuffix) -MM lib/Platform/FreeBSD/System/TcpConnection.cpp

$(IntermediateDirectory)/lib_Platform_FreeBSD_System_TcpConnection.cpp$(PreprocessSuffix): lib/Platform/FreeBSD/System/TcpConnection.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Platform_FreeBSD_System_TcpConnection.cpp$(PreprocessSuffix) lib/Platform/FreeBSD/System/TcpConnection.cpp

$(IntermediateDirectory)/lib_Platform_FreeBSD_System_TcpListener.cpp$(ObjectSuffix): lib/Platform/FreeBSD/System/TcpListener.cpp $(IntermediateDirectory)/lib_Platform_FreeBSD_System_TcpListener.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Platform/FreeBSD/System/TcpListener.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Platform_FreeBSD_System_TcpListener.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Platform_FreeBSD_System_TcpListener.cpp$(DependSuffix): lib/Platform/FreeBSD/System/TcpListener.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Platform_FreeBSD_System_TcpListener.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Platform_FreeBSD_System_TcpListener.cpp$(DependSuffix) -MM lib/Platform/FreeBSD/System/TcpListener.cpp

$(IntermediateDirectory)/lib_Platform_FreeBSD_System_TcpListener.cpp$(PreprocessSuffix): lib/Platform/FreeBSD/System/TcpListener.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Platform_FreeBSD_System_TcpListener.cpp$(PreprocessSuffix) lib/Platform/FreeBSD/System/TcpListener.cpp

$(IntermediateDirectory)/lib_Platform_FreeBSD_System_Dispatcher.cpp$(ObjectSuffix): lib/Platform/FreeBSD/System/Dispatcher.cpp $(IntermediateDirectory)/lib_Platform_FreeBSD_System_Dispatcher.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Platform/FreeBSD/System/Dispatcher.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Platform_FreeBSD_System_Dispatcher.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Platform_FreeBSD_System_Dispatcher.cpp$(DependSuffix): lib/Platform/FreeBSD/System/Dispatcher.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Platform_FreeBSD_System_Dispatcher.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Platform_FreeBSD_System_Dispatcher.cpp$(DependSuffix) -MM lib/Platform/FreeBSD/System/Dispatcher.cpp

$(IntermediateDirectory)/lib_Platform_FreeBSD_System_Dispatcher.cpp$(PreprocessSuffix): lib/Platform/FreeBSD/System/Dispatcher.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Platform_FreeBSD_System_Dispatcher.cpp$(PreprocessSuffix) lib/Platform/FreeBSD/System/Dispatcher.cpp

$(IntermediateDirectory)/lib_Platform_FreeBSD_System_Ipv4Resolver.cpp$(ObjectSuffix): lib/Platform/FreeBSD/System/Ipv4Resolver.cpp $(IntermediateDirectory)/lib_Platform_FreeBSD_System_Ipv4Resolver.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Platform/FreeBSD/System/Ipv4Resolver.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Platform_FreeBSD_System_Ipv4Resolver.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Platform_FreeBSD_System_Ipv4Resolver.cpp$(DependSuffix): lib/Platform/FreeBSD/System/Ipv4Resolver.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Platform_FreeBSD_System_Ipv4Resolver.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Platform_FreeBSD_System_Ipv4Resolver.cpp$(DependSuffix) -MM lib/Platform/FreeBSD/System/Ipv4Resolver.cpp

$(IntermediateDirectory)/lib_Platform_FreeBSD_System_Ipv4Resolver.cpp$(PreprocessSuffix): lib/Platform/FreeBSD/System/Ipv4Resolver.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Platform_FreeBSD_System_Ipv4Resolver.cpp$(PreprocessSuffix) lib/Platform/FreeBSD/System/Ipv4Resolver.cpp

$(IntermediateDirectory)/lib_Platform_FreeBSD_System_Timer.cpp$(ObjectSuffix): lib/Platform/FreeBSD/System/Timer.cpp $(IntermediateDirectory)/lib_Platform_FreeBSD_System_Timer.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Platform/FreeBSD/System/Timer.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Platform_FreeBSD_System_Timer.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Platform_FreeBSD_System_Timer.cpp$(DependSuffix): lib/Platform/FreeBSD/System/Timer.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Platform_FreeBSD_System_Timer.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Platform_FreeBSD_System_Timer.cpp$(DependSuffix) -MM lib/Platform/FreeBSD/System/Timer.cpp

$(IntermediateDirectory)/lib_Platform_FreeBSD_System_Timer.cpp$(PreprocessSuffix): lib/Platform/FreeBSD/System/Timer.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Platform_FreeBSD_System_Timer.cpp$(PreprocessSuffix) lib/Platform/FreeBSD/System/Timer.cpp

$(IntermediateDirectory)/lib_Platform_FreeBSD_System_TcpConnector.cpp$(ObjectSuffix): lib/Platform/FreeBSD/System/TcpConnector.cpp $(IntermediateDirectory)/lib_Platform_FreeBSD_System_TcpConnector.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Platform/FreeBSD/System/TcpConnector.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Platform_FreeBSD_System_TcpConnector.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Platform_FreeBSD_System_TcpConnector.cpp$(DependSuffix): lib/Platform/FreeBSD/System/TcpConnector.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Platform_FreeBSD_System_TcpConnector.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Platform_FreeBSD_System_TcpConnector.cpp$(DependSuffix) -MM lib/Platform/FreeBSD/System/TcpConnector.cpp

$(IntermediateDirectory)/lib_Platform_FreeBSD_System_TcpConnector.cpp$(PreprocessSuffix): lib/Platform/FreeBSD/System/TcpConnector.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Platform_FreeBSD_System_TcpConnector.cpp$(PreprocessSuffix) lib/Platform/FreeBSD/System/TcpConnector.cpp

$(IntermediateDirectory)/lib_Platform_Android_System_ErrorMessage.cpp$(ObjectSuffix): lib/Platform/Android/System/ErrorMessage.cpp $(IntermediateDirectory)/lib_Platform_Android_System_ErrorMessage.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Platform/Android/System/ErrorMessage.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Platform_Android_System_ErrorMessage.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Platform_Android_System_ErrorMessage.cpp$(DependSuffix): lib/Platform/Android/System/ErrorMessage.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Platform_Android_System_ErrorMessage.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Platform_Android_System_ErrorMessage.cpp$(DependSuffix) -MM lib/Platform/Android/System/ErrorMessage.cpp

$(IntermediateDirectory)/lib_Platform_Android_System_ErrorMessage.cpp$(PreprocessSuffix): lib/Platform/Android/System/ErrorMessage.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Platform_Android_System_ErrorMessage.cpp$(PreprocessSuffix) lib/Platform/Android/System/ErrorMessage.cpp

$(IntermediateDirectory)/lib_Platform_Android_System_TcpConnection.cpp$(ObjectSuffix): lib/Platform/Android/System/TcpConnection.cpp $(IntermediateDirectory)/lib_Platform_Android_System_TcpConnection.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Platform/Android/System/TcpConnection.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Platform_Android_System_TcpConnection.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Platform_Android_System_TcpConnection.cpp$(DependSuffix): lib/Platform/Android/System/TcpConnection.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Platform_Android_System_TcpConnection.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Platform_Android_System_TcpConnection.cpp$(DependSuffix) -MM lib/Platform/Android/System/TcpConnection.cpp

$(IntermediateDirectory)/lib_Platform_Android_System_TcpConnection.cpp$(PreprocessSuffix): lib/Platform/Android/System/TcpConnection.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Platform_Android_System_TcpConnection.cpp$(PreprocessSuffix) lib/Platform/Android/System/TcpConnection.cpp

$(IntermediateDirectory)/lib_Platform_Android_System_TcpListener.cpp$(ObjectSuffix): lib/Platform/Android/System/TcpListener.cpp $(IntermediateDirectory)/lib_Platform_Android_System_TcpListener.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Platform/Android/System/TcpListener.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Platform_Android_System_TcpListener.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Platform_Android_System_TcpListener.cpp$(DependSuffix): lib/Platform/Android/System/TcpListener.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Platform_Android_System_TcpListener.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Platform_Android_System_TcpListener.cpp$(DependSuffix) -MM lib/Platform/Android/System/TcpListener.cpp

$(IntermediateDirectory)/lib_Platform_Android_System_TcpListener.cpp$(PreprocessSuffix): lib/Platform/Android/System/TcpListener.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Platform_Android_System_TcpListener.cpp$(PreprocessSuffix) lib/Platform/Android/System/TcpListener.cpp

$(IntermediateDirectory)/lib_Platform_Android_System_Dispatcher.cpp$(ObjectSuffix): lib/Platform/Android/System/Dispatcher.cpp $(IntermediateDirectory)/lib_Platform_Android_System_Dispatcher.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Platform/Android/System/Dispatcher.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Platform_Android_System_Dispatcher.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Platform_Android_System_Dispatcher.cpp$(DependSuffix): lib/Platform/Android/System/Dispatcher.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Platform_Android_System_Dispatcher.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Platform_Android_System_Dispatcher.cpp$(DependSuffix) -MM lib/Platform/Android/System/Dispatcher.cpp

$(IntermediateDirectory)/lib_Platform_Android_System_Dispatcher.cpp$(PreprocessSuffix): lib/Platform/Android/System/Dispatcher.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Platform_Android_System_Dispatcher.cpp$(PreprocessSuffix) lib/Platform/Android/System/Dispatcher.cpp

$(IntermediateDirectory)/lib_Platform_Android_System_Ipv4Resolver.cpp$(ObjectSuffix): lib/Platform/Android/System/Ipv4Resolver.cpp $(IntermediateDirectory)/lib_Platform_Android_System_Ipv4Resolver.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Platform/Android/System/Ipv4Resolver.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Platform_Android_System_Ipv4Resolver.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Platform_Android_System_Ipv4Resolver.cpp$(DependSuffix): lib/Platform/Android/System/Ipv4Resolver.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Platform_Android_System_Ipv4Resolver.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Platform_Android_System_Ipv4Resolver.cpp$(DependSuffix) -MM lib/Platform/Android/System/Ipv4Resolver.cpp

$(IntermediateDirectory)/lib_Platform_Android_System_Ipv4Resolver.cpp$(PreprocessSuffix): lib/Platform/Android/System/Ipv4Resolver.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Platform_Android_System_Ipv4Resolver.cpp$(PreprocessSuffix) lib/Platform/Android/System/Ipv4Resolver.cpp

$(IntermediateDirectory)/lib_Platform_Android_System_Timer.cpp$(ObjectSuffix): lib/Platform/Android/System/Timer.cpp $(IntermediateDirectory)/lib_Platform_Android_System_Timer.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Platform/Android/System/Timer.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Platform_Android_System_Timer.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Platform_Android_System_Timer.cpp$(DependSuffix): lib/Platform/Android/System/Timer.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Platform_Android_System_Timer.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Platform_Android_System_Timer.cpp$(DependSuffix) -MM lib/Platform/Android/System/Timer.cpp

$(IntermediateDirectory)/lib_Platform_Android_System_Timer.cpp$(PreprocessSuffix): lib/Platform/Android/System/Timer.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Platform_Android_System_Timer.cpp$(PreprocessSuffix) lib/Platform/Android/System/Timer.cpp

$(IntermediateDirectory)/lib_Platform_Android_System_TcpConnector.cpp$(ObjectSuffix): lib/Platform/Android/System/TcpConnector.cpp $(IntermediateDirectory)/lib_Platform_Android_System_TcpConnector.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Platform/Android/System/TcpConnector.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Platform_Android_System_TcpConnector.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Platform_Android_System_TcpConnector.cpp$(DependSuffix): lib/Platform/Android/System/TcpConnector.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Platform_Android_System_TcpConnector.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Platform_Android_System_TcpConnector.cpp$(DependSuffix) -MM lib/Platform/Android/System/TcpConnector.cpp

$(IntermediateDirectory)/lib_Platform_Android_System_TcpConnector.cpp$(PreprocessSuffix): lib/Platform/Android/System/TcpConnector.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Platform_Android_System_TcpConnector.cpp$(PreprocessSuffix) lib/Platform/Android/System/TcpConnector.cpp

$(IntermediateDirectory)/lib_Platform_Windows_System_ErrorMessage.cpp$(ObjectSuffix): lib/Platform/Windows/System/ErrorMessage.cpp $(IntermediateDirectory)/lib_Platform_Windows_System_ErrorMessage.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Platform/Windows/System/ErrorMessage.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Platform_Windows_System_ErrorMessage.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Platform_Windows_System_ErrorMessage.cpp$(DependSuffix): lib/Platform/Windows/System/ErrorMessage.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Platform_Windows_System_ErrorMessage.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Platform_Windows_System_ErrorMessage.cpp$(DependSuffix) -MM lib/Platform/Windows/System/ErrorMessage.cpp

$(IntermediateDirectory)/lib_Platform_Windows_System_ErrorMessage.cpp$(PreprocessSuffix): lib/Platform/Windows/System/ErrorMessage.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Platform_Windows_System_ErrorMessage.cpp$(PreprocessSuffix) lib/Platform/Windows/System/ErrorMessage.cpp

$(IntermediateDirectory)/lib_Platform_Windows_System_TcpConnection.cpp$(ObjectSuffix): lib/Platform/Windows/System/TcpConnection.cpp $(IntermediateDirectory)/lib_Platform_Windows_System_TcpConnection.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Platform/Windows/System/TcpConnection.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Platform_Windows_System_TcpConnection.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Platform_Windows_System_TcpConnection.cpp$(DependSuffix): lib/Platform/Windows/System/TcpConnection.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Platform_Windows_System_TcpConnection.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Platform_Windows_System_TcpConnection.cpp$(DependSuffix) -MM lib/Platform/Windows/System/TcpConnection.cpp

$(IntermediateDirectory)/lib_Platform_Windows_System_TcpConnection.cpp$(PreprocessSuffix): lib/Platform/Windows/System/TcpConnection.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Platform_Windows_System_TcpConnection.cpp$(PreprocessSuffix) lib/Platform/Windows/System/TcpConnection.cpp

$(IntermediateDirectory)/lib_Platform_Windows_System_TcpListener.cpp$(ObjectSuffix): lib/Platform/Windows/System/TcpListener.cpp $(IntermediateDirectory)/lib_Platform_Windows_System_TcpListener.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Platform/Windows/System/TcpListener.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Platform_Windows_System_TcpListener.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Platform_Windows_System_TcpListener.cpp$(DependSuffix): lib/Platform/Windows/System/TcpListener.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Platform_Windows_System_TcpListener.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Platform_Windows_System_TcpListener.cpp$(DependSuffix) -MM lib/Platform/Windows/System/TcpListener.cpp

$(IntermediateDirectory)/lib_Platform_Windows_System_TcpListener.cpp$(PreprocessSuffix): lib/Platform/Windows/System/TcpListener.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Platform_Windows_System_TcpListener.cpp$(PreprocessSuffix) lib/Platform/Windows/System/TcpListener.cpp

$(IntermediateDirectory)/lib_Platform_Windows_System_MemoryMappedFile.cpp$(ObjectSuffix): lib/Platform/Windows/System/MemoryMappedFile.cpp $(IntermediateDirectory)/lib_Platform_Windows_System_MemoryMappedFile.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Platform/Windows/System/MemoryMappedFile.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Platform_Windows_System_MemoryMappedFile.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Platform_Windows_System_MemoryMappedFile.cpp$(DependSuffix): lib/Platform/Windows/System/MemoryMappedFile.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Platform_Windows_System_MemoryMappedFile.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Platform_Windows_System_MemoryMappedFile.cpp$(DependSuffix) -MM lib/Platform/Windows/System/MemoryMappedFile.cpp

$(IntermediateDirectory)/lib_Platform_Windows_System_MemoryMappedFile.cpp$(PreprocessSuffix): lib/Platform/Windows/System/MemoryMappedFile.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Platform_Windows_System_MemoryMappedFile.cpp$(PreprocessSuffix) lib/Platform/Windows/System/MemoryMappedFile.cpp

$(IntermediateDirectory)/lib_Platform_Windows_System_Dispatcher.cpp$(ObjectSuffix): lib/Platform/Windows/System/Dispatcher.cpp $(IntermediateDirectory)/lib_Platform_Windows_System_Dispatcher.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Platform/Windows/System/Dispatcher.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Platform_Windows_System_Dispatcher.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Platform_Windows_System_Dispatcher.cpp$(DependSuffix): lib/Platform/Windows/System/Dispatcher.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Platform_Windows_System_Dispatcher.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Platform_Windows_System_Dispatcher.cpp$(DependSuffix) -MM lib/Platform/Windows/System/Dispatcher.cpp

$(IntermediateDirectory)/lib_Platform_Windows_System_Dispatcher.cpp$(PreprocessSuffix): lib/Platform/Windows/System/Dispatcher.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Platform_Windows_System_Dispatcher.cpp$(PreprocessSuffix) lib/Platform/Windows/System/Dispatcher.cpp

$(IntermediateDirectory)/lib_Platform_Windows_System_Ipv4Resolver.cpp$(ObjectSuffix): lib/Platform/Windows/System/Ipv4Resolver.cpp $(IntermediateDirectory)/lib_Platform_Windows_System_Ipv4Resolver.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Platform/Windows/System/Ipv4Resolver.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Platform_Windows_System_Ipv4Resolver.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Platform_Windows_System_Ipv4Resolver.cpp$(DependSuffix): lib/Platform/Windows/System/Ipv4Resolver.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Platform_Windows_System_Ipv4Resolver.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Platform_Windows_System_Ipv4Resolver.cpp$(DependSuffix) -MM lib/Platform/Windows/System/Ipv4Resolver.cpp

$(IntermediateDirectory)/lib_Platform_Windows_System_Ipv4Resolver.cpp$(PreprocessSuffix): lib/Platform/Windows/System/Ipv4Resolver.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Platform_Windows_System_Ipv4Resolver.cpp$(PreprocessSuffix) lib/Platform/Windows/System/Ipv4Resolver.cpp

$(IntermediateDirectory)/lib_Platform_Windows_System_Timer.cpp$(ObjectSuffix): lib/Platform/Windows/System/Timer.cpp $(IntermediateDirectory)/lib_Platform_Windows_System_Timer.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Platform/Windows/System/Timer.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Platform_Windows_System_Timer.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Platform_Windows_System_Timer.cpp$(DependSuffix): lib/Platform/Windows/System/Timer.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Platform_Windows_System_Timer.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Platform_Windows_System_Timer.cpp$(DependSuffix) -MM lib/Platform/Windows/System/Timer.cpp

$(IntermediateDirectory)/lib_Platform_Windows_System_Timer.cpp$(PreprocessSuffix): lib/Platform/Windows/System/Timer.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Platform_Windows_System_Timer.cpp$(PreprocessSuffix) lib/Platform/Windows/System/Timer.cpp

$(IntermediateDirectory)/lib_Platform_Windows_System_TcpConnector.cpp$(ObjectSuffix): lib/Platform/Windows/System/TcpConnector.cpp $(IntermediateDirectory)/lib_Platform_Windows_System_TcpConnector.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Platform/Windows/System/TcpConnector.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Platform_Windows_System_TcpConnector.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Platform_Windows_System_TcpConnector.cpp$(DependSuffix): lib/Platform/Windows/System/TcpConnector.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Platform_Windows_System_TcpConnector.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Platform_Windows_System_TcpConnector.cpp$(DependSuffix) -MM lib/Platform/Windows/System/TcpConnector.cpp

$(IntermediateDirectory)/lib_Platform_Windows_System_TcpConnector.cpp$(PreprocessSuffix): lib/Platform/Windows/System/TcpConnector.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Platform_Windows_System_TcpConnector.cpp$(PreprocessSuffix) lib/Platform/Windows/System/TcpConnector.cpp

$(IntermediateDirectory)/lib_Platform_OSX_System_Context.c$(ObjectSuffix): lib/Platform/OSX/System/Context.c $(IntermediateDirectory)/lib_Platform_OSX_System_Context.c$(DependSuffix)
	$(CC) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Platform/OSX/System/Context.c" $(CFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Platform_OSX_System_Context.c$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Platform_OSX_System_Context.c$(DependSuffix): lib/Platform/OSX/System/Context.c
	@$(CC) $(CFLAGS) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Platform_OSX_System_Context.c$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Platform_OSX_System_Context.c$(DependSuffix) -MM lib/Platform/OSX/System/Context.c

$(IntermediateDirectory)/lib_Platform_OSX_System_Context.c$(PreprocessSuffix): lib/Platform/OSX/System/Context.c
	$(CC) $(CFLAGS) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Platform_OSX_System_Context.c$(PreprocessSuffix) lib/Platform/OSX/System/Context.c

$(IntermediateDirectory)/lib_Platform_OSX_System_ErrorMessage.cpp$(ObjectSuffix): lib/Platform/OSX/System/ErrorMessage.cpp $(IntermediateDirectory)/lib_Platform_OSX_System_ErrorMessage.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Platform/OSX/System/ErrorMessage.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Platform_OSX_System_ErrorMessage.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Platform_OSX_System_ErrorMessage.cpp$(DependSuffix): lib/Platform/OSX/System/ErrorMessage.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Platform_OSX_System_ErrorMessage.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Platform_OSX_System_ErrorMessage.cpp$(DependSuffix) -MM lib/Platform/OSX/System/ErrorMessage.cpp

$(IntermediateDirectory)/lib_Platform_OSX_System_ErrorMessage.cpp$(PreprocessSuffix): lib/Platform/OSX/System/ErrorMessage.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Platform_OSX_System_ErrorMessage.cpp$(PreprocessSuffix) lib/Platform/OSX/System/ErrorMessage.cpp

$(IntermediateDirectory)/lib_Platform_OSX_System_TcpConnection.cpp$(ObjectSuffix): lib/Platform/OSX/System/TcpConnection.cpp $(IntermediateDirectory)/lib_Platform_OSX_System_TcpConnection.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Platform/OSX/System/TcpConnection.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Platform_OSX_System_TcpConnection.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Platform_OSX_System_TcpConnection.cpp$(DependSuffix): lib/Platform/OSX/System/TcpConnection.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Platform_OSX_System_TcpConnection.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Platform_OSX_System_TcpConnection.cpp$(DependSuffix) -MM lib/Platform/OSX/System/TcpConnection.cpp

$(IntermediateDirectory)/lib_Platform_OSX_System_TcpConnection.cpp$(PreprocessSuffix): lib/Platform/OSX/System/TcpConnection.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Platform_OSX_System_TcpConnection.cpp$(PreprocessSuffix) lib/Platform/OSX/System/TcpConnection.cpp

$(IntermediateDirectory)/lib_Platform_OSX_System_TcpListener.cpp$(ObjectSuffix): lib/Platform/OSX/System/TcpListener.cpp $(IntermediateDirectory)/lib_Platform_OSX_System_TcpListener.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Platform/OSX/System/TcpListener.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Platform_OSX_System_TcpListener.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Platform_OSX_System_TcpListener.cpp$(DependSuffix): lib/Platform/OSX/System/TcpListener.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Platform_OSX_System_TcpListener.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Platform_OSX_System_TcpListener.cpp$(DependSuffix) -MM lib/Platform/OSX/System/TcpListener.cpp

$(IntermediateDirectory)/lib_Platform_OSX_System_TcpListener.cpp$(PreprocessSuffix): lib/Platform/OSX/System/TcpListener.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Platform_OSX_System_TcpListener.cpp$(PreprocessSuffix) lib/Platform/OSX/System/TcpListener.cpp

$(IntermediateDirectory)/lib_Platform_OSX_System_Dispatcher.cpp$(ObjectSuffix): lib/Platform/OSX/System/Dispatcher.cpp $(IntermediateDirectory)/lib_Platform_OSX_System_Dispatcher.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Platform/OSX/System/Dispatcher.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Platform_OSX_System_Dispatcher.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Platform_OSX_System_Dispatcher.cpp$(DependSuffix): lib/Platform/OSX/System/Dispatcher.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Platform_OSX_System_Dispatcher.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Platform_OSX_System_Dispatcher.cpp$(DependSuffix) -MM lib/Platform/OSX/System/Dispatcher.cpp

$(IntermediateDirectory)/lib_Platform_OSX_System_Dispatcher.cpp$(PreprocessSuffix): lib/Platform/OSX/System/Dispatcher.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Platform_OSX_System_Dispatcher.cpp$(PreprocessSuffix) lib/Platform/OSX/System/Dispatcher.cpp

$(IntermediateDirectory)/lib_Platform_OSX_System_Ipv4Resolver.cpp$(ObjectSuffix): lib/Platform/OSX/System/Ipv4Resolver.cpp $(IntermediateDirectory)/lib_Platform_OSX_System_Ipv4Resolver.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Platform/OSX/System/Ipv4Resolver.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Platform_OSX_System_Ipv4Resolver.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Platform_OSX_System_Ipv4Resolver.cpp$(DependSuffix): lib/Platform/OSX/System/Ipv4Resolver.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Platform_OSX_System_Ipv4Resolver.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Platform_OSX_System_Ipv4Resolver.cpp$(DependSuffix) -MM lib/Platform/OSX/System/Ipv4Resolver.cpp

$(IntermediateDirectory)/lib_Platform_OSX_System_Ipv4Resolver.cpp$(PreprocessSuffix): lib/Platform/OSX/System/Ipv4Resolver.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Platform_OSX_System_Ipv4Resolver.cpp$(PreprocessSuffix) lib/Platform/OSX/System/Ipv4Resolver.cpp

$(IntermediateDirectory)/lib_Platform_OSX_System_Timer.cpp$(ObjectSuffix): lib/Platform/OSX/System/Timer.cpp $(IntermediateDirectory)/lib_Platform_OSX_System_Timer.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Platform/OSX/System/Timer.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Platform_OSX_System_Timer.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Platform_OSX_System_Timer.cpp$(DependSuffix): lib/Platform/OSX/System/Timer.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Platform_OSX_System_Timer.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Platform_OSX_System_Timer.cpp$(DependSuffix) -MM lib/Platform/OSX/System/Timer.cpp

$(IntermediateDirectory)/lib_Platform_OSX_System_Timer.cpp$(PreprocessSuffix): lib/Platform/OSX/System/Timer.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Platform_OSX_System_Timer.cpp$(PreprocessSuffix) lib/Platform/OSX/System/Timer.cpp

$(IntermediateDirectory)/lib_Platform_OSX_System_TcpConnector.cpp$(ObjectSuffix): lib/Platform/OSX/System/TcpConnector.cpp $(IntermediateDirectory)/lib_Platform_OSX_System_TcpConnector.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/lib/Platform/OSX/System/TcpConnector.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/lib_Platform_OSX_System_TcpConnector.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/lib_Platform_OSX_System_TcpConnector.cpp$(DependSuffix): lib/Platform/OSX/System/TcpConnector.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/lib_Platform_OSX_System_TcpConnector.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/lib_Platform_OSX_System_TcpConnector.cpp$(DependSuffix) -MM lib/Platform/OSX/System/TcpConnector.cpp

$(IntermediateDirectory)/lib_Platform_OSX_System_TcpConnector.cpp$(PreprocessSuffix): lib/Platform/OSX/System/TcpConnector.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/lib_Platform_OSX_System_TcpConnector.cpp$(PreprocessSuffix) lib/Platform/OSX/System/TcpConnector.cpp

$(IntermediateDirectory)/build_CMakeFiles_3.10.2_CompilerIdCXX_CMakeCXXCompilerId.cpp$(ObjectSuffix): build/CMakeFiles/3.10.2/CompilerIdCXX/CMakeCXXCompilerId.cpp $(IntermediateDirectory)/build_CMakeFiles_3.10.2_CompilerIdCXX_CMakeCXXCompilerId.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/build/CMakeFiles/3.10.2/CompilerIdCXX/CMakeCXXCompilerId.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/build_CMakeFiles_3.10.2_CompilerIdCXX_CMakeCXXCompilerId.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/build_CMakeFiles_3.10.2_CompilerIdCXX_CMakeCXXCompilerId.cpp$(DependSuffix): build/CMakeFiles/3.10.2/CompilerIdCXX/CMakeCXXCompilerId.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/build_CMakeFiles_3.10.2_CompilerIdCXX_CMakeCXXCompilerId.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/build_CMakeFiles_3.10.2_CompilerIdCXX_CMakeCXXCompilerId.cpp$(DependSuffix) -MM build/CMakeFiles/3.10.2/CompilerIdCXX/CMakeCXXCompilerId.cpp

$(IntermediateDirectory)/build_CMakeFiles_3.10.2_CompilerIdCXX_CMakeCXXCompilerId.cpp$(PreprocessSuffix): build/CMakeFiles/3.10.2/CompilerIdCXX/CMakeCXXCompilerId.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/build_CMakeFiles_3.10.2_CompilerIdCXX_CMakeCXXCompilerId.cpp$(PreprocessSuffix) build/CMakeFiles/3.10.2/CompilerIdCXX/CMakeCXXCompilerId.cpp

$(IntermediateDirectory)/build_CMakeFiles_3.10.2_CompilerIdC_CMakeCCompilerId.c$(ObjectSuffix): build/CMakeFiles/3.10.2/CompilerIdC/CMakeCCompilerId.c $(IntermediateDirectory)/build_CMakeFiles_3.10.2_CompilerIdC_CMakeCCompilerId.c$(DependSuffix)
	$(CC) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/build/CMakeFiles/3.10.2/CompilerIdC/CMakeCCompilerId.c" $(CFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/build_CMakeFiles_3.10.2_CompilerIdC_CMakeCCompilerId.c$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/build_CMakeFiles_3.10.2_CompilerIdC_CMakeCCompilerId.c$(DependSuffix): build/CMakeFiles/3.10.2/CompilerIdC/CMakeCCompilerId.c
	@$(CC) $(CFLAGS) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/build_CMakeFiles_3.10.2_CompilerIdC_CMakeCCompilerId.c$(ObjectSuffix) -MF$(IntermediateDirectory)/build_CMakeFiles_3.10.2_CompilerIdC_CMakeCCompilerId.c$(DependSuffix) -MM build/CMakeFiles/3.10.2/CompilerIdC/CMakeCCompilerId.c

$(IntermediateDirectory)/build_CMakeFiles_3.10.2_CompilerIdC_CMakeCCompilerId.c$(PreprocessSuffix): build/CMakeFiles/3.10.2/CompilerIdC/CMakeCCompilerId.c
	$(CC) $(CFLAGS) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/build_CMakeFiles_3.10.2_CompilerIdC_CMakeCCompilerId.c$(PreprocessSuffix) build/CMakeFiles/3.10.2/CompilerIdC/CMakeCCompilerId.c

$(IntermediateDirectory)/build__3rdParty_Hunter_toolchain__builds_CMakeFiles_feature_tests.cxx$(ObjectSuffix): build/_3rdParty/Hunter/toolchain/_builds/CMakeFiles/feature_tests.cxx $(IntermediateDirectory)/build__3rdParty_Hunter_toolchain__builds_CMakeFiles_feature_tests.cxx$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/build/_3rdParty/Hunter/toolchain/_builds/CMakeFiles/feature_tests.cxx" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/build__3rdParty_Hunter_toolchain__builds_CMakeFiles_feature_tests.cxx$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/build__3rdParty_Hunter_toolchain__builds_CMakeFiles_feature_tests.cxx$(DependSuffix): build/_3rdParty/Hunter/toolchain/_builds/CMakeFiles/feature_tests.cxx
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/build__3rdParty_Hunter_toolchain__builds_CMakeFiles_feature_tests.cxx$(ObjectSuffix) -MF$(IntermediateDirectory)/build__3rdParty_Hunter_toolchain__builds_CMakeFiles_feature_tests.cxx$(DependSuffix) -MM build/_3rdParty/Hunter/toolchain/_builds/CMakeFiles/feature_tests.cxx

$(IntermediateDirectory)/build__3rdParty_Hunter_toolchain__builds_CMakeFiles_feature_tests.cxx$(PreprocessSuffix): build/_3rdParty/Hunter/toolchain/_builds/CMakeFiles/feature_tests.cxx
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/build__3rdParty_Hunter_toolchain__builds_CMakeFiles_feature_tests.cxx$(PreprocessSuffix) build/_3rdParty/Hunter/toolchain/_builds/CMakeFiles/feature_tests.cxx

$(IntermediateDirectory)/build__3rdParty_Hunter_toolchain__builds_CMakeFiles_feature_tests.c$(ObjectSuffix): build/_3rdParty/Hunter/toolchain/_builds/CMakeFiles/feature_tests.c $(IntermediateDirectory)/build__3rdParty_Hunter_toolchain__builds_CMakeFiles_feature_tests.c$(DependSuffix)
	$(CC) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/build/_3rdParty/Hunter/toolchain/_builds/CMakeFiles/feature_tests.c" $(CFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/build__3rdParty_Hunter_toolchain__builds_CMakeFiles_feature_tests.c$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/build__3rdParty_Hunter_toolchain__builds_CMakeFiles_feature_tests.c$(DependSuffix): build/_3rdParty/Hunter/toolchain/_builds/CMakeFiles/feature_tests.c
	@$(CC) $(CFLAGS) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/build__3rdParty_Hunter_toolchain__builds_CMakeFiles_feature_tests.c$(ObjectSuffix) -MF$(IntermediateDirectory)/build__3rdParty_Hunter_toolchain__builds_CMakeFiles_feature_tests.c$(DependSuffix) -MM build/_3rdParty/Hunter/toolchain/_builds/CMakeFiles/feature_tests.c

$(IntermediateDirectory)/build__3rdParty_Hunter_toolchain__builds_CMakeFiles_feature_tests.c$(PreprocessSuffix): build/_3rdParty/Hunter/toolchain/_builds/CMakeFiles/feature_tests.c
	$(CC) $(CFLAGS) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/build__3rdParty_Hunter_toolchain__builds_CMakeFiles_feature_tests.c$(PreprocessSuffix) build/_3rdParty/Hunter/toolchain/_builds/CMakeFiles/feature_tests.c

$(IntermediateDirectory)/build__3rdParty_Hunter_toolchain__builds_CMakeFiles_3.10.2_CompilerIdCXX_CMakeCXXCompilerId.cpp$(ObjectSuffix): build/_3rdParty/Hunter/toolchain/_builds/CMakeFiles/3.10.2/CompilerIdCXX/CMakeCXXCompilerId.cpp $(IntermediateDirectory)/build__3rdParty_Hunter_toolchain__builds_CMakeFiles_3.10.2_CompilerIdCXX_CMakeCXXCompilerId.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/build/_3rdParty/Hunter/toolchain/_builds/CMakeFiles/3.10.2/CompilerIdCXX/CMakeCXXCompilerId.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/build__3rdParty_Hunter_toolchain__builds_CMakeFiles_3.10.2_CompilerIdCXX_CMakeCXXCompilerId.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/build__3rdParty_Hunter_toolchain__builds_CMakeFiles_3.10.2_CompilerIdCXX_CMakeCXXCompilerId.cpp$(DependSuffix): build/_3rdParty/Hunter/toolchain/_builds/CMakeFiles/3.10.2/CompilerIdCXX/CMakeCXXCompilerId.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/build__3rdParty_Hunter_toolchain__builds_CMakeFiles_3.10.2_CompilerIdCXX_CMakeCXXCompilerId.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/build__3rdParty_Hunter_toolchain__builds_CMakeFiles_3.10.2_CompilerIdCXX_CMakeCXXCompilerId.cpp$(DependSuffix) -MM build/_3rdParty/Hunter/toolchain/_builds/CMakeFiles/3.10.2/CompilerIdCXX/CMakeCXXCompilerId.cpp

$(IntermediateDirectory)/build__3rdParty_Hunter_toolchain__builds_CMakeFiles_3.10.2_CompilerIdCXX_CMakeCXXCompilerId.cpp$(PreprocessSuffix): build/_3rdParty/Hunter/toolchain/_builds/CMakeFiles/3.10.2/CompilerIdCXX/CMakeCXXCompilerId.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/build__3rdParty_Hunter_toolchain__builds_CMakeFiles_3.10.2_CompilerIdCXX_CMakeCXXCompilerId.cpp$(PreprocessSuffix) build/_3rdParty/Hunter/toolchain/_builds/CMakeFiles/3.10.2/CompilerIdCXX/CMakeCXXCompilerId.cpp

$(IntermediateDirectory)/build__3rdParty_Hunter_toolchain__builds_CMakeFiles_3.10.2_CompilerIdC_CMakeCCompilerId.c$(ObjectSuffix): build/_3rdParty/Hunter/toolchain/_builds/CMakeFiles/3.10.2/CompilerIdC/CMakeCCompilerId.c $(IntermediateDirectory)/build__3rdParty_Hunter_toolchain__builds_CMakeFiles_3.10.2_CompilerIdC_CMakeCCompilerId.c$(DependSuffix)
	$(CC) $(SourceSwitch) "/home/exploshot/Dev/qwertycoin/build/_3rdParty/Hunter/toolchain/_builds/CMakeFiles/3.10.2/CompilerIdC/CMakeCCompilerId.c" $(CFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/build__3rdParty_Hunter_toolchain__builds_CMakeFiles_3.10.2_CompilerIdC_CMakeCCompilerId.c$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/build__3rdParty_Hunter_toolchain__builds_CMakeFiles_3.10.2_CompilerIdC_CMakeCCompilerId.c$(DependSuffix): build/_3rdParty/Hunter/toolchain/_builds/CMakeFiles/3.10.2/CompilerIdC/CMakeCCompilerId.c
	@$(CC) $(CFLAGS) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/build__3rdParty_Hunter_toolchain__builds_CMakeFiles_3.10.2_CompilerIdC_CMakeCCompilerId.c$(ObjectSuffix) -MF$(IntermediateDirectory)/build__3rdParty_Hunter_toolchain__builds_CMakeFiles_3.10.2_CompilerIdC_CMakeCCompilerId.c$(DependSuffix) -MM build/_3rdParty/Hunter/toolchain/_builds/CMakeFiles/3.10.2/CompilerIdC/CMakeCCompilerId.c

$(IntermediateDirectory)/build__3rdParty_Hunter_toolchain__builds_CMakeFiles_3.10.2_CompilerIdC_CMakeCCompilerId.c$(PreprocessSuffix): build/_3rdParty/Hunter/toolchain/_builds/CMakeFiles/3.10.2/CompilerIdC/CMakeCCompilerId.c
	$(CC) $(CFLAGS) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/build__3rdParty_Hunter_toolchain__builds_CMakeFiles_3.10.2_CompilerIdC_CMakeCCompilerId.c$(PreprocessSuffix) build/_3rdParty/Hunter/toolchain/_builds/CMakeFiles/3.10.2/CompilerIdC/CMakeCCompilerId.c


-include $(IntermediateDirectory)/*$(DependSuffix)
##
## Clean
##
clean:
	$(RM) -r ./Debug/


