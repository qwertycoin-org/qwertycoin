// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2016, The Forknote developers
// Copyright (c) 2016-2018, The Karbowanec developers
// Copyright (c) 2018-2021, The Qwertycoin Group.
//
// This file is part of Qwertycoin.
//
// Qwertycoin is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Qwertycoin is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Qwertycoin.  If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include <functional>
#include <unordered_map>
#include <Common/Math.h>
#include <CryptoNoteCore/ITransaction.h>
#include <Global/Constants.h>
#include <Logging/LoggerRef.h>
#include <Rpc/CoreRpcServerCommandsDefinitions.h>
#include <Rpc/HttpServer.h>

using namespace Qwertycoin;

namespace CryptoNote {

	class core;

	class NodeServer;

	class BlockchainExplorer;

	class ICryptoNoteProtocolQuery;

	class RpcServer : public HttpServer
	{
		template<class Handler>
		struct RpcHandler
		{
			const Handler handler;
			const bool allowBusyCore;
		};

	public:
		typedef std::function<
				bool(RpcServer *, const HttpRequest &request, HttpResponse &response)
		> HandlerFunction;

		RpcServer(
				System::Dispatcher &dispatcher,
				Logging::ILogger &log,
				core &c,
				NodeServer &p2p,
				const ICryptoNoteProtocolQuery &protocolQuery);

		bool restrictRPC(const bool is_resctricted);

		bool enableCors(const std::string domain);

		bool setFeeAddress(const std::string &fee_address, const AccountPublicAddress &fee_acc);

		bool setViewKey(const std::string &view_key);

		bool setContactInfo(const std::string &contact);

		bool masternodeCheckIncomingTx(const BinaryArray &tx_blob);

		std::string getCorsDomain();

	private:
		void processRequest(const HttpRequest &request, HttpResponse &response) override;

		bool processJsonRpcRequest(const HttpRequest &request, HttpResponse &response);

		bool isCoreReady();

		// binary handlers
		bool onGetBlocks(
				const COMMAND_RPC_GET_BLOCKS_FAST::request &req,
				COMMAND_RPC_GET_BLOCKS_FAST::response &res);

		bool onQueryBlocks(
				const COMMAND_RPC_QUERY_BLOCKS::request &req,
				COMMAND_RPC_QUERY_BLOCKS::response &res);

		bool onQueryBlocksLite(
				const COMMAND_RPC_QUERY_BLOCKS_LITE::request &req,
				COMMAND_RPC_QUERY_BLOCKS_LITE::response &res);

		bool onQueryBlocksDetailed(
				const COMMAND_RPC_QUERY_BLOCKS_DETAILED::request &req,
				COMMAND_RPC_QUERY_BLOCKS_DETAILED::response &res);

		bool onGetIndexes(
				const COMMAND_RPC_GET_TX_GLOBAL_OUTPUTS_INDEXES::request &req,
				COMMAND_RPC_GET_TX_GLOBAL_OUTPUTS_INDEXES::response &res);

		bool onGetRandomOuts(
				const COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::request &req,
				COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::response &res);

		bool onGetPoolChanges(
				const COMMAND_RPC_GET_POOL_CHANGES::request &req,
				COMMAND_RPC_GET_POOL_CHANGES::response &rsp);

		bool onGetPoolChangesLite(
				const COMMAND_RPC_GET_POOL_CHANGES_LITE::request &req,
				COMMAND_RPC_GET_POOL_CHANGES_LITE::response &rsp);

		// http handlers
		bool onGetIndex(
				const COMMAND_HTTP::request &req, COMMAND_HTTP::response &res);

		bool onGetSupply(
				const COMMAND_HTTP::request &req, COMMAND_HTTP::response &res);

		bool onGetPaymentId(
				const COMMAND_HTTP::request &req, COMMAND_HTTP::response &res);

		// json handlers
		bool onGetInfo(
				const COMMAND_RPC_GET_INFO::request &req,
				COMMAND_RPC_GET_INFO::response &res);

		bool onGetVersion(
				const COMMAND_RPC_GET_VERSION::request &req,
				COMMAND_RPC_GET_VERSION::response &res);

		bool onGetHardwareInfo(
				const COMMAND_RPC_GET_HARDWARE_INFO::request &req,
				COMMAND_RPC_GET_HARDWARE_INFO::response &res);

		bool onGetHeight(
				const COMMAND_RPC_GET_HEIGHT::request &req,
				COMMAND_RPC_GET_HEIGHT::response &res);

		bool onGetTransactions(
				const COMMAND_RPC_GET_TRANSACTIONS::request &req,
				COMMAND_RPC_GET_TRANSACTIONS::response &res);

		bool onGetTransactionsByHeights(
				const COMMAND_RPC_GET_TRANSACTIONS_BY_HEIGHTS::request &req,
				COMMAND_RPC_GET_TRANSACTIONS_BY_HEIGHTS::response &res);

		bool onGetRawTransactionsByHeights(
				const COMMAND_RPC_GET_RAW_TRANSACTIONS_BY_HEIGHTS::request &req,
				COMMAND_RPC_GET_RAW_TRANSACTIONS_BY_HEIGHTS::response &res);

		bool onGetRawTransactionPool(
				const COMMAND_RPC_GET_RAW_TRANSACTIONS_FROM_POOL::request &req,
				COMMAND_RPC_GET_RAW_TRANSACTIONS_FROM_POOL::response &res);

		bool onSendRawTx(
				const COMMAND_RPC_SEND_RAW_TX::request &req,
				COMMAND_RPC_SEND_RAW_TX::response &res);

		bool onStartMining(
				const COMMAND_RPC_START_MINING::request &req,
				COMMAND_RPC_START_MINING::response &res);

		bool onStopMining(
				const COMMAND_RPC_STOP_MINING::request &req,
				COMMAND_RPC_STOP_MINING::response &res);

		bool onStopDaemon(
				const COMMAND_RPC_STOP_DAEMON::request &req,
				COMMAND_RPC_STOP_DAEMON::response &res);

		bool onGetFeeAddress(
				const COMMAND_RPC_GET_FEE_ADDRESS::request &req,
				COMMAND_RPC_GET_FEE_ADDRESS::response &res);

		bool onGetPeerList(
				const COMMAND_RPC_GET_PEER_LIST::request &req,
				COMMAND_RPC_GET_PEER_LIST::response &res);

		bool onGetBlocksDetailsByHeights(
				const COMMAND_RPC_GET_BLOCKS_DETAILS_BY_HEIGHTS::request &req,
				COMMAND_RPC_GET_BLOCKS_DETAILS_BY_HEIGHTS::response &rsp);

		bool onGetBlocksDetailsByHashes(
				const COMMAND_RPC_GET_BLOCKS_DETAILS_BY_HASHES::request &req,
				COMMAND_RPC_GET_BLOCKS_DETAILS_BY_HASHES::response &rsp);

		bool onGetBlockDetailsByHeight(
				const COMMAND_RPC_GET_BLOCK_DETAILS_BY_HEIGHT::request &req,
				COMMAND_RPC_GET_BLOCK_DETAILS_BY_HEIGHT::response &rsp);

		bool onGetBlockDetailsByHash(
				const COMMAND_RPC_GET_BLOCK_DETAILS_BY_HASH::request &req,
				COMMAND_RPC_GET_BLOCK_DETAILS_BY_HASH::response &rsp);

		bool onGetBlocksHashesByTimestamps(
				const COMMAND_RPC_GET_BLOCKS_HASHES_BY_TIMESTAMPS::request &req,
				COMMAND_RPC_GET_BLOCKS_HASHES_BY_TIMESTAMPS::response &rsp);

		bool onGetTransactionsDetailsByHashes(
				const COMMAND_RPC_GET_TRANSACTIONS_DETAILS_BY_HASHES::request &req,
				COMMAND_RPC_GET_TRANSACTIONS_DETAILS_BY_HASHES::response &rsp);

		bool onGetTransactionDetailsByHash(
				const COMMAND_RPC_GET_TRANSACTION_DETAILS_BY_HASH::request &req,
				COMMAND_RPC_GET_TRANSACTION_DETAILS_BY_HASH::response &rsp);

		bool onGetTransactionHashesByPaymentId(
				const COMMAND_RPC_GET_TRANSACTION_HASHES_BY_PAYMENT_ID::request &req,
				COMMAND_RPC_GET_TRANSACTION_HASHES_BY_PAYMENT_ID::response &rsp);

		// json rpc
		bool onGetBlockCount(
				const COMMAND_RPC_GET_BLOCK_COUNT::request &req,
				COMMAND_RPC_GET_BLOCK_COUNT::response &res);

		bool onGetBlockHash(
				const COMMAND_RPC_GET_BLOCK_HASH::request &req,
				COMMAND_RPC_GET_BLOCK_HASH::response &res);

		bool onGetBlockTemplate(
				const COMMAND_RPC_GET_BLOCK_TEMPLATE::request &req,
				COMMAND_RPC_GET_BLOCK_TEMPLATE::response &res);

		bool onGetCurrencyId(
				const COMMAND_RPC_GET_CURRENCY_ID::request &,
				COMMAND_RPC_GET_CURRENCY_ID::response &res);

		bool onSubmitBlock(
				const COMMAND_RPC_SUBMIT_BLOCK::request &req,
				COMMAND_RPC_SUBMIT_BLOCK::response &res);

		bool onGetLastBlockHeader(
				const COMMAND_RPC_GET_LAST_BLOCK_HEADER::request &req,
				COMMAND_RPC_GET_LAST_BLOCK_HEADER::response &res);

		bool onGetBlockHeaderByHash(
				const COMMAND_RPC_GET_BLOCK_HEADER_BY_HASH::request &req,
				COMMAND_RPC_GET_BLOCK_HEADER_BY_HASH::response &res);

		bool onGetBlockHeaderByHeight(
				const COMMAND_RPC_GET_BLOCK_HEADER_BY_HEIGHT::request &req,
				COMMAND_RPC_GET_BLOCK_HEADER_BY_HEIGHT::response &res);

		void fillBlockHeaderResponse(
				const Block &blk,
				bool orphan_status,
				uint64_t height,
				const Crypto::Hash &hash,
				BLOCK_HEADER_RESPONSE_ENTRY &responseEntry);

		bool onBlocksListJson(
				const COMMAND_RPC_GET_BLOCKS_LIST::request &req,
				COMMAND_RPC_GET_BLOCKS_LIST::response &res);

		bool onBlockJson(
				const COMMAND_RPC_GET_BLOCK_DETAILS::request &req,
				COMMAND_RPC_GET_BLOCK_DETAILS::response &res);

		bool onTransactionJson(
				const COMMAND_RPC_GET_TRANSACTION_DETAILS::request &req,
				COMMAND_RPC_GET_TRANSACTION_DETAILS::response &res);

		bool onPoolJson(
				const COMMAND_RPC_GET_POOL::request &req,
				COMMAND_RPC_GET_POOL::response &res);

		bool onTransactionsPoolJson(
				const COMMAND_RPC_GET_POOL::request &req,
				COMMAND_RPC_GET_POOL::response &res);

		bool onMempoolJson(
				const COMMAND_RPC_GET_MEMPOOL::request &req,
				COMMAND_RPC_GET_MEMPOOL::response &res);

		bool onTransactionsByPaymentId(
				const COMMAND_RPC_GET_TRANSACTIONS_BY_PAYMENT_ID::request &req,
				COMMAND_RPC_GET_TRANSACTIONS_BY_PAYMENT_ID::response &res);

		bool onCheckTxKey(
				const COMMAND_RPC_CHECK_TX_KEY::request &req,
				COMMAND_RPC_CHECK_TX_KEY::response &res);

		bool onCheckTxWithViewKey(
				const COMMAND_RPC_CHECK_TX_WITH_PRIVATE_VIEW_KEY::request &req,
				COMMAND_RPC_CHECK_TX_WITH_PRIVATE_VIEW_KEY::response &res);

		bool onCheckTxProof(
				const COMMAND_RPC_CHECK_TX_PROOF::request &req,
				COMMAND_RPC_CHECK_TX_PROOF::response &res);

		bool onCheckReserveProof(
				const COMMAND_RPC_CHECK_RESERVE_PROOF::request &req,
				COMMAND_RPC_CHECK_RESERVE_PROOF::response &res);

		bool onValidateAddress(
				const COMMAND_RPC_VALIDATE_ADDRESS::request &req,
				COMMAND_RPC_VALIDATE_ADDRESS::response &res);

		bool onVerifyMessage(
				const COMMAND_RPC_VERIFY_MESSAGE::request &req,
				COMMAND_RPC_VERIFY_MESSAGE::response &res);

		bool onGetDifficultyStat(
				const COMMAND_RPC_GET_DIFFICULTY_STAT::request &req,
				COMMAND_RPC_GET_DIFFICULTY_STAT::response &res);

		bool onGetMixin(const Transaction &transaction, uint64_t &mixin);

	private:
		static std::unordered_map<std::string, RpcHandler<HandlerFunction>> s_handlers;
		Logging::LoggerRef logger;
		core &m_core;
		NodeServer &m_p2p;
		const ICryptoNoteProtocolQuery &m_protocolQuery;
		bool m_restricted_rpc;
		std::string m_cors_domain;
		std::string m_fee_address;
		std::string m_contact_info;
		Crypto::SecretKey m_view_key = NULL_SECRET_KEY;
		AccountPublicAddress m_fee_acc;
	};

} // namespace CryptoNote
