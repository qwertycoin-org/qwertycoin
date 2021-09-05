// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
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

#include <string>
#include <future>
#include <unordered_map>
#include <BlockchainExplorer/BlockchainExplorerData.h>
#include <Common/StringTools.h>
#include <Common/Base58.h>
#include <Common/HardwareInfo.h>
#include <Common/Math.h>
#include <CryptoNoteCore/TransactionUtils.h>
#include <CryptoNoteCore/CryptoNoteTools.h>
#include <CryptoNoteCore/CryptoNoteFormatUtils.h>
#include <CryptoNoteCore/Core.h>
#include <CryptoNoteCore/IBlock.h>
#include <CryptoNoteCore/Miner.h>
#include <CryptoNoteCore/TransactionExtra.h>
#include <CryptoNoteProtocol/ICryptoNoteProtocolQuery.h>
#include <P2p/NetNode.h>
#include <Rpc/CoreRpcServerErrorCodes.h>
#include <Rpc/JsonRpc.h>
#include <Rpc/RpcServer.h>
#include <version.h>

#undef ERROR // TODO: WTF!?

const uint32_t MAX_NUMBER_OF_BLOCKS_PER_STATS_REQUEST = 10000;
const uint64_t BLOCK_LIST_MAX_COUNT = 1000;

using namespace Crypto;
using namespace Common;
using namespace Logging;

static const Crypto::SecretKey I = {{0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
											0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
											0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
											0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};

namespace CryptoNote {

	namespace {

		template<typename Command>
		RpcServer::HandlerFunction binMethod(bool (RpcServer::*handler)(typename Command::request const &,
																		typename Command::response &))
		{
			return [handler](RpcServer *obj, const HttpRequest &request, HttpResponse &response) {
				boost::value_initialized<typename Command::request> req;
				boost::value_initialized<typename Command::response> res;

				if (!loadFromBinaryKeyValue(static_cast<typename Command::request &>(req),
											request.getBody())) {
					return false;
				}

				bool result = (obj->*handler)(req, res);
				response.setBody(storeToBinaryKeyValue(res.data()));

				return result;
			};
		}

		template<typename Command>
		RpcServer::HandlerFunction jsonMethod(bool (RpcServer::*handler)(typename Command::request const &,
																		 typename Command::response &))
		{
			return [handler](RpcServer *obj, const HttpRequest &request, HttpResponse &response) {
				boost::value_initialized<typename Command::request> req;
				boost::value_initialized<typename Command::response> res;

				if (!loadFromJson(static_cast<typename Command::request &>(req), request.getBody())) {
					return false;
				}

				bool result = (obj->*handler)(req, res);
				std::string cors_domain = obj->getCorsDomain();
				if (!cors_domain.empty()) {
					response.addHeader("Access-Control-Allow-Origin", cors_domain);
					response.addHeader("Access-Control-Allow-Headers",
									   "Origin, X-Requested-With, Content-Type, Accept");
					response.addHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
				}
				response.addHeader("Content-Type", "application/json");
				response.setBody(storeToJson(res.data()));

				return result;
			};
		}

		template<typename Command>
		RpcServer::HandlerFunction httpMethod(bool (RpcServer::*handler)(typename Command::request const &,
																		 typename Command::response &))
		{
			return [handler](RpcServer *obj, const HttpRequest &request, HttpResponse &response) {
				boost::value_initialized<typename Command::request> req;
				boost::value_initialized<typename Command::response> res;

				if (!loadFromJson(static_cast<typename Command::request &>(req), request.getBody())) {
					return false;
				}

				bool result = (obj->*handler)(req, res);

				std::string cors_domain = obj->getCorsDomain();
				if (!cors_domain.empty()) {
					response.addHeader("Access-Control-Allow-Origin", cors_domain);
					response.addHeader("Access-Control-Allow-Headers",
									   "Origin, X-Requested-With, Content-Type, Accept");
					response.addHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
				}
				response.addHeader("Content-Type", "text/html; charset=UTF-8");
				response.addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
				response.addHeader("Expires", "0");
				response.setStatus(HttpResponse::HTTP_STATUS::STATUS_200);

				response.setBody(res);

				return result;
			};
		}

	} // namespace

	std::unordered_map<std::string, RpcServer::RpcHandler<RpcServer::HandlerFunction>>
			RpcServer::s_handlers = {
			// binary handlers
			{"/getblocks.bin",
			 {binMethod<COMMAND_RPC_GET_BLOCKS_FAST>(
					 &RpcServer::onGetBlocks), false}},
			{"/queryblocks.bin",
			 {binMethod<COMMAND_RPC_QUERY_BLOCKS>(
					 &RpcServer::onQueryBlocks), false}},
			{"/queryblockslite.bin",
			 {binMethod<COMMAND_RPC_QUERY_BLOCKS_LITE>(
					 &RpcServer::onQueryBlocksLite),
			  false}},

			{"/get_o_indexes.bin",
			 {binMethod<COMMAND_RPC_GET_TX_GLOBAL_OUTPUTS_INDEXES>(
					 &RpcServer::onGetIndexes),
			  false}},
			{"/getrandom_outs.bin",
			 {binMethod<COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS>(
					 &RpcServer::onGetRandomOuts),
			  false}},
			{"/get_pool_changes.bin",
			 {binMethod<COMMAND_RPC_GET_POOL_CHANGES>(
					 &RpcServer::onGetPoolChanges), false}},
			{"/get_pool_changes_lite.bin",
			 {binMethod<COMMAND_RPC_GET_POOL_CHANGES_LITE>(
					 &RpcServer::onGetPoolChangesLite),
			  false}},

			// http handlers
			{"/", {httpMethod<COMMAND_HTTP>(
					&RpcServer::onGetIndex), true}},
			{"/supply", {httpMethod<COMMAND_HTTP>(
					&RpcServer::onGetSupply), false}},
			{"/paymentid", {httpMethod<COMMAND_HTTP>(
					&RpcServer::onGetPaymentId), false}},

			// json handlers
			{"/getinfo", {jsonMethod<COMMAND_RPC_GET_INFO>(
					&RpcServer::onGetInfo), true}},
			{"/getversion", {jsonMethod<COMMAND_RPC_GET_VERSION>(
					&RpcServer::onGetVersion), true}},
			{"/gethardwareinfo", {jsonMethod<COMMAND_RPC_GET_HARDWARE_INFO>(
					&RpcServer::onGetHardwareInfo), true}},
			{"/getheight",
			 {jsonMethod<COMMAND_RPC_GET_HEIGHT>(
					 &RpcServer::onGetHeight), true}},
			{"/gettransactions",
			 {jsonMethod<COMMAND_RPC_GET_TRANSACTIONS>(
					 &RpcServer::onGetTransactions),
			  false}},
			{"/sendrawtransaction",
			 {jsonMethod<COMMAND_RPC_SEND_RAW_TX>(
					 &RpcServer::onSendRawTx), false}},
			{"/feeaddress",
			 {jsonMethod<COMMAND_RPC_GET_FEE_ADDRESS>(
					 &RpcServer::onGetFeeAddress), true}},
			{"/peers",
			 {jsonMethod<COMMAND_RPC_GET_PEER_LIST>(
					 &RpcServer::onGetPeerList), true}},
			{"/get_mempool",
			 {jsonMethod<COMMAND_RPC_GET_POOL>(&RpcServer::onTransactionsPoolJson),
			  false}},
			{"/get_mempool_detailed",
			 {jsonMethod<COMMAND_RPC_GET_MEMPOOL>(
					 &RpcServer::onMempoolJson), false}},
			{"/getpeers",
			 {jsonMethod<COMMAND_RPC_GET_PEER_LIST>(
					 &RpcServer::onGetPeerList), true}},
			{"/getblocks",
			 {jsonMethod<COMMAND_RPC_GET_BLOCKS_FAST>(
					 &RpcServer::onGetBlocks), false}},

			{"/queryblocks",
			 {jsonMethod<COMMAND_RPC_QUERY_BLOCKS>(
					 &RpcServer::onQueryBlocks), false}},
			{"/queryblockslite",
			 {jsonMethod<COMMAND_RPC_QUERY_BLOCKS_LITE>(
					 &RpcServer::onQueryBlocksLite),
			  false}},
			{"/queryblocksdetailed",
			 {jsonMethod<COMMAND_RPC_QUERY_BLOCKS_DETAILED>(
					 &RpcServer::onQueryBlocksDetailed),
			  false}},

			{"/get_o_indexes",
			 {jsonMethod<COMMAND_RPC_GET_TX_GLOBAL_OUTPUTS_INDEXES>(
					 &RpcServer::onGetIndexes),
			  false}},
			{"/getrandom_outs",
			 {jsonMethod<COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS>(
					 &RpcServer::onGetRandomOuts),
			  false}},
			{"/get_pool_changes",
			 {jsonMethod<COMMAND_RPC_GET_POOL_CHANGES>(
					 &RpcServer::onGetPoolChanges), false}},
			{"/get_pool_changes_lite",
			 {jsonMethod<COMMAND_RPC_GET_POOL_CHANGES_LITE>(
					 &RpcServer::onGetPoolChangesLite),
			  false}},
			{"/get_block_details_by_height",
			 {jsonMethod<COMMAND_RPC_GET_BLOCK_DETAILS_BY_HEIGHT>(
					 &RpcServer::onGetBlockDetailsByHeight),
			  false}},
			{"/get_block_details_by_hash",
			 {jsonMethod<COMMAND_RPC_GET_BLOCK_DETAILS_BY_HASH>(
					 &RpcServer::onGetBlockDetailsByHash),
			  true}},
			{"/get_blocks_details_by_heights",
			 {jsonMethod<COMMAND_RPC_GET_BLOCKS_DETAILS_BY_HEIGHTS>(
					 &RpcServer::onGetBlocksDetailsByHeights),
			  false}},
			{"/get_blocks_details_by_hashes",
			 {jsonMethod<COMMAND_RPC_GET_BLOCKS_DETAILS_BY_HASHES>(
					 &RpcServer::onGetBlocksDetailsByHashes),
			  false}},
			{"/get_blocks_hashes_by_timestamps",
			 {jsonMethod<COMMAND_RPC_GET_BLOCKS_HASHES_BY_TIMESTAMPS>(
					 &RpcServer::onGetBlocksHashesByTimestamps),
			  false}},
			{"/get_transaction_details_by_hashes",
			 {jsonMethod<COMMAND_RPC_GET_TRANSACTIONS_DETAILS_BY_HASHES>(
					 &RpcServer::onGetTransactionsDetailsByHashes),
			  false}},
			{"/get_transaction_details_by_hash",
			 {jsonMethod<COMMAND_RPC_GET_TRANSACTION_DETAILS_BY_HASH>(
					 &RpcServer::onGetTransactionDetailsByHash),
			  true}},
			{"/get_transactions_by_heights",
			 {jsonMethod<COMMAND_RPC_GET_TRANSACTIONS_BY_HEIGHTS>(
					 &RpcServer::onGetTransactionsByHeights),
			  false}},
			{"/get_raw_transactions_by_heights", {jsonMethod<COMMAND_RPC_GET_RAW_TRANSACTIONS_BY_HEIGHTS>(
					&RpcServer::onGetRawTransactionsByHeights), false}},
			{"/get_raw_transactions_from_pool", {jsonMethod<COMMAND_RPC_GET_RAW_TRANSACTIONS_FROM_POOL>(
					&RpcServer::onGetRawTransactionPool), false}},
			{"/get_transaction_hashes_by_payment_id",
			 {jsonMethod<COMMAND_RPC_GET_TRANSACTION_HASHES_BY_PAYMENT_ID>(
					 &RpcServer::onGetTransactionHashesByPaymentId),
			  false}},

			// disabled in restricted rpc mode
			{"/start_mining",
			 {jsonMethod<COMMAND_RPC_START_MINING>(
					 &RpcServer::onStartMining), false}},
			{"/stop_mining",
			 {jsonMethod<COMMAND_RPC_STOP_MINING>(
					 &RpcServer::onStopMining), false}},
			{"/stop_daemon",
			 {jsonMethod<COMMAND_RPC_STOP_DAEMON>(
					 &RpcServer::onStopDaemon), true}},
			{"/get_difficulty_stat",
			 {jsonMethod<COMMAND_RPC_GET_DIFFICULTY_STAT>(
					 &RpcServer::onGetDifficultyStat),
			  false}},

			// json rpc
			{"/json_rpc",
			 {std::bind(&RpcServer::processJsonRpcRequest, std::placeholders::_1,
						std::placeholders::_2, std::placeholders::_3),
			  true}}
	};

	RpcServer::RpcServer(System::Dispatcher &dispatcher, Logging::ILogger &log, core &c,
						 NodeServer &p2p, const ICryptoNoteProtocolQuery &protocolQuery)
			: HttpServer(dispatcher, log),
			  logger(log, "RpcServer"),
			  m_core(c),
			  m_p2p(p2p),
			  m_protocolQuery(protocolQuery)
	{
	}

	void RpcServer::processRequest(const HttpRequest &request, HttpResponse &response)
	{
		logger(TRACE) << "RPC request came: \n" << request << std::endl;

		/*
		 * Greetings to Aivve from the Karbowanec project for this idea.
		 */
		try {
			auto url = request.getUrl();

			auto it = s_handlers.find(url);
			if (it == s_handlers.end()) {
				if (Common::starts_with(url, "/api/")) {
					// blocks
					std::string block_height_method = "/api/block/height/";
					std::string block_hash_method = "/api/block/hash/";
					// transactions
					std::string tx_height_method = "/api/transaction/height/";
					std::string tx_hash_method = "/api/transaction/hash/";
					std::string tx_mempool_method = "/api/mempool";
					std::string tx_mempool_detailed_method = "/api/mempool/detailed";
					std::string payment_id_method = "/api/payment_id/";
					// tools
					std::string version_method = "/api/tools/version";
					std::string hardware_info_method = "/api/tools/hardwareinfo";

					if (Common::starts_with(url, block_height_method)) {
						std::string height_str = url.substr(block_height_method.size());
						uint32_t height = Common::integer_cast<uint32_t>(height_str);
						auto it = s_handlers.find("/get_block_details_by_height");
						if (!it->second.allowBusyCore && !isCoreReady()) {
							response.setStatus(HttpResponse::STATUS_500);
							response.setBody("Core is busy");
							return;
						}
						COMMAND_RPC_GET_BLOCK_DETAILS_BY_HEIGHT::request req;
						req.blockHeight = height;
						COMMAND_RPC_GET_BLOCK_DETAILS_BY_HEIGHT::response rsp;
						bool r = onGetBlockDetailsByHeight(req, rsp);
						if (r) {
							response.addHeader("Content-Type", "application/json");
							response.setStatus(HttpResponse::HTTP_STATUS::STATUS_200);
							response.setBody(storeToJson(rsp));
						}
						else {
							response.setStatus(HttpResponse::STATUS_500);
							response.setBody("Internal error");
						}
						return;
					}
					else if (Common::starts_with(url, block_hash_method)) {
						std::string hash_str = url.substr(block_hash_method.size());

						auto it = s_handlers.find("/get_block_details_by_hash");
						if (!it->second.allowBusyCore && !isCoreReady()) {
							response.setStatus(HttpResponse::STATUS_500);
							response.setBody("Core is busy");
							return;
						}
						COMMAND_RPC_GET_BLOCK_DETAILS_BY_HASH::request req;
						req.hash = hash_str;
						COMMAND_RPC_GET_BLOCK_DETAILS_BY_HASH::response rsp;
						bool r = onGetBlockDetailsByHash(req, rsp);
						if (r) {
							response.addHeader("Content-Type", "application/json");
							response.setStatus(HttpResponse::HTTP_STATUS::STATUS_200);
							response.setBody(storeToJson(rsp));
						}
						else {
							response.setStatus(HttpResponse::STATUS_500);
							response.setBody("Internal error");
						}
						return;
					}
					else if (Common::starts_with(url, tx_height_method)) {
						std::string hash_str = url.substr(tx_height_method.size());
						uint32_t tempInt = std::stoi(hash_str);
						auto it = s_handlers.find("/get_transactions_by_heights");
						if (!it->second.allowBusyCore && !isCoreReady()) {
							response.setStatus(HttpResponse::STATUS_500);
							response.setBody("Core is busy");
							return;
						}

						Block blk;
						std::vector<Crypto::Hash> vh;
						uint32_t upperBorder = std::min(tempInt, m_core.get_current_blockchain_height());
						Crypto::Hash blockHash = m_core.getBlockIdByHeight(upperBorder);

						if (!m_core.getBlockByHash(blockHash, blk)) {
							throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_INTERNAL_ERROR,
														"Internal error: can't get block by hash. Hash = "
														+ podToHex(blockHash) + '.'};
						}

						if (blk.baseTransaction.inputs.front().type() != typeid(BaseInput)) {
							throw JsonRpc::JsonRpcError{
									CORE_RPC_ERROR_CODE_INTERNAL_ERROR,
									"Internal error: coinbase transaction in the block has the wrong type"
							};
						}

						for (Crypto::Hash &bTxs : blk.transactionHashes) {
							vh.push_back(bTxs);
						}

						vh.push_back(getObjectHash(blk.baseTransaction));

						std::list<Crypto::Hash> missedTxs;
						std::list<Transaction> txs;

						m_core.getTransactions(vh, txs, missedTxs, true);

						logger(DEBUGGING) << "Found " << txs.size() << "/" << vh.size()
										  << " transactions on the blockchain.";

						TransactionDetails2 transactionsDetails;

						bool r = m_core.fillTransactionDetails(txs.back(), transactionsDetails);
						if (r) {
							response.addHeader("Content-Type", "application/json");
							response.setStatus(HttpResponse::HTTP_STATUS::STATUS_200);
							response.setBody(storeToJson(transactionsDetails));
						}
						else {
							response.setStatus(HttpResponse::STATUS_500);
							response.setBody("Internal error");
							throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_INTERNAL_ERROR,
														"Internal error: can't fill transaction details."};
						}

						return;
					}
					else if (Common::starts_with(url, tx_hash_method)) {
						std::string hash_str = url.substr(tx_hash_method.size());
						auto it = s_handlers.find("/get_transaction_details_by_hash");
						if (!it->second.allowBusyCore && !isCoreReady()) {
							response.setStatus(HttpResponse::STATUS_500);
							response.setBody("Core is busy");
							return;
						}
						COMMAND_RPC_GET_TRANSACTION_DETAILS_BY_HASH::request req;
						req.hash = hash_str;
						COMMAND_RPC_GET_TRANSACTION_DETAILS_BY_HASH::response rsp;
						bool r = onGetTransactionDetailsByHash(req, rsp);
						if (r) {
							response.addHeader("Content-Type", "application/json");
							response.setStatus(HttpResponse::HTTP_STATUS::STATUS_200);
							response.setBody(storeToJson(rsp));
						}
						else {
							response.setStatus(HttpResponse::STATUS_500);
							response.setBody("Internal error");
						}
						return;

					}
					else if (Common::starts_with(url, tx_mempool_method)) {
						auto it = s_handlers.find("/get_mempool");
						if (!it->second.allowBusyCore && !isCoreReady()) {
							response.setStatus(HttpResponse::STATUS_500);
							response.setBody("Core is busy");
							return;
						}
						COMMAND_RPC_GET_POOL::request req;
						COMMAND_RPC_GET_POOL::response rsp;
						bool r = onTransactionsPoolJson(req, rsp);
						if (r) {
							response.addHeader("Content-Type", "application/json");
							response.setStatus(HttpResponse::HTTP_STATUS::STATUS_200);
							response.setBody(storeToJson(rsp));
						}
						else {
							response.setStatus(HttpResponse::STATUS_500);
							response.setBody("Internal error");
						}

						return;
					}
					else if (Common::starts_with(url, tx_mempool_detailed_method)) {
						auto it = s_handlers.find("/get_mempool_detailed");
						if (!it->second.allowBusyCore && !isCoreReady()) {
							response.setStatus(HttpResponse::STATUS_500);
							response.setBody("Core is busy");
							return;
						}
						COMMAND_RPC_GET_MEMPOOL::request req;
						COMMAND_RPC_GET_MEMPOOL::response rsp;
						bool r = onMempoolJson(req, rsp);
						if (r) {
							response.addHeader("Content-Type", "application/json");
							response.setStatus(HttpResponse::HTTP_STATUS::STATUS_200);
							response.setBody(storeToJson(rsp));
						}
						else {
							response.setStatus(HttpResponse::STATUS_500);
							response.setBody("Internal error");
						}

						return;
					}
					else if (Common::starts_with(url, payment_id_method)) {
						std::string pid_str = url.substr(payment_id_method.size());

						auto it = s_handlers.find("/get_transaction_hashes_by_payment_id");
						if (!it->second.allowBusyCore && !isCoreReady()) {
							response.setStatus(HttpResponse::STATUS_500);
							response.setBody("Core is busy");
							return;
						}
						COMMAND_RPC_GET_TRANSACTION_HASHES_BY_PAYMENT_ID::request req;
						req.paymentId = pid_str;
						COMMAND_RPC_GET_TRANSACTION_HASHES_BY_PAYMENT_ID::response rsp;
						bool r = onGetTransactionHashesByPaymentId(req, rsp);
						if (r) {
							response.addHeader("Content-Type", "application/json");
							response.setStatus(HttpResponse::HTTP_STATUS::STATUS_200);
							response.setBody(storeToJson(rsp));
						}
						else {
							response.setStatus(HttpResponse::STATUS_500);
							response.setBody("Internal error");
						}
						return;
					}
					else if (Common::starts_with(url, version_method)) {
						auto it = s_handlers.find("/getversion");
						if (!it->second.allowBusyCore && !isCoreReady()) {
							response.setStatus(HttpResponse::STATUS_500);
							response.setBody("Core is busy");
							return;
						}
						COMMAND_RPC_GET_VERSION::request req;
						COMMAND_RPC_GET_VERSION::response rsp;
						bool r = onGetVersion(req, rsp);
						if (r) {
							response.addHeader("Content-Type", "application/json");
							response.setStatus(HttpResponse::HTTP_STATUS::STATUS_200);
							response.setBody(storeToJson(rsp));
						}
						else {
							response.setStatus(HttpResponse::STATUS_500);
							response.setBody("Internal error");
						}
						return;
					}
					else if (Common::starts_with(url, hardware_info_method)) {
						auto it = s_handlers.find("/gethardwareinfo");
						if (!it->second.allowBusyCore && !isCoreReady()) {
							response.setStatus(HttpResponse::STATUS_500);
							response.setBody("Core is busy");
							return;
						}
						COMMAND_RPC_GET_HARDWARE_INFO::request req;
						COMMAND_RPC_GET_HARDWARE_INFO::response rsp;
						bool r = onGetHardwareInfo(req, rsp);
						if (r) {
							response.addHeader("Content-Type", "application/json");
							response.setStatus(HttpResponse::HTTP_STATUS::STATUS_200);
							response.setBody(storeToJson(rsp));
						}
						else {
							response.setStatus(HttpResponse::STATUS_500);
							response.setBody("Internal error");
						}
						return;
					}
					response.setStatus(HttpResponse::STATUS_404);
					return;
				}
				else {
					response.setStatus(HttpResponse::STATUS_404);
					return;
				}
			}

			if (!it->second.allowBusyCore && !isCoreReady()) {
				response.setStatus(HttpResponse::STATUS_500);
				response.setBody("Core is busy");
				return;
			}

			it->second.handler(this, request, response);

		} catch (const JsonRpc::JsonRpcError &err) {
			response.addHeader("Content-Type", "application/json");
			response.setStatus(HttpResponse::STATUS_500);
			response.setBody(storeToJsonValue(err).toString());
		} catch (const std::exception &e) {
			response.setStatus(HttpResponse::STATUS_500);
			response.setBody(e.what());
		}
	}

	bool RpcServer::processJsonRpcRequest(const HttpRequest &request, HttpResponse &response)
	{
		using namespace JsonRpc;

		response.addHeader("Content-Type", "application/json");
		if (!m_cors_domain.empty()) {
			response.addHeader("Access-Control-Allow-Origin", m_cors_domain);
			response.addHeader("Access-Control-Allow-Headers",
							   "Origin, X-Requested-With, Content-Type, Accept");
			response.addHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
		}

		JsonRpcRequest jsonRequest;
		JsonRpcResponse jsonResponse;

		try {
			logger(TRACE) << "JSON-RPC request: " << request.getBody();

			jsonRequest.parseRequest(request.getBody());
			jsonResponse.setId(jsonRequest.getId()); // copy id

			static std::unordered_map<std::string, RpcServer::RpcHandler<JsonMemberMethod>>
					jsonRpcHandlers = {
					{"getblockcount",   {makeMemberMethod(&RpcServer::onGetBlockCount),                   true}},
					{"on_getblockhash", {makeMemberMethod(&RpcServer::onGetBlockHash),                    false}},
					{"getblocktemplate",
										{makeMemberMethod(&RpcServer::onGetBlockTemplate),                false}},
					{"getcurrencyid",   {makeMemberMethod(&RpcServer::onGetCurrencyId),                   true}},
					{"submitblock",     {makeMemberMethod(&RpcServer::onSubmitBlock),                     false}},
					{"getlastblockheader",
										{makeMemberMethod(&RpcServer::onGetLastBlockHeader),              false}},
					{"getblockheaderbyhash",
										{makeMemberMethod(&RpcServer::onGetBlockHeaderByHash),            false}},
					{"getblockheaderbyheight",
										{makeMemberMethod(&RpcServer::onGetBlockHeaderByHeight),          false}},
					{"getblockbyhash",
										{makeMemberMethod(&RpcServer::onGetBlockDetailsByHash),           true}},
					{"f_blocks_list_json",
										{makeMemberMethod(&RpcServer::onBlocksListJson),                  false}},
					{"f_block_json",    {makeMemberMethod(&RpcServer::onBlockJson),                       false}},
					{"f_transaction_json",
										{makeMemberMethod(&RpcServer::onTransactionJson),                 false}},
					{"get_mempool",
										{makeMemberMethod(&RpcServer::onTransactionsPoolJson),            false}},
					{"get_mempool_detailed",
										{makeMemberMethod(&RpcServer::onMempoolJson),                     false}},
					{"f_pool_json",     {makeMemberMethod(&RpcServer::onPoolJson),                        false}},
					{"transactions_by_payment_id",
										{makeMemberMethod(&RpcServer::onTransactionsByPaymentId),         false}},
					{"get_transaction_hashes_by_payment_id",
										{makeMemberMethod(&RpcServer::onGetTransactionHashesByPaymentId), false}},
					{"get_transaction_details_by_hashes",
										{makeMemberMethod(&RpcServer::onGetTransactionsDetailsByHashes),  false}},
					{"get_transactions_by_heights",
										{makeMemberMethod(&RpcServer::onGetTransactionsByHeights),        false}},
					{"gettransaction",
										{makeMemberMethod(&RpcServer::onGetTransactionDetailsByHash),     false}},
					{"get_blocks_details_by_heights",
										{makeMemberMethod(&RpcServer::onGetBlocksDetailsByHeights),       false}},
					{"get_block_details_by_height",
										{makeMemberMethod(&RpcServer::onGetBlockDetailsByHeight),         false}},
					{"get_blocks_details_by_hashes",
										{makeMemberMethod(&RpcServer::onGetBlocksDetailsByHashes),        false}},
					{"get_blocks_hashes_by_timestamps",
										{makeMemberMethod(&RpcServer::onGetBlocksHashesByTimestamps),     false}},
					{"check_tx_key",    {makeMemberMethod(&RpcServer::onCheckTxKey),                      false}},
					{"check_tx_with_view_key",
										{makeMemberMethod(&RpcServer::onCheckTxWithViewKey),              false}},
					{"check_tx_proof",
										{makeMemberMethod(&RpcServer::onCheckTxProof),                    false}},
					{"check_reserve_proof",
										{makeMemberMethod(&RpcServer::onCheckReserveProof),               false}},
					{"validateaddress",
										{makeMemberMethod(&RpcServer::onValidateAddress),                 false}},
					{"verifymessage",   {makeMemberMethod(&RpcServer::onVerifyMessage),                   false}}
			};

			auto it = jsonRpcHandlers.find(jsonRequest.getMethod());
			if (it == jsonRpcHandlers.end()) {
				throw JsonRpcError(JsonRpc::errMethodNotFound);
			}
			if (!it->second.allowBusyCore && !isCoreReady()) {
				throw JsonRpcError(CORE_RPC_ERROR_CODE_CORE_BUSY, "Core is busy");
			}
			it->second.handler(this, jsonRequest, jsonResponse);
		} catch (const JsonRpcError &err) {
			jsonResponse.setError(err);
		} catch (const std::exception &e) {
			jsonResponse.setError(JsonRpcError(JsonRpc::errInternalError, e.what()));
		}

		response.setBody(jsonResponse.getBody());
		logger(TRACE) << "JSON-RPC response: " << jsonResponse.getBody();

		return true;
	}

	bool RpcServer::restrictRPC(const bool is_restricted)
	{
		m_restricted_rpc = is_restricted;

		return true;
	}

	bool RpcServer::enableCors(const std::string domain)
	{
		m_cors_domain = domain;

		return true;
	}

	std::string RpcServer::getCorsDomain()
	{
		return m_cors_domain;
	}

	bool RpcServer::setFeeAddress(const std::string &fee_address, const AccountPublicAddress &fee_acc)
	{
		m_fee_address = fee_address;
		m_fee_acc = fee_acc;

		return true;
	}

	bool RpcServer::setViewKey(const std::string &view_key)
	{
		Crypto::Hash private_view_key_hash;
		size_t size;
		if (!Common::fromHex(view_key, &private_view_key_hash, sizeof(private_view_key_hash), size)
			|| size != sizeof(private_view_key_hash)) {
			logger(INFO) << "Could not parse private view key";
			return false;
		}

		m_view_key = *(struct Crypto::SecretKey *) &private_view_key_hash;

		return true;
	}

	bool RpcServer::setContactInfo(const std::string &contact)
	{
		m_contact_info = contact;

		return true;
	}

	bool RpcServer::isCoreReady()
	{
		return m_core.currency().isTestnet() || m_p2p.get_payload_object().isSynchronized();
	}

	bool RpcServer::masternodeCheckIncomingTx(const BinaryArray &tx_blob)
	{
		Crypto::Hash tx_hash = NULL_HASH;
		Crypto::Hash tx_prefixt_hash = NULL_HASH;
		Transaction tx;
		if (!parseAndValidateTransactionFromBinaryArray(tx_blob, tx, tx_hash, tx_prefixt_hash)) {
			logger(INFO) << "Could not parse tx from blob";
			return false;
		}

		// always relay fusion transactions
		uint64_t inputs_amount = 0;
		get_inputs_money_amount(tx, inputs_amount);
		uint64_t outputs_amount = get_outs_money_amount(tx);

		const uint64_t fee = inputs_amount - outputs_amount;
		bool isFusionTransaction = m_core.currency().isFusionTransaction(
				tx, tx_blob.size(), m_core.get_current_blockchain_height() - 1);
		if (fee == 0 && isFusionTransaction) {
			logger(DEBUGGING) << "Masternode received fusion transaction, relaying with no fee check";
			return true;
		}

		CryptoNote::TransactionPrefix transaction = *static_cast<const TransactionPrefix *>(&tx);

		std::vector<uint32_t> out;
		uint64_t amount;

		if (!CryptoNote::findOutputsToAccount(transaction, m_fee_acc, m_view_key, out, amount)) {
			logger(INFO) << "Could not find outputs to masternode fee address";
			return false;
		}

		if (amount != 0) {
			logger(INFO) << "Masternode received relayed transaction fee: "
						 << m_core.currency().formatAmount(amount) << " QWC";
			return true;
		}

		return false;
	}

	bool RpcServer::onGetBlocks(const COMMAND_RPC_GET_BLOCKS_FAST::request &req,
								COMMAND_RPC_GET_BLOCKS_FAST::response &res)
	{
		// TODO: code duplication see InProcessNode::doGetNewBlocks()
		if (req.block_ids.empty()) {
			res.status = "Failed";
			return false;
		}

		if (req.block_ids.back() != m_core.getBlockIdByHeight(0)) {
			res.status = "Failed";
			return false;
		}

		uint32_t totalBlockCount;
		uint32_t startBlockIndex;
		std::vector<Crypto::Hash> supplement = m_core.findBlockchainSupplement(
				req.block_ids, COMMAND_RPC_GET_BLOCKS_FAST_MAX_COUNT, totalBlockCount, startBlockIndex);

		res.current_height = totalBlockCount;
		res.start_height = startBlockIndex;

		for (const auto &blockId : supplement) {
			assert(m_core.have_block(blockId));
			auto completeBlock = m_core.getBlock(blockId);
			assert(completeBlock != nullptr);

			res.blocks.resize(res.blocks.size() + 1);
			res.blocks.back().block = asString(toBinaryArray(completeBlock->getBlock()));

			res.blocks.back().txs.reserve(completeBlock->getTransactionCount());
			for (size_t i = 0; i < completeBlock->getTransactionCount(); ++i) {
				res.blocks.back().txs.push_back(
						asString(toBinaryArray(completeBlock->getTransaction(i))));
			}
		}

		res.status = CORE_RPC_STATUS_OK;

		return true;
	}

	bool RpcServer::onQueryBlocks(const COMMAND_RPC_QUERY_BLOCKS::request &req,
								  COMMAND_RPC_QUERY_BLOCKS::response &res)
	{
		uint32_t startHeight;
		uint32_t currentHeight;
		uint32_t fullOffset;

		if (!m_core.queryBlocks(req.block_ids, req.timestamp, startHeight, currentHeight, fullOffset,
								res.items)) {
			res.status = "Failed to perform query";
			return false;
		}

		res.start_height = startHeight;
		res.current_height = currentHeight;
		res.full_offset = fullOffset;

		res.status = CORE_RPC_STATUS_OK;

		return true;
	}

	bool RpcServer::onQueryBlocksLite(const COMMAND_RPC_QUERY_BLOCKS_LITE::request &req,
									  COMMAND_RPC_QUERY_BLOCKS_LITE::response &res)
	{
		uint32_t startHeight;
		uint32_t currentHeight;
		uint32_t fullOffset;
		if (!m_core.queryBlocksLite(req.blockIds, req.timestamp, startHeight, currentHeight, fullOffset,
									res.items)) {
			res.status = "Failed to perform query";
			return false;
		}

		res.startHeight = startHeight;
		res.currentHeight = currentHeight;
		res.fullOffset = fullOffset;

		res.status = CORE_RPC_STATUS_OK;

		return true;
	}

	bool RpcServer::onQueryBlocksDetailed(const COMMAND_RPC_QUERY_BLOCKS_DETAILED::request &req,
										  COMMAND_RPC_QUERY_BLOCKS_DETAILED::response &res)
	{
		uint32_t startIndex;
		uint32_t currentIndex;
		uint32_t fullOffset;

		if (!m_core.queryBlocksDetailed(req.blockIds, req.timestamp, startIndex, currentIndex,
										fullOffset, res.blocks)) {
			res.status = "Failed to perform query";
			return false;
		}

		res.startHeight = startIndex;
		res.currentHeight = currentIndex;
		res.fullOffset = fullOffset;

		res.status = CORE_RPC_STATUS_OK;

		return true;
	}

	bool RpcServer::onGetIndexes(const COMMAND_RPC_GET_TX_GLOBAL_OUTPUTS_INDEXES::request &req,
								 COMMAND_RPC_GET_TX_GLOBAL_OUTPUTS_INDEXES::response &res)
	{
		std::vector<uint32_t> outputIndexes;
		if (!m_core.get_tx_outputs_gindexs(req.txid, outputIndexes)) {
			res.status = "Failed";
			return true;
		}

		res.o_indexes.assign(outputIndexes.begin(), outputIndexes.end());
		logger(TRACE) << "COMMAND_RPC_GET_TX_GLOBAL_OUTPUTS_INDEXES: [" << res.o_indexes.size() << "]";

		res.status = CORE_RPC_STATUS_OK;

		return true;
	}

	bool RpcServer::onGetRandomOuts(const COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::request &req,
									COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::response &res)
	{
		res.status = "Failed";
		if (!m_core.get_random_outs_for_amounts(req, res)) {
			return true;
		}

		res.status = CORE_RPC_STATUS_OK;

		std::stringstream ss;
		typedef COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount outs_for_amount;
		typedef COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::out_entry out_entry;

		std::for_each(res.outs.begin(), res.outs.end(), [&](outs_for_amount &ofa) {
			ss << "[" << ofa.amount << "]:";

			assert(ofa.outs.size() && "internal error: ofa.outs.size() is empty");

			std::for_each(ofa.outs.begin(), ofa.outs.end(),
						  [&](out_entry &oe) { ss << oe.global_amount_index << " "; });

			ss << ENDL;
		});
		std::string s = ss.str();
		logger(TRACE) << "COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS: " << ENDL << s;

		res.status = CORE_RPC_STATUS_OK;

		return true;
	}

	bool RpcServer::onGetPoolChanges(const COMMAND_RPC_GET_POOL_CHANGES::request &req,
									 COMMAND_RPC_GET_POOL_CHANGES::response &rsp)
	{
		rsp.status = CORE_RPC_STATUS_OK;
		std::vector<CryptoNote::Transaction> addedTransactions;
		rsp.isTailBlockActual = m_core.getPoolChanges(req.tailBlockId, req.knownTxsIds,
													  addedTransactions, rsp.deletedTxsIds);
		for (auto &tx : addedTransactions) {
			BinaryArray txBlob;
			if (!toBinaryArray(tx, txBlob)) {
				rsp.status = "Internal error";
				break;
			}

			rsp.addedTxs.emplace_back(std::move(txBlob));
		}
		return true;
	}

	bool RpcServer::onGetPoolChangesLite(const COMMAND_RPC_GET_POOL_CHANGES_LITE::request &req,
										 COMMAND_RPC_GET_POOL_CHANGES_LITE::response &rsp)
	{
		rsp.status = CORE_RPC_STATUS_OK;
		rsp.isTailBlockActual = m_core.getPoolChangesLite(req.tailBlockId, req.knownTxsIds,
														  rsp.addedTxs, rsp.deletedTxsIds);

		return true;
	}

	bool RpcServer::onGetBlocksDetailsByHeights(
			const COMMAND_RPC_GET_BLOCKS_DETAILS_BY_HEIGHTS::request &req,
			COMMAND_RPC_GET_BLOCKS_DETAILS_BY_HEIGHTS::response &rsp)
	{
		try {
			std::vector<BlockDetails2> blockDetails;
			for (const uint32_t &height : req.blockHeights) {
				if (m_core.get_current_blockchain_height() <= height) {
					throw JsonRpc::JsonRpcError{
							CORE_RPC_ERROR_CODE_TOO_BIG_HEIGHT,
							std::string("To big height: ") + std::to_string(height)
							+ ", current blockchain height = "
							+ std::to_string(m_core.get_current_blockchain_height() - 1)
					};
				}
				Hash block_hash = m_core.getBlockIdByHeight(height);
				Block blk;
				if (!m_core.getBlockByHash(block_hash, blk)) {
					throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_INTERNAL_ERROR,
												"Internal error: can't get block by height "
												+ std::to_string(height) + '.'};
				}
				BlockDetails2 detail;
				if (!m_core.fillBlockDetails(blk, detail)) {
					throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_INTERNAL_ERROR,
												"Internal error: can't fill block details."};
				}
				blockDetails.push_back(detail);
			}
			rsp.blocks = std::move(blockDetails);
		} catch (std::system_error &e) {
			rsp.status = e.what();
			return false;
		} catch (std::exception &e) {
			rsp.status = "Error: " + std::string(e.what());
			return false;
		}

		rsp.status = CORE_RPC_STATUS_OK;

		return true;
	}

	bool RpcServer::onGetBlocksDetailsByHashes(
			const COMMAND_RPC_GET_BLOCKS_DETAILS_BY_HASHES::request &req,
			COMMAND_RPC_GET_BLOCKS_DETAILS_BY_HASHES::response &rsp)
	{
		try {
			std::vector<BlockDetails2> blockDetails;
			for (const Crypto::Hash &hash : req.blockHashes) {
				Block blk;
				if (!m_core.getBlockByHash(hash, blk)) {
					// throw JsonRpc::JsonRpcError{
					//    CORE_RPC_ERROR_CODE_INTERNAL_ERROR,
					//    "Internal error: can't get block by hash " + Common::PodToHex(hash) + '.'
					//};
				}
				BlockDetails2 detail;
				if (!m_core.fillBlockDetails(blk, detail)) {
					throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_INTERNAL_ERROR,
												"Internal error: can't fill block details."};
				}
				blockDetails.push_back(detail);
			}
			rsp.blocks = std::move(blockDetails);
		} catch (std::system_error &e) {
			rsp.status = e.what();
			return false;
		} catch (std::exception &e) {
			rsp.status = "Error: " + std::string(e.what());
			return false;
		}

		rsp.status = CORE_RPC_STATUS_OK;

		return true;
	}

	bool RpcServer::onGetBlockDetailsByHeight(
			const COMMAND_RPC_GET_BLOCK_DETAILS_BY_HEIGHT::request &req,
			COMMAND_RPC_GET_BLOCK_DETAILS_BY_HEIGHT::response &rsp)
	{
		try {
			BlockDetails2 blockDetails;
			if (m_core.get_current_blockchain_height() <= req.blockHeight) {
				throw JsonRpc::JsonRpcError{
						CORE_RPC_ERROR_CODE_TOO_BIG_HEIGHT,
						std::string("To big height: ") + std::to_string(req.blockHeight)
						+ ", current blockchain height = "
						+ std::to_string(m_core.get_current_blockchain_height() - 1)
				};
			}
			Hash block_hash = m_core.getBlockIdByHeight(req.blockHeight);
			Block blk;
			if (!m_core.getBlockByHash(block_hash, blk)) {
				throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_INTERNAL_ERROR,
											"Internal error: can't get block by height "
											+ std::to_string(req.blockHeight) + '.'};
			}
			if (!m_core.fillBlockDetails(blk, blockDetails)) {
				throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_INTERNAL_ERROR,
											"Internal error: can't fill block details."};
			}
			rsp.block = blockDetails;
		} catch (std::system_error &e) {
			rsp.status = e.what();
			return false;
		} catch (std::exception &e) {
			rsp.status = "Error: " + std::string(e.what());
			return false;
		}

		rsp.status = CORE_RPC_STATUS_OK;

		return true;
	}

	bool RpcServer::onGetBlockDetailsByHash(const COMMAND_RPC_GET_BLOCK_DETAILS_BY_HASH::request &req,
											COMMAND_RPC_GET_BLOCK_DETAILS_BY_HASH::response &rsp)
	{
		try {
			BlockDetails2 blockDetails;
			Crypto::Hash block_hash;

			if (!parse_hash256(req.hash, block_hash)) {
				throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_WRONG_PARAM,
											"Failed to parse hex representation of block hash. Hex = "
											+ req.hash + '.'};
			}
			Block blk;
			if (!m_core.getBlockByHash(block_hash, blk)) {
				throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_INTERNAL_ERROR,
											"Internal error: can't get block by hash. Hash = "
											+ req.hash + '.'};
			}
			if (!m_core.fillBlockDetails(blk, blockDetails)) {
				throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_INTERNAL_ERROR,
											"Internal error: can't fill block details."};
			}
			rsp.block = blockDetails;
		} catch (std::system_error &e) {
			throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_INTERNAL_ERROR, e.what()};
			return false;
		} catch (std::exception &e) {
			throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_INTERNAL_ERROR,
										"Error: " + std::string(e.what())};
			return false;
		}
		rsp.status = CORE_RPC_STATUS_OK;
		return true;
	}

	bool RpcServer::onGetBlocksHashesByTimestamps(
			const COMMAND_RPC_GET_BLOCKS_HASHES_BY_TIMESTAMPS::request &req,
			COMMAND_RPC_GET_BLOCKS_HASHES_BY_TIMESTAMPS::response &rsp)
	{
		try {
			uint32_t count;
			std::vector<Crypto::Hash> blockHashes;
			if (!m_core.get_blockchain_storage().getBlockIdsByTimestamp(
					req.timestampBegin, req.timestampEnd, req.limit, blockHashes, count)) {
				throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_INTERNAL_ERROR,
											"Internal error: can't get blocks within timestamps "
											+ std::to_string(req.timestampBegin) + " - "
											+ std::to_string(req.timestampEnd) + "."};
			}
			rsp.blockHashes = std::move(blockHashes);
			rsp.count = count;
		} catch (std::system_error &e) {
			rsp.status = e.what();
			return false;
		} catch (std::exception &e) {
			rsp.status = "Error: " + std::string(e.what());
			return false;
		}

		rsp.status = CORE_RPC_STATUS_OK;

		return true;
	}

	bool RpcServer::onGetTransactionsDetailsByHashes(
			const COMMAND_RPC_GET_TRANSACTIONS_DETAILS_BY_HASHES::request &req,
			COMMAND_RPC_GET_TRANSACTIONS_DETAILS_BY_HASHES::response &rsp)
	{
		try {
			std::vector<TransactionDetails2> transactionsDetails;
			transactionsDetails.reserve(req.transactionHashes.size());

			std::list<Crypto::Hash> missed_txs;
			std::list<Transaction> txs;
			m_core.getTransactions(req.transactionHashes, txs, missed_txs, true);

			if (!txs.empty()) {
				for (const Transaction &tx : txs) {
					TransactionDetails2 txDetails;
					if (!m_core.fillTransactionDetails(tx, txDetails)) {
						throw JsonRpc::JsonRpcError{
								CORE_RPC_ERROR_CODE_INTERNAL_ERROR,
								"Internal error: can't fill transaction details."
						};
					}
					transactionsDetails.push_back(txDetails);
				}

				rsp.transactions = std::move(transactionsDetails);
				rsp.status = CORE_RPC_STATUS_OK;
			}
			if (txs.empty() || !missed_txs.empty()) {
				std::ostringstream ss;
				std::string separator;
				for (auto h : missed_txs) {
					ss << separator << Common::podToHex(h);
					separator = ",";
				}
				rsp.status = "transaction(s) not found: " + ss.str() + ".";
			}
		} catch (std::system_error &e) {
			rsp.status = e.what();
			return false;
		} catch (std::exception &e) {
			rsp.status = "Error: " + std::string(e.what());
			return false;
		}

		return true;
	}

	bool RpcServer::onGetTransactionDetailsByHash(
			const COMMAND_RPC_GET_TRANSACTION_DETAILS_BY_HASH::request &req,
			COMMAND_RPC_GET_TRANSACTION_DETAILS_BY_HASH::response &rsp)
	{
		try {
			std::list<Crypto::Hash> missed_txs;
			std::list<Transaction> txs;
			std::vector<Crypto::Hash> hashes;
			Crypto::Hash tx_hash;
			if (!parse_hash256(req.hash, tx_hash)) {
				throw JsonRpc::JsonRpcError{
						CORE_RPC_ERROR_CODE_WRONG_PARAM,
						"Failed to parse hex representation of transaction hash. Hex = " + req.hash + '.'
				};
			}
			hashes.push_back(tx_hash);
			m_core.getTransactions(hashes, txs, missed_txs, true);

			if (txs.empty() || !missed_txs.empty()) {
				std::string hash_str = Common::podToHex(missed_txs.back());
				throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_WRONG_PARAM,
											"transaction wasn't found. Hash = " + hash_str + '.'};
			}

			TransactionDetails2 transactionsDetails;
			if (!m_core.fillTransactionDetails(txs.back(), transactionsDetails)) {
				throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_INTERNAL_ERROR,
											"Internal error: can't fill transaction details."};
			}

			rsp.transaction = std::move(transactionsDetails);
		} catch (std::system_error &e) {
			throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_INTERNAL_ERROR, e.what()};
			return false;
		} catch (std::exception &e) {
			throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_INTERNAL_ERROR,
										"Error: " + std::string(e.what())};
			return false;
		}
		rsp.status = CORE_RPC_STATUS_OK;
		return true;
	}

	bool RpcServer::onGetTransactionHashesByPaymentId(
			const COMMAND_RPC_GET_TRANSACTION_HASHES_BY_PAYMENT_ID::request &req,
			COMMAND_RPC_GET_TRANSACTION_HASHES_BY_PAYMENT_ID::response &rsp)
	{
		Crypto::Hash pid_hash;
		if (!parse_hash256(req.paymentId, pid_hash)) {
			throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_WRONG_PARAM,
										"Failed to parse hex representation of payment id. Hex = "
										+ req.paymentId + '.'};
		}
		try {
			rsp.transactionHashes = m_core.getTransactionHashesByPaymentId(pid_hash);
		} catch (std::system_error &e) {
			throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_INTERNAL_ERROR, e.what()};
			return false;
		} catch (std::exception &e) {
			throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_INTERNAL_ERROR,
										"Error: " + std::string(e.what())};
			return false;
		}
		rsp.status = CORE_RPC_STATUS_OK;
		return true;
	}
//
// HTTP handlers
//

	bool RpcServer::onGetIndex(const COMMAND_HTTP::request &req, COMMAND_HTTP::response &res)
	{
		const std::string index_start =
			R"(
				<!DOCTYPE html><html lang='en'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1, shrink-to-fit=no'>
				<meta http-equiv='refresh' content='60'/><title data-i18n='website-title'>Masternode | Qwertycoin is a secure worldwide digital currency.</title><link href='https://fonts.googleapis.com/css2?family=Montserrat:wght@800;900&family=Open+Sans&display=swap' rel='stylesheet'><link rel='stylesheet' href='https://stackpath.bootstrapcdn.com/bootstrap/4.4.1/css/bootstrap.min.css'></head><body style='color: white;background-color:black;'><img src='https://qwertycoin.org/img/logo.png' alt='Qwertycoin'><br/><hr/><h1>Masternode</h1><br/>Version 
    		)";
		const std::string index_finish = "</body></html>";
		const std::time_t uptime = std::time(nullptr) - m_core.getStartTime();
		const std::string uptime_str = std::to_string((unsigned int) floor(uptime / 60.0 / 60.0 / 24.0))
									   + "d " +
									   std::to_string((unsigned int) floor(fmod((uptime / 60.0 / 60.0), 24.0))) + "h "
									   + std::to_string((unsigned int) floor(fmod((uptime / 60.0), 60.0))) + "m "
									   + std::to_string((unsigned int) fmod(uptime, 60.0)) + "s";
		uint32_t top_block_index = m_core.get_current_blockchain_height() - 1;
		uint32_t top_known_block_index =
				std::max(static_cast<uint32_t>(1), m_protocolQuery.getObservedHeight()) - 1;
		size_t outConn = m_p2p.get_outgoing_connections_count();
		size_t incConn = m_p2p.get_connections_count() - outConn;
		Crypto::Hash last_block_hash = m_core.getBlockIdByHeight(top_block_index);
		size_t anchor_peerlist_size = m_p2p.getPeerlistManager().get_anchor_peers_count();
		size_t white_peerlist_size = m_p2p.getPeerlistManager().get_white_peers_count();
		size_t grey_peerlist_size = m_p2p.getPeerlistManager().get_gray_peers_count();
		size_t alt_blocks_count = m_core.get_alternative_blocks_count();
		size_t total_tx_count = m_core.get_blockchain_total_transactions() - top_block_index + 1;
		size_t tx_pool_count = m_core.get_pool_transactions_count();

		const std::string body = index_start +
			PROJECT_VERSION_LONG + (m_core.currency().isTestnet() ? " testnet" : " mainnet") + 
			"<ul>" + 
			"<li>Synchronization status: " + std::to_string(top_block_index) + "/" + std::to_string(top_known_block_index) + 
			"<li>Last Blockhash: " + Common::podToHex(last_block_hash) + "</li>" + 
			"<li>Difficulty: " + std::to_string(m_core.getNextBlockDifficulty(0)) + "</li>" + 
			"<li>Alt. Blocks: " + std::to_string(alt_blocks_count) + "</li>" + 
			"<li>Total transactions in network: " + std::to_string(total_tx_count) + "</li>" + 
			"<li>Transactions unconfirmed: " + std::to_string(tx_pool_count) + "</li>" + 
			"<li>Connections: " + 
			"<ul>" + 
				"<li>RPC: \t" + std::to_string(get_connections_count()) + "</li>" + 
				"<li>Outgoing: \t" + std::to_string(outConn) + "</li>" + 
				"<li>Incoming: \t" + std::to_string(incConn) + "</li>" + 
			"</ul></li>" + 
			"<li>Peers: " + 
			"<ul>" + 
				"<li>Anchor: \t" + std::to_string(anchor_peerlist_size) + 
				"<li>White: \t" + std::to_string(white_peerlist_size) + 
				"<li>Grey: \t" + std::to_string(grey_peerlist_size) + 
			"</ul></li>" + 
			"<li>Contact: " + m_contact_info + "</li>" + 
			"<li>Fee address: " + m_fee_address + "</li>" + 
			"<li>Uptime: " + uptime_str + "</li>" + 
			"</ul>" + 
			index_finish;

		res = body;

		return true;
	}

	bool RpcServer::onGetSupply(const COMMAND_HTTP::request &req, COMMAND_HTTP::response &res)
	{
		std::string already_generated_coins =
				m_core.currency().formatAmount(m_core.getTotalGeneratedAmount());
		res = already_generated_coins;

		return true;
	}

	bool RpcServer::onGetPaymentId(const COMMAND_HTTP::request &req, COMMAND_HTTP::response &res)
	{
		res = Common::podToHex(Crypto::rand<Crypto::Hash>());
		return true;
	}

//
// JSON handlers
//

	bool RpcServer::onGetInfo(const COMMAND_RPC_GET_INFO::request &req,
							  COMMAND_RPC_GET_INFO::response &res)
	{
		res.height = m_core.get_current_blockchain_height();
		res.last_known_block_index =
				std::max(static_cast<uint32_t>(1), m_protocolQuery.getObservedHeight()) - 1;
		if (res.height == res.last_known_block_index) {
			// node is synced
			res.difficulty = m_core.getNextBlockDifficulty(time(nullptr));
		}
		else {
			// node is not synced yet
			res.difficulty = m_core.getNextBlockDifficulty(0);
		}
		res.tx_count = m_core.get_blockchain_total_transactions() - res.height; // without coinbase
		res.tx_pool_size = m_core.get_pool_transactions_count();
		res.alt_blocks_count = m_core.get_alternative_blocks_count();
		uint64_t total_conn = m_p2p.get_connections_count();
		res.outgoing_connections_count = m_p2p.get_outgoing_connections_count();
		res.incoming_connections_count = total_conn - res.outgoing_connections_count;
		res.rpc_connections_count = get_connections_count();
		res.white_peerlist_size = m_p2p.getPeerlistManager().get_white_peers_count();
		res.grey_peerlist_size = m_p2p.getPeerlistManager().get_gray_peers_count();
		Crypto::Hash last_block_hash =
				m_core.getBlockIdByHeight(m_core.get_current_blockchain_height() - 1);
		res.top_block_hash = Common::podToHex(last_block_hash);
		res.version = PROJECT_VERSION_LONG;
		res.fee_address = m_fee_address.empty() ? std::string() : m_fee_address;
		res.contact = m_contact_info.empty() ? std::string() : m_contact_info;
		res.min_tx_fee = m_core.getMinimalFee();
		res.readable_tx_fee = m_core.currency().formatAmount(m_core.getMinimalFee());
		res.start_time = (uint64_t) m_core.getStartTime();
		// NOTE: that large uint64_t number is unsafe in JavaScript environment
		// and therefore as a JSON value so we display it as a formatted string
		res.already_generated_coins = m_core.currency().formatAmount(m_core.getTotalGeneratedAmount());

		Block blk;
		if (!m_core.getBlockByHash(last_block_hash, blk)) {
			throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_INTERNAL_ERROR,
										"Internal error: can't get last block by hash."};
		}

		if (blk.baseTransaction.inputs.front().type() != typeid(BaseInput)) {
			throw JsonRpc::JsonRpcError{
					CORE_RPC_ERROR_CODE_INTERNAL_ERROR,
					"Internal error: coinbase transaction in the block has the wrong type"
			};
		}

		BLOCK_HEADER_RESPONSE_ENTRY block_header;
		uint32_t lastBlockHeight = boost::get<BaseInput>(blk.baseTransaction.inputs.front()).blockIndex;

		Crypto::Hash tmp_hash = m_core.getBlockIdByHeight(lastBlockHeight);
		bool is_orphaned = last_block_hash != tmp_hash;
		fillBlockHeaderResponse(blk, is_orphaned, lastBlockHeight, last_block_hash, block_header);

		res.block_major_version = block_header.major_version;
		res.block_minor_version = block_header.minor_version;
		res.last_block_timestamp = block_header.timestamp;
		res.last_block_reward = block_header.reward;
		m_core.getBlockDifficulty(static_cast<uint32_t>(lastBlockHeight), res.last_block_difficulty);

		res.status = CORE_RPC_STATUS_OK;

		return true;
	}

	bool RpcServer::onGetVersion(const COMMAND_RPC_GET_VERSION::request &req,
								 COMMAND_RPC_GET_VERSION::response &res)
	{
		res.version = PROJECT_VERSION_LONG;

		return true;
	}

	bool RpcServer::onGetHardwareInfo(const COMMAND_RPC_GET_HARDWARE_INFO::request &req,
									  COMMAND_RPC_GET_HARDWARE_INFO::response &res)
	{
		const std::time_t uptime = std::time(nullptr) - m_core.getStartTime();
		const std::string uptime_str = std::to_string((unsigned int) floor(uptime / 60.0 / 60.0 / 24.0))
									   + "d " +
									   std::to_string((unsigned int) floor(fmod((uptime / 60.0 / 60.0), 24.0))) + "h "
									   + std::to_string((unsigned int) floor(fmod((uptime / 60.0), 60.0))) + "m "
									   + std::to_string((unsigned int) fmod(uptime, 60.0)) + "s";

		// CPU
		res.cpuInfo.coreCount = Tools::CPU::quantities().physical;
		res.cpuInfo.threadCount = Tools::CPU::quantities().logical;
		res.cpuInfo.architecture = Tools::CPU::architecture();

		// RAM
		res.ramInfo.ramTotal = Tools::Memory::MemInfo::sysMem();
		res.ramInfo.ramAvailable = Tools::Memory::MemInfo::freeSysMem();
		res.ramInfo.ramUsageVirt = Tools::Memory::MemInfo::usedVirtMem();
		res.ramInfo.ramUsagePhys = Tools::Memory::MemInfo::usedPhysMem();
		res.ramInfo.ramUsageVirtMax = Tools::Memory::MemInfo::usedVirtMemMax();
		res.ramInfo.ramUsagePhysMax = Tools::Memory::MemInfo::usedPhysMemMax();

		// Space
		boost::filesystem::path configFolderPath(m_core.getConfigFolder());
		res.spaceInfo.freeSpace = std::to_string(Tools::Storage::SpaceInfo::freeSpace(configFolderPath) / (1 << 20)) + " MB";
		res.spaceInfo.availableSpace = std::to_string(Tools::Storage::SpaceInfo::availableSpace(configFolderPath) / (1 << 20)) + " MB";
		res.spaceInfo.capacitySpace = std::to_string(Tools::Storage::SpaceInfo::capacitySpace(configFolderPath) / (1 << 20)) + " MB";

		// other
		res.uptime = uptime_str;
		res.status = CORE_RPC_STATUS_OK;

		return true;
	}

	bool RpcServer::onGetHeight(const COMMAND_RPC_GET_HEIGHT::request &req,
								COMMAND_RPC_GET_HEIGHT::response &res)
	{
		res.height = m_core.get_current_blockchain_height();
		res.status = CORE_RPC_STATUS_OK;

		return true;
	}

	bool RpcServer::onGetTransactions(const COMMAND_RPC_GET_TRANSACTIONS::request &req,
									  COMMAND_RPC_GET_TRANSACTIONS::response &res)
	{
		std::vector<Hash> vh;
		for (const auto &tx_hex_str : req.txs_hashes) {
			BinaryArray b;
			if (!fromHex(tx_hex_str, b)) {
				res.status = "Failed to parse hex representation of transaction hash";
				return true;
			}
			if (b.size() != sizeof(Hash)) {
				res.status = "Failed, size of data mismatch";
			}
			vh.push_back(*reinterpret_cast<const Hash *>(b.data()));
		}
		std::list<Hash> missed_txs;
		std::list<Transaction> txs;
		m_core.getTransactions(vh, txs, missed_txs);

		for (auto &tx : txs) {
			res.txs_as_hex.push_back(toHex(toBinaryArray(tx)));
		}

		for (const auto &miss_tx : missed_txs) {
			res.missed_tx.push_back(Common::podToHex(miss_tx));
		}

		res.status = CORE_RPC_STATUS_OK;

		return true;
	}

	bool RpcServer::onGetTransactionsByHeights(
			const COMMAND_RPC_GET_TRANSACTIONS_BY_HEIGHTS::request &req,
			COMMAND_RPC_GET_TRANSACTIONS_BY_HEIGHTS::response &res)
	{
		try {
			std::vector<Crypto::Hash> vh;

			if (req.range) {
				if (req.heights.size() != 2) {
					res.status = "Range set true but heights size != 2";
					return true;
				}

				uint32_t upperBorder = std::min(req.heights[1], m_core.get_current_blockchain_height());

				for (size_t i = 0; i < (upperBorder - req.heights[0]); i++) {
					Block blk;
					Crypto::Hash blockHash = m_core.getBlockIdByHeight(req.heights[0] + i);

					if (!m_core.getBlockByHash(blockHash, blk)) {
						throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_INTERNAL_ERROR,
													"Internal error: can't get block by hash. Hash = "
													+ podToHex(blockHash) + '.'};
					}

					if (blk.baseTransaction.inputs.front().type() != typeid(BaseInput)) {
						throw JsonRpc::JsonRpcError{
								CORE_RPC_ERROR_CODE_INTERNAL_ERROR,
								"Internal error: coinbase transaction in the block has the wrong type"
						};
					}

					for (Crypto::Hash &bTxs : blk.transactionHashes) {
						vh.push_back(bTxs);
					}

					if (req.include_miner_txs) {
						vh.push_back(getObjectHash(blk.baseTransaction));
					}
				}
			}
			else {
				for (size_t i = 0; i < req.heights.size(); i++) {
					Block blk;
					Crypto::Hash blockHash = m_core.getBlockIdByHeight(req.heights[i]);

					for (Crypto::Hash &bTxs : blk.transactionHashes) {
						vh.push_back(bTxs);
					}

					if (req.include_miner_txs) {
						vh.push_back(getObjectHash(blk.baseTransaction));
					}
				}
			}

			std::list<Crypto::Hash> missedTxs;
			std::list<Transaction> txs;

			m_core.getTransactions(vh, txs, missedTxs, true);

			std::list<std::string> txHashes;
			for (auto &tx : txs) {
				txHashes.push_back(Common::podToHex(getObjectHash(tx)));
			}

			logger(DEBUGGING) << "Found " << txs.size() << "/" << vh.size()
							  << " transactions on the blockchain.";

			std::list<std::string>::const_iterator txHi = txHashes.begin();
			std::vector<Crypto::Hash>::const_iterator vHi = vh.begin();

			for (const Transaction &tx : txs) {
				res.txs.push_back(COMMAND_RPC_GET_TRANSACTIONS_BY_HEIGHTS::entry());
				COMMAND_RPC_GET_TRANSACTIONS_BY_HEIGHTS::entry &e = res.txs.back();

				Crypto::Hash blockHash;
				uint32_t blockHeight;
				uint64_t fee;
				get_tx_fee(tx, fee);

				Crypto::Hash txHash = *vHi++;
				e.tx_hash = *txHi++;

				bool r = m_core.getBlockContainingTx(txHash, blockHash, blockHeight);
				bool oR = m_core.get_tx_outputs_gindexs(txHash, e.output_indices);

				if (req.as_json) {
					e.as_json = tx;
				}

				e.block_height = blockHeight;
				e.block_timestamp = m_core.getBlockTimestamp(blockHeight);
				e.fee = fee;
			}

			if (txs.empty() || !missedTxs.empty()) {
				std::ostringstream oss;
				std::string seperator;
				for (auto h : missedTxs) {
					oss << seperator << Common::podToHex(h);
					seperator = ",";
				}
				res.status = "transaction(s) not found: " + oss.str() + ".";
			}

			res.status = CORE_RPC_STATUS_OK;
			return true;

		} catch (std::system_error &e) {
			res.status = e.what();
			return false;
		} catch (std::exception &e) {
			res.status = "Error: " + std::string(e.what());
			return false;
		}

		return true;
	}

	bool RpcServer::onGetRawTransactionsByHeights(const COMMAND_RPC_GET_RAW_TRANSACTIONS_BY_HEIGHTS::request &req,
												  COMMAND_RPC_GET_RAW_TRANSACTIONS_BY_HEIGHTS::response &res)
	{
		try {
			if (req.heights.size() > BLOCK_LIST_MAX_COUNT) {
				throw JsonRpc::JsonRpcError{
						CORE_RPC_ERROR_CODE_WRONG_PARAM,
						std::string("Requested blocks count: ") +
						std::to_string(req.heights.size()) +
						" exceeded max limit of " +
						std::to_string(BLOCK_LIST_MAX_COUNT)
				};
			}

			std::vector<uint32_t> heights;

			if (req.range) {
				if (req.heights.size() != 2) {
					throw JsonRpc::JsonRpcError{
							CORE_RPC_ERROR_CODE_WRONG_PARAM,
							std::string("The range is set to true but heights size is not equal to 2")
					};
				}

				uint32_t upperBound = std::min(req.heights[1], m_core.get_current_blockchain_height());
				for (size_t i = 0; i < (upperBound - req.heights[0]); i++) {
					heights.push_back(req.heights[0] + i);
				}
			}
			else {
				heights = req.heights;
			}

			for (const uint32_t &height : heights) {
				if (m_core.get_current_blockchain_height() <= height) {
					throw JsonRpc::JsonRpcError{
							CORE_RPC_ERROR_CODE_TOO_BIG_HEIGHT,
							std::string("To big height: ") +
							std::to_string(height) +
							", current blockchain height = " +
							std::to_string(m_core.get_current_blockchain_height() - 1)
					};
				}

				Crypto::Hash blockHash = m_core.getBlockIdByHeight(height);
				Block blk;
				std::vector<Crypto::Hash> txsIds;
				if (!m_core.getBlockByHash(blockHash, blk)) {
					throw JsonRpc::JsonRpcError{
							CORE_RPC_ERROR_CODE_INTERNAL_ERROR,
							"Internal error: can't get block by height " +
							std::to_string(height) +
							'.'
					};
				}

				if (req.includeMinerTxs) {
					txsIds.reserve(blk.transactionHashes.size() + 1);
					txsIds.push_back(getObjectHash(blk.baseTransaction));
				}
				else {
					txsIds.reserve(blk.transactionHashes.size());
				}

				if (!blk.transactionHashes.empty()) {
					txsIds.insert(txsIds.end(), blk.transactionHashes.begin(), blk.transactionHashes.end());
				}

				std::vector<Crypto::Hash>::const_iterator ti = txsIds.begin();

				std::vector<std::pair<Transaction, std::vector<uint32_t>>> txs;
				std::list<Crypto::Hash> missed;

				if (!txsIds.empty()) {
					if (!m_core.getTransactionsWithOutputGlobalIndexes(txsIds, missed, txs)) {
						throw JsonRpc::JsonRpcError{
								CORE_RPC_ERROR_CODE_INTERNAL_ERROR,
								"Error getting transactions with output global indexes"
						};
					}

					for (const auto &txi : txs) {
						res.transactions.push_back(TxWithOutputGlobalIndices());
						TxWithOutputGlobalIndices &e = res.transactions.back();

						e.hash = *ti++;
						e.block_hash = blockHash;
						e.height = height;
						e.timestamp = blk.timestamp;
						e.transaction = *static_cast<const TransactionPrefix *>(&txi.first);
						e.output_indexes = txi.second;
						e.fee = is_coinbase(txi.first) ? 0 : getInputAmount(txi.first) - getOutputAmount(txi.first);
					}
				}

				for (const auto &missTx : missed) {
					res.missedTxs.push_back(Common::podToHex(missTx));
				}
			}
		} catch (std::system_error &e) {
			throw JsonRpc::JsonRpcError{
					CORE_RPC_ERROR_CODE_INTERNAL_ERROR,
					e.what()
			};

			return false;
		} catch (std::exception &e) {
			throw JsonRpc::JsonRpcError{
					CORE_RPC_ERROR_CODE_INTERNAL_ERROR,
					"Error: " + std::string(e.what())
			};

			return false;
		}

		res.status = CORE_RPC_STATUS_OK;
		return true;
	}

	bool RpcServer::onGetRawTransactionPool(const COMMAND_RPC_GET_RAW_TRANSACTIONS_FROM_POOL::request &req,
											COMMAND_RPC_GET_RAW_TRANSACTIONS_FROM_POOL::response &res)
	{
		auto pool = m_core.getMemoryPool();

		for (const auto &txd : pool) {
			res.transactions.push_back(TxWithOutputGlobalIndices());
			TxWithOutputGlobalIndices &e = res.transactions.back();

			e.hash = txd.id;
			e.height = boost::value_initialized<uint32_t>();
			e.block_hash = boost::value_initialized<Crypto::Hash>();
			e.timestamp = txd.receiveTime;
			e.transaction = *static_cast<const TransactionPrefix *>(&txd.tx);
			e.fee = txd.fee;
		}

		res.status = CORE_RPC_STATUS_OK;
		return true;
	}

	bool RpcServer::onSendRawTx(const COMMAND_RPC_SEND_RAW_TX::request &req,
								COMMAND_RPC_SEND_RAW_TX::response &res)
	{
		BinaryArray tx_blob;
		if (!fromHex(req.tx_as_hex, tx_blob)) {
			logger(INFO) << "[on_send_raw_tx]: Failed to parse tx from hexbuff: " << req.tx_as_hex;
			res.status = "Failed";
			return true;
		}

		Crypto::Hash transactionHash = Crypto::cn_fast_hash(tx_blob.data(), tx_blob.size());
		logger(DEBUGGING) << "transaction " << transactionHash << " came in on_send_raw_tx";

		tx_verification_context tvc = boost::value_initialized<tx_verification_context>();
		if (!m_core.handle_incoming_tx(tx_blob, tvc, false, false)) {
			logger(INFO) << "[on_send_raw_tx]: Failed to process tx";
			res.status = "Failed";
			return true;
		}

		if (tvc.m_verification_failed) {
			logger(INFO) << "[on_send_raw_tx]: tx verification failed";
			res.status = "Failed";
			return true;
		}

		if (!tvc.m_should_be_relayed) {
			logger(INFO) << "[on_send_raw_tx]: tx accepted, but not relayed";
			res.status = "Not relayed";
			return true;
		}

		if (!m_fee_address.empty() && m_view_key != NULL_SECRET_KEY) {
			if (!masternodeCheckIncomingTx(tx_blob)) {
				logger(INFO) << "Transaction not relayed due to lack of masternode fee";
				res.status = "Not relayed due to lack of node fee";
				return true;
			}
		}

		NOTIFY_NEW_TRANSACTIONS::request r;
		r.txs.push_back(asString(tx_blob));
		m_core.get_protocol()->relay_transactions(r);

		// TODO:
		//  make sure that tx has reached other nodes here,
		//  probably wait to receive reflections from other nodes
		res.status = CORE_RPC_STATUS_OK;

		return true;
	}

	bool RpcServer::onStartMining(const COMMAND_RPC_START_MINING::request &req,
								  COMMAND_RPC_START_MINING::response &res)
	{
		if (m_restricted_rpc) {
			res.status = "Failed, restricted handle";
			return false;
		}

		AccountPublicAddress adr;
		if (!m_core.currency().parseAccountAddressString(req.miner_address, adr)) {
			res.status = "Failed, wrong address";
			return true;
		}

		if (!m_core.get_miner().start(adr, static_cast<size_t>(req.threads_count))) {
			res.status = "Failed, mining not started";
			return true;
		}

		res.status = CORE_RPC_STATUS_OK;

		return true;
	}

	bool RpcServer::onStopMining(const COMMAND_RPC_STOP_MINING::request &req,
								 COMMAND_RPC_STOP_MINING::response &res)
	{
		if (m_restricted_rpc) {
			res.status = "Failed, restricted handle";
			return false;
		}

		if (!m_core.get_miner().stop()) {
			res.status = "Failed, mining not stopped";
			return true;
		}

		res.status = CORE_RPC_STATUS_OK;

		return true;
	}

	bool RpcServer::onStopDaemon(const COMMAND_RPC_STOP_DAEMON::request &req,
								 COMMAND_RPC_STOP_DAEMON::response &res)
	{
		if (m_restricted_rpc) {
			res.status = "Failed, restricted handle";
			return false;
		}
		if (m_core.currency().isTestnet()) {
			m_p2p.sendStopSignal();
			res.status = CORE_RPC_STATUS_OK;
		}
		else {
			res.status = CORE_RPC_ERROR_CODE_INTERNAL_ERROR;
			return false;
		}
		return true;
	}

	bool RpcServer::onGetFeeAddress(const COMMAND_RPC_GET_FEE_ADDRESS::request &req,
									COMMAND_RPC_GET_FEE_ADDRESS::response &res)
	{
		if (m_fee_address.empty()) {
			res.status = CORE_RPC_STATUS_OK;
			return false;
		}

		res.fee_address = m_fee_address;
		res.status = CORE_RPC_STATUS_OK;

		return true;
	}

	bool RpcServer::onGetPeerList(const COMMAND_RPC_GET_PEER_LIST::request &req,
									 	COMMAND_RPC_GET_PEER_LIST::response &res)
	{
		if (m_restricted_rpc) {
			res.status = "Method disabled";
			return false;
		}

		std::list<AnchorPeerlistEntry> pl_anchor;
		std::vector<PeerlistEntry> pl_wite;
		std::vector<PeerlistEntry> pl_gray;
		m_p2p.getPeerlistManager().get_peerlist_full(pl_anchor, pl_gray, pl_wite);

		for (const auto &pe : pl_anchor) {
			std::stringstream ss;
			ss << pe.adr;
			res.anchor_peers.push_back(ss.str());
		}

		for (const auto &pe : pl_wite) {
			std::stringstream ss;
			ss << pe.adr;
			res.white_peers.push_back(ss.str());
		}

		for (const auto &pe : pl_gray) {
			std::stringstream ss;
			ss << pe.adr;
			res.gray_peers.push_back(ss.str());
		}

		res.status = CORE_RPC_STATUS_OK;
		return true;
	}

	bool RpcServer::onBlocksListJson(const COMMAND_RPC_GET_BLOCKS_LIST::request &req,
									 COMMAND_RPC_GET_BLOCKS_LIST::response &res)
	{
		if (m_core.get_current_blockchain_height() <= req.height) {
			throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_TOO_BIG_HEIGHT,
										std::string("To big height: ") + std::to_string(req.height)
										+ ", current blockchain height = "
										+ std::to_string(
												m_core.get_current_blockchain_height())};
		}

		uint32_t print_blocks_count = 20;
		uint32_t last_height = req.height - print_blocks_count;
		if (req.height <= print_blocks_count) {
			last_height = 0;
		}

		for (uint32_t i = req.height; i >= last_height; i--) {
			Hash block_hash = m_core.getBlockIdByHeight(static_cast<uint32_t>(i));
			Block blk;
			if (!m_core.getBlockByHash(block_hash, blk)) {
				throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_INTERNAL_ERROR,
											"Internal error: can't get block by height. Height = "
											+ std::to_string(i) + '.'};
			}

			size_t tx_cumulative_block_size;
			m_core.getBlockSize(block_hash, tx_cumulative_block_size);
			size_t blockBlobSize = getObjectBinarySize(blk);
			size_t minerTxBlobSize = getObjectBinarySize(blk.baseTransaction);
			difficulty_type blockDiff;
			m_core.getBlockDifficulty(static_cast<uint32_t>(i), blockDiff);

			BLOCK_SHORT_RESPONSE block_short;
			BLOCK_HEADER_RESPONSE_ENTRY blockHeaderResponse;

			Crypto::Hash tmp_hash = m_core.getBlockIdByHeight(i);
			bool is_orphaned = block_hash != tmp_hash;

			fillBlockHeaderResponse(blk, is_orphaned, i, block_hash, blockHeaderResponse);

			block_short.timestamp = blk.timestamp;
			block_short.height = i;
			block_short.hash = Common::podToHex(block_hash);
			block_short.cumul_size = blockBlobSize + tx_cumulative_block_size - minerTxBlobSize;
			block_short.tx_count = blk.transactionHashes.size() + 1;
			block_short.reward = blockHeaderResponse.reward;
			block_short.difficulty = blockDiff;
			block_short.min_tx_fee = m_core.getMinimalFeeForHeight(i);

			res.blocks.push_back(block_short);

			if (i == 0) {
				break;
			}
		}

		res.status = CORE_RPC_STATUS_OK;

		return true;
	}

	bool RpcServer::onBlockJson(const COMMAND_RPC_GET_BLOCK_DETAILS::request &req,
								COMMAND_RPC_GET_BLOCK_DETAILS::response &res)
	{
		Hash hash;

		try {
			auto height = boost::lexical_cast<uint32_t>(req.hash);
			hash = m_core.getBlockIdByHeight(height);
		} catch (boost::bad_lexical_cast &) {
			if (!parse_hash256(req.hash, hash)) {
				throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_WRONG_PARAM,
											"Failed to parse hex representation of block hash. Hex = "
											+ req.hash + '.'};
			}
		}

		Block blk;
		if (!m_core.getBlockByHash(hash, blk)) {
			throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_INTERNAL_ERROR,
										"Internal error: can't get block by hash. Hash = " + req.hash
										+ '.'};
		}

		if (blk.baseTransaction.inputs.front().type() != typeid(BaseInput)) {
			throw JsonRpc::JsonRpcError{
					CORE_RPC_ERROR_CODE_INTERNAL_ERROR,
					"Internal error: coinbase transaction in the block has the wrong type"
			};
		}

		BLOCK_HEADER_RESPONSE_ENTRY block_header;
		res.block.height = boost::get<BaseInput>(blk.baseTransaction.inputs.front()).blockIndex;

		Crypto::Hash tmp_hash = m_core.getBlockIdByHeight(res.block.height);
		bool is_orphaned = hash != tmp_hash;
		fillBlockHeaderResponse(blk, is_orphaned, res.block.height, hash, block_header);

		res.block.major_version = block_header.major_version;
		res.block.minor_version = block_header.minor_version;
		res.block.timestamp = block_header.timestamp;
		res.block.prev_hash = block_header.prev_hash;
		res.block.nonce = block_header.nonce;
		res.block.hash = block_header.hash;
		res.block.depth = block_header.depth;
		res.block.orphan_status = block_header.orphan_status;
		m_core.getBlockDifficulty(static_cast<uint32_t>(res.block.height), res.block.difficulty);
		m_core.getBlockCumulativeDifficulty(static_cast<uint32_t>(res.block.height),
											res.block.cumulativeDifficulty);

		res.block.reward = block_header.reward;

		std::vector<size_t> blocksSizes;
		if (!m_core.getBackwardBlocksSizes(res.block.height, blocksSizes,
										   parameters::CRYPTONOTE_REWARD_BLOCKS_WINDOW)) {
			return false;
		}
		res.block.sizeMedian = Common::medianValue(blocksSizes);

		size_t blockSize = 0;
		if (!m_core.getBlockSize(hash, blockSize)) {
			return false;
		}
		res.block.transactionsCumulativeSize = blockSize;

		size_t blokBlobSize = getObjectBinarySize(blk);
		size_t minerTxBlobSize = getObjectBinarySize(blk.baseTransaction);
		res.block.blockSize = blokBlobSize + res.block.transactionsCumulativeSize - minerTxBlobSize;

		uint64_t alreadyGeneratedCoins;
		if (!m_core.getAlreadyGeneratedCoins(hash, alreadyGeneratedCoins)) {
			return false;
		}
		res.block.alreadyGeneratedCoins = std::to_string(alreadyGeneratedCoins);

		if (!m_core.getGeneratedTransactionsNumber(res.block.height,
												   res.block.alreadyGeneratedTransactions)) {
			return false;
		}

		uint64_t prevBlockGeneratedCoins = 0;
		uint32_t previousBlockHeight = 0;
		uint64_t blockTarget = CryptoNote::parameters::DIFFICULTY_TARGET;

		if (res.block.height > 0) {
			if (!m_core.getAlreadyGeneratedCoins(blk.previousBlockHash, prevBlockGeneratedCoins)) {
				return false;
			}
		}

		if (res.block.height >= CryptoNote::parameters::UPGRADE_HEIGHT_V6) {
			m_core.getBlockHeight(blk.previousBlockHash, previousBlockHeight);
			blockTarget = blk.timestamp - m_core.getBlockTimestamp(previousBlockHeight);
		}

		uint64_t maxReward = 0;
		uint64_t currentReward = 0;
		int64_t emissionChange = 0;
		size_t blockGrantedFullRewardZone =
				CryptoNote::parameters::CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V2;
		res.block.effectiveSizeMedian = std::max(res.block.sizeMedian, blockGrantedFullRewardZone);

		if (!m_core.getBlockReward(res.block.major_version, res.block.sizeMedian, 0,
								   prevBlockGeneratedCoins, 0, maxReward, emissionChange,
								   res.block.height, blockTarget)) {
			return false;
		}
		if (!m_core.getBlockReward(res.block.major_version, res.block.sizeMedian,
								   res.block.transactionsCumulativeSize, prevBlockGeneratedCoins, 0,
								   currentReward, emissionChange, res.block.height, blockTarget)) {
			return false;
		}

		res.block.baseReward = maxReward;
		if (maxReward == 0 && currentReward == 0) {
			res.block.penalty = static_cast<double>(0);
		}
		else {
			if (maxReward < currentReward) {
				return false;
			}
			res.block.penalty =
					static_cast<double>(maxReward - currentReward) / static_cast<double>(maxReward);
		}

		// Base transaction adding
		TRANSACTION_SHORT_RESPONSE transaction_short;
		transaction_short.hash = Common::podToHex(getObjectHash(blk.baseTransaction));
		transaction_short.fee = 0;
		transaction_short.amount_out = get_outs_money_amount(blk.baseTransaction);
		transaction_short.size = getObjectBinarySize(blk.baseTransaction);
		res.block.transactions.push_back(transaction_short);

		std::list<Crypto::Hash> missed_txs;
		std::list<Transaction> txs;
		m_core.getTransactions(blk.transactionHashes, txs, missed_txs);

		res.block.totalFeeAmount = 0;

		for (const Transaction &tx : txs) {
			TRANSACTION_SHORT_RESPONSE tr_short;
			uint64_t amount_in = 0;
			get_inputs_money_amount(tx, amount_in);
			uint64_t amount_out = get_outs_money_amount(tx);

			tr_short.hash = Common::podToHex(getObjectHash(tx));
			tr_short.fee = amount_in - amount_out;
			tr_short.amount_out = amount_out;
			tr_short.size = getObjectBinarySize(tx);
			res.block.transactions.push_back(tr_short);

			res.block.totalFeeAmount += tr_short.fee;
		}

		res.status = CORE_RPC_STATUS_OK;

		return true;
	}

	bool RpcServer::onTransactionJson(const COMMAND_RPC_GET_TRANSACTION_DETAILS::request &req,
									  COMMAND_RPC_GET_TRANSACTION_DETAILS::response &res)
	{
		Hash hash;

		if (!parse_hash256(req.hash, hash)) {
			throw JsonRpc::JsonRpcError{
					CORE_RPC_ERROR_CODE_WRONG_PARAM,
					"Failed to parse hex representation of transaction hash. Hex = " + req.hash + '.'
			};
		}

		std::vector<Crypto::Hash> tx_ids;
		tx_ids.push_back(hash);

		std::list<Crypto::Hash> missed_txs;
		std::list<Transaction> txs;
		m_core.getTransactions(tx_ids, txs, missed_txs, true);

		if (1 == txs.size()) {
			res.tx = txs.front();
		}
		else {
			throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_WRONG_PARAM,
										"transaction wasn't found. Hash = " + req.hash + '.'};
		}

		Crypto::Hash blockHash;
		uint32_t blockHeight;
		if (m_core.getBlockContainingTx(hash, blockHash, blockHeight)) {
			Block blk;
			if (m_core.getBlockByHash(blockHash, blk)) {
				size_t tx_cumulative_block_size;
				m_core.getBlockSize(blockHash, tx_cumulative_block_size);
				size_t blokBlobSize = getObjectBinarySize(blk);
				size_t minerTxBlobSize = getObjectBinarySize(blk.baseTransaction);
				BLOCK_SHORT_RESPONSE block_short;

				block_short.timestamp = blk.timestamp;
				block_short.height = blockHeight;
				block_short.hash = Common::podToHex(blockHash);
				block_short.cumul_size = blokBlobSize + tx_cumulative_block_size - minerTxBlobSize;
				block_short.tx_count = blk.transactionHashes.size() + 1;
				res.block = block_short;
				res.txDetails.confirmations = m_protocolQuery.getObservedHeight() - blockHeight;
			}
		}

		uint64_t amount_in = 0;
		get_inputs_money_amount(res.tx, amount_in);
		uint64_t amount_out = get_outs_money_amount(res.tx);

		res.txDetails.hash = Common::podToHex(getObjectHash(res.tx));
		res.txDetails.fee = amount_in - amount_out;
		if (amount_in == 0) {
			res.txDetails.fee = 0;
		}
		res.txDetails.amount_out = amount_out;
		res.txDetails.size = getObjectBinarySize(res.tx);

		uint64_t mixin;
		if (!onGetMixin(res.tx, mixin)) {
			return false;
		}
		res.txDetails.mixin = mixin;

		Crypto::Hash paymentId;
		if (CryptoNote::getPaymentIdFromTxExtra(res.tx.extra, paymentId)) {
			res.txDetails.paymentId = Common::podToHex(paymentId);
		}
		else {
			res.txDetails.paymentId = "";
		}

		res.txDetails.extra.raw = res.tx.extra;

		std::vector<CryptoNote::TransactionExtraField> txExtraFields;
		parseTransactionExtra(res.tx.extra, txExtraFields);
		for (const CryptoNote::TransactionExtraField &field : txExtraFields) {
			if (typeid(CryptoNote::TransactionExtraPadding) == field.type()) {
				res.txDetails.extra.padding.push_back(
						std::move(boost::get<CryptoNote::TransactionExtraPadding>(field).size));
			}
			else if (typeid(CryptoNote::TransactionExtraPublicKey) == field.type()) {
				res.txDetails.extra.publicKey = getTransactionPublicKeyFromExtra(res.tx.extra);
			}
			else if (typeid(CryptoNote::TransactionExtraNonce) == field.type()) {
				res.txDetails.extra.nonce.push_back(Common::toHex(
						boost::get<CryptoNote::TransactionExtraNonce>(field).nonce.data(),
						boost::get<CryptoNote::TransactionExtraNonce>(field).nonce.size()));
			}
		}

		res.status = CORE_RPC_STATUS_OK;

		return true;
	}

	bool RpcServer::onPoolJson(const COMMAND_RPC_GET_POOL::request &req,
							   COMMAND_RPC_GET_POOL::response &res)
	{
		auto pool = m_core.getPoolTransactions();
		for (const Transaction tx : pool) {
			TRANSACTION_SHORT_RESPONSE transaction_short;

			uint64_t amount_in = getInputAmount(tx);
			uint64_t amount_out = getOutputAmount(tx);
			bool amount_in_less = (amount_in < amount_out + parameters::MINIMUM_FEE);

			transaction_short.hash = Common::podToHex(getObjectHash(tx));
			transaction_short.fee = amount_in_less ? parameters::MINIMUM_FEE : amount_in - amount_out;
			transaction_short.amount_out = amount_out;
			transaction_short.size = getObjectBinarySize(tx);
			res.transactions.push_back(transaction_short);
		}

		res.status = CORE_RPC_STATUS_OK;

		return true;
	}

	bool RpcServer::onMempoolJson(const COMMAND_RPC_GET_MEMPOOL::request &req,
								  COMMAND_RPC_GET_MEMPOOL::response &res)
	{
		auto pool = m_core.getMemoryPool();
		for (const CryptoNote::tx_memory_pool::TransactionDetails txd : pool) {
			MEMPOOL_TRANSACTION_RESPONSE mempool_transaction;
			uint64_t amount_out = getOutputAmount(txd.tx);

			mempool_transaction.hash = Common::podToHex(txd.id);
			mempool_transaction.fee = txd.fee;
			mempool_transaction.amount_out = amount_out;
			mempool_transaction.size = txd.blobSize;
			mempool_transaction.receiveTime = txd.receiveTime;
			mempool_transaction.keptByBlock = txd.keptByBlock;
			mempool_transaction.max_used_block_height = txd.maxUsedBlock.height;
			mempool_transaction.max_used_block_id = Common::podToHex(txd.maxUsedBlock.id);
			mempool_transaction.last_failed_height = txd.lastFailedBlock.height;
			mempool_transaction.last_failed_id = Common::podToHex(txd.lastFailedBlock.id);
			mempool_transaction.tx_json = txd.tx;
			res.transactions.push_back(mempool_transaction);
		}

		res.status = CORE_RPC_STATUS_OK;

		return true;
	}

	bool RpcServer::onGetMixin(const Transaction &transaction, uint64_t &mixin)
	{
		mixin = 0;

		for (const TransactionInput &txin : transaction.inputs) {
			if (txin.type() != typeid(KeyInput)) {
				continue;
			}
			uint64_t currentMixin = boost::get<KeyInput>(txin).outputIndexes.size();
			if (currentMixin > mixin) {
				mixin = currentMixin;
			}
		}

		return true;
	}

	bool RpcServer::onTransactionsByPaymentId(
			const COMMAND_RPC_GET_TRANSACTIONS_BY_PAYMENT_ID::request &req,
			COMMAND_RPC_GET_TRANSACTIONS_BY_PAYMENT_ID::response &res)
	{
		if (req.payment_id.empty()) {
			throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_WRONG_PARAM,
										"Wrong parameters, expected payment_id"};
		}
		logger(Logging::DEBUGGING, Logging::WHITE)
				<< "RPC request came: Search by Payment ID: " << req.payment_id;

		Crypto::Hash paymentId;
		std::vector<Transaction> transactions;

		if (!parse_hash256(req.payment_id, paymentId)) {
			throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_WRONG_PARAM,
										"Failed to parse Payment ID: " + req.payment_id + '.'};
		}

		if (!m_core.getTransactionsByPaymentId(paymentId, transactions)) {
			throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_INTERNAL_ERROR,
										"Internal error: can't get transactions by Payment ID: "
										+ req.payment_id + '.'};
		}

		for (const Transaction &tx : transactions) {
			TRANSACTION_SHORT_RESPONSE transaction_short;
			uint64_t amount_in = 0;
			get_inputs_money_amount(tx, amount_in);
			uint64_t amount_out = get_outs_money_amount(tx);

			transaction_short.hash = Common::podToHex(getObjectHash(tx));
			transaction_short.fee = amount_in - amount_out;
			transaction_short.amount_out = amount_out;
			transaction_short.size = getObjectBinarySize(tx);
			res.transactions.push_back(transaction_short);
		}

		res.status = CORE_RPC_STATUS_OK;

		return true;
	}

	bool RpcServer::onTransactionsPoolJson(const COMMAND_RPC_GET_POOL::request &req,
										   COMMAND_RPC_GET_POOL::response &res)
	{
		auto pool = m_core.getPoolTransactions();
		for (const Transaction &tx : pool) {
			TRANSACTION_SHORT_RESPONSE transaction_short;
			uint64_t amount_in = getInputAmount(tx);
			uint64_t amount_out = getOutputAmount(tx);

			transaction_short.hash = Common::podToHex(getObjectHash(tx));
			transaction_short.fee = amount_in - amount_out;
			transaction_short.amount_out = amount_out;
			transaction_short.size = getObjectBinarySize(tx);
			res.transactions.push_back(transaction_short);
		}

		res.status = CORE_RPC_STATUS_OK;

		return true;
	}

	bool RpcServer::onGetBlockCount(const COMMAND_RPC_GET_BLOCK_COUNT::request &req,
									COMMAND_RPC_GET_BLOCK_COUNT::response &res)
	{
		res.count = m_core.get_current_blockchain_height();
		res.status = CORE_RPC_STATUS_OK;

		return true;
	}

	bool RpcServer::onGetBlockHash(const COMMAND_RPC_GET_BLOCK_HASH::request &req,
								   COMMAND_RPC_GET_BLOCK_HASH::response &res)
	{
		if (req.size() != 1) {
			throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_WRONG_PARAM,
										"Wrong parameters, expected height"};
		}

		uint32_t h = static_cast<uint32_t>(req[0]);
		Crypto::Hash blockId = m_core.getBlockIdByHeight(h);
		if (blockId == NULL_HASH) {
			throw JsonRpc::JsonRpcError{
					CORE_RPC_ERROR_CODE_TOO_BIG_HEIGHT,
					std::string("To big height: ") + std::to_string(h) + ", current blockchain height = "
					+ std::to_string(m_core.get_current_blockchain_height())
			};
		}

		res = Common::podToHex(blockId);

		return true;
	}

	namespace {

		uint64_t slowMemMem(void *start_buff, size_t buflen, void *pat, size_t patlen)
		{
			void *buf = start_buff;
			void *end = (char *) buf + buflen - patlen;
			while ((buf = memchr(buf, ((char *) pat)[0], buflen))) {
				if (buf > end) {
					return 0;
				}
				if (memcmp(buf, pat, patlen) == 0) {
					return (char *) buf - (char *) start_buff;
				}
				buf = (char *) buf + 1;
			}
			return 0;
		}

	} // namespace

	bool RpcServer::onGetBlockTemplate(const COMMAND_RPC_GET_BLOCK_TEMPLATE::request &req,
									   COMMAND_RPC_GET_BLOCK_TEMPLATE::response &res)
	{
		if (req.reserve_size > TX_EXTRA_NONCE_MAX_COUNT) {
			throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_TOO_BIG_RESERVE_SIZE,
										"To big reserved size, maximum 255"};
		}

		AccountPublicAddress acc = boost::value_initialized<AccountPublicAddress>();

		if (!req.wallet_address.size()
			|| !m_core.currency().parseAccountAddressString(req.wallet_address, acc)) {
			throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_WRONG_WALLET_ADDRESS,
										"Failed to parse wallet address"};
		}

		Block b = boost::value_initialized<Block>();
		CryptoNote::BinaryArray blob_reserve;
		blob_reserve.resize(req.reserve_size, 0);
		if (!m_core.get_block_template(b, acc, res.difficulty, res.height, blob_reserve)) {
			logger(ERROR) << "Failed to create block template";
			throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_INTERNAL_ERROR,
										"Internal error: failed to create block template"};
		}

		BinaryArray block_blob = toBinaryArray(b);
		PublicKey tx_pub_key = CryptoNote::getTransactionPublicKeyFromExtra(b.baseTransaction.extra);
		if (tx_pub_key == NULL_PUBLIC_KEY) {
			logger(ERROR) << "Failed to find tx pub key in coinbase extra";
			throw JsonRpc::JsonRpcError{
					CORE_RPC_ERROR_CODE_INTERNAL_ERROR,
					"Internal error: failed to find tx pub key in coinbase extra"
			};
		}

		if (0 < req.reserve_size) {
			res.reserved_offset = slowMemMem((void *) block_blob.data(), block_blob.size(), &tx_pub_key,
											 sizeof(tx_pub_key));
			if (!res.reserved_offset) {
				logger(ERROR) << "Failed to find tx pub key in blockblob";
				throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_INTERNAL_ERROR,
											"Internal error: failed to create block template"};
			}
			// 3 bytes:
			//   - tag for TX_EXTRA_TAG_PUBKEY(1 byte)
			//   - tag for TX_EXTRA_NONCE(1 byte)
			//   - counter in TX_EXTRA_NONCE(1 byte)
			res.reserved_offset += sizeof(tx_pub_key) + 3;
			if (res.reserved_offset + req.reserve_size > block_blob.size()) {
				logger(ERROR) << "Failed to calculate offset for reserved bytes";
				throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_INTERNAL_ERROR,
											"Internal error: failed to create block template"};
			}
		}
		else {
			res.reserved_offset = 0;
		}

		BinaryArray hashing_blob;
		if (!get_block_hashing_blob(b, hashing_blob)) {
			logger(ERROR) << "Failed to get blockhashing_blob";
			throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_INTERNAL_ERROR,
										"Internal error: failed to get blockhashing_blob"};
		}

		res.blocktemplate_blob = toHex(block_blob);
		res.blockhashing_blob = toHex(hashing_blob);
		res.status = CORE_RPC_STATUS_OK;

		return true;
	}

	bool RpcServer::onGetCurrencyId(const COMMAND_RPC_GET_CURRENCY_ID::request & /*req*/,
									COMMAND_RPC_GET_CURRENCY_ID::response &res)
	{
		Hash currencyId = m_core.currency().genesisBlockHash();
		res.currency_id_blob = Common::podToHex(currencyId);

		return true;
	}

	bool RpcServer::onSubmitBlock(const COMMAND_RPC_SUBMIT_BLOCK::request &req,
								  COMMAND_RPC_SUBMIT_BLOCK::response &res)
	{
		if (req.size() != 1) {
			throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_WRONG_PARAM, "Wrong param"};
		}

		BinaryArray blockblob;
		if (!fromHex(req[0], blockblob)) {
			throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_WRONG_BLOCKBLOB, "Wrong block blob"};
		}

		block_verification_context bvc = boost::value_initialized<block_verification_context>();

		m_core.handle_incoming_block_blob(blockblob, bvc, true, true);

		if (!bvc.m_added_to_main_chain) {
			throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_BLOCK_NOT_ACCEPTED,
										"Block not accepted"};
		}

		res.status = CORE_RPC_STATUS_OK;

		return true;
	}

	namespace {

		uint64_t getBlockReward(const Block &blk)
		{
			uint64_t reward = 0;
			for (const TransactionOutput &out : blk.baseTransaction.outputs) {
				reward += out.amount;
			}

			return reward;
		}

	} // namespace

	void RpcServer::fillBlockHeaderResponse(const Block &blk, bool orphan_status, uint64_t height,
											const Hash &hash, BLOCK_HEADER_RESPONSE_ENTRY &responce)
	{
		responce.major_version = blk.majorVersion;
		responce.minor_version = blk.minorVersion;
		responce.timestamp = blk.timestamp;
		responce.prev_hash = Common::podToHex(blk.previousBlockHash);
		responce.nonce = blk.nonce;
		responce.orphan_status = orphan_status;
		responce.height = height;
		responce.depth = m_core.get_current_blockchain_height() - height - 1;
		responce.hash = Common::podToHex(hash);
		m_core.getBlockDifficulty(static_cast<uint32_t>(height), responce.difficulty);
		responce.reward = getBlockReward(blk);
	}

	bool RpcServer::onGetLastBlockHeader(const COMMAND_RPC_GET_LAST_BLOCK_HEADER::request &req,
										 COMMAND_RPC_GET_LAST_BLOCK_HEADER::response &res)
	{
		uint32_t last_block_height;
		Hash last_block_hash;

		m_core.get_blockchain_top(last_block_height, last_block_hash);

		Block last_block;
		if (!m_core.getBlockByHash(last_block_hash, last_block)) {
			throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_INTERNAL_ERROR,
										"Internal error: can't get last block hash."};
		}

		Crypto::Hash tmp_hash = m_core.getBlockIdByHeight(last_block_height);
		bool is_orphaned = last_block_hash != tmp_hash;
		fillBlockHeaderResponse(last_block, is_orphaned, last_block_height, last_block_hash,
								res.block_header);
		res.status = CORE_RPC_STATUS_OK;

		return true;
	}

	bool RpcServer::onGetBlockHeaderByHash(
			const COMMAND_RPC_GET_BLOCK_HEADER_BY_HASH::request &req,
			COMMAND_RPC_GET_BLOCK_HEADER_BY_HASH::response &res)
	{
		Hash block_hash;

		if (!parse_hash256(req.hash, block_hash)) {
			throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_WRONG_PARAM,
										"Failed to parse hex representation of block hash. Hex = "
										+ req.hash + '.'};
		}

		Block blk;
		if (!m_core.getBlockByHash(block_hash, blk)) {
			throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_INTERNAL_ERROR,
										"Internal error: can't get block by hash. Hash = " + req.hash
										+ '.'};
		}

		if (blk.baseTransaction.inputs.front().type() != typeid(BaseInput)) {
			throw JsonRpc::JsonRpcError{
					CORE_RPC_ERROR_CODE_INTERNAL_ERROR,
					"Internal error: coinbase transaction in the block has the wrong type"
			};
		}

		uint64_t block_height = boost::get<BaseInput>(blk.baseTransaction.inputs.front()).blockIndex;
		Crypto::Hash tmp_hash = m_core.getBlockIdByHeight(block_height);
		bool is_orphaned = block_hash != tmp_hash;
		fillBlockHeaderResponse(blk, is_orphaned, block_height, block_hash, res.block_header);
		res.status = CORE_RPC_STATUS_OK;

		return true;
	}

	bool RpcServer::onGetBlockHeaderByHeight(
			const COMMAND_RPC_GET_BLOCK_HEADER_BY_HEIGHT::request &req,
			COMMAND_RPC_GET_BLOCK_HEADER_BY_HEIGHT::response &res)
	{
		if (m_core.get_current_blockchain_height() <= req.height) {
			throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_TOO_BIG_HEIGHT,
										std::string("To big height: ") + std::to_string(req.height)
										+ ", current blockchain height = "
										+ std::to_string(
												m_core.get_current_blockchain_height())};
		}

		Hash block_hash = m_core.getBlockIdByHeight(static_cast<uint32_t>(req.height));
		Block blk;
		if (!m_core.getBlockByHash(block_hash, blk)) {
			throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_INTERNAL_ERROR,
										"Internal error: can't get block by height. Height = "
										+ std::to_string(req.height) + '.'};
		}

		Crypto::Hash tmp_hash = m_core.getBlockIdByHeight(req.height);
		bool is_orphaned = block_hash != tmp_hash;
		fillBlockHeaderResponse(blk, is_orphaned, req.height, block_hash, res.block_header);
		res.status = CORE_RPC_STATUS_OK;

		return true;
	}

	bool RpcServer::onCheckTxKey(const COMMAND_RPC_CHECK_TX_KEY::request &req,
								 COMMAND_RPC_CHECK_TX_KEY::response &res)
	{
		// parse txid
		Crypto::Hash txid;
		if (!parse_hash256(req.txid, txid)) {
			throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_WRONG_PARAM, "Failed to parse txid"};
		}

		// parse address
		CryptoNote::AccountPublicAddress address;
		if (!m_core.currency().parseAccountAddressString(req.address, address)) {
			throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_WRONG_PARAM,
										"Failed to parse address " + req.address + '.'};
		}

		// parse txkey
		Crypto::Hash tx_key_hash;
		size_t size;
		if (!Common::fromHex(req.txkey, &tx_key_hash, sizeof(tx_key_hash), size)
			|| size != sizeof(tx_key_hash)) {
			throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_WRONG_PARAM, "Failed to parse txkey"};
		}
		Crypto::SecretKey tx_key = *(struct Crypto::SecretKey *) &tx_key_hash;

		// fetch tx
		Transaction tx;
		std::vector<Crypto::Hash> tx_ids;
		tx_ids.push_back(txid);
		std::list<Crypto::Hash> missed_txs;
		std::list<Transaction> txs;
		m_core.getTransactions(tx_ids, txs, missed_txs, true);

		if (1 == txs.size()) {
			tx = txs.front();
		}
		else {
			throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_WRONG_PARAM,
										"Couldn't find transaction with hash: " + req.txid + '.'};
		}
		CryptoNote::TransactionPrefix transaction = *static_cast<const TransactionPrefix *>(&tx);

		// obtain key derivation
		Crypto::KeyDerivation derivation;
		if (!Crypto::generate_key_derivation(address.viewPublicKey, tx_key, derivation)) {
			throw JsonRpc::JsonRpcError{
					CORE_RPC_ERROR_CODE_WRONG_PARAM,
					"Failed to generate key derivation from supplied parameters"
			};
		}

		// look for outputs
		uint64_t received(0);
		size_t keyIndex(0);
		std::vector<TransactionOutput> outputs;
		try {
			for (const TransactionOutput &o : transaction.outputs) {
				if (o.target.type() == typeid(KeyOutput)) {
					const KeyOutput out_key = boost::get<KeyOutput>(o.target);
					Crypto::PublicKey pubkey;
					derive_public_key(derivation, keyIndex, address.spendPublicKey, pubkey);
					if (pubkey == out_key.key) {
						received += o.amount;
						outputs.push_back(o);
					}
				}
				++keyIndex;
			}
		} catch (...) {
			throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_INTERNAL_ERROR, "Unknown error"};
		}

		res.amount = received;
		res.outputs = outputs;
		res.status = CORE_RPC_STATUS_OK;

		return true;
	}

	bool RpcServer::onCheckTxWithViewKey(
			const COMMAND_RPC_CHECK_TX_WITH_PRIVATE_VIEW_KEY::request &req,
			COMMAND_RPC_CHECK_TX_WITH_PRIVATE_VIEW_KEY::response &res)
	{
		// parse txid
		Crypto::Hash txid;
		if (!parse_hash256(req.txid, txid)) {
			throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_WRONG_PARAM, "Failed to parse txid"};
		}

		// parse address
		CryptoNote::AccountPublicAddress address;
		if (!m_core.currency().parseAccountAddressString(req.address, address)) {
			throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_WRONG_PARAM,
										"Failed to parse address " + req.address + '.'};
		}

		// parse view key
		Crypto::Hash view_key_hash;
		size_t size;
		if (!Common::fromHex(req.view_key, &view_key_hash, sizeof(view_key_hash), size)
			|| size != sizeof(view_key_hash)) {
			throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_WRONG_PARAM,
										"Failed to parse private view key"};
		}
		Crypto::SecretKey viewKey = *(struct Crypto::SecretKey *) &view_key_hash;

		// fetch tx
		Transaction tx;
		std::vector<Crypto::Hash> tx_ids;
		tx_ids.push_back(txid);
		std::list<Crypto::Hash> missed_txs;
		std::list<Transaction> txs;
		m_core.getTransactions(tx_ids, txs, missed_txs, true);

		if (1 == txs.size()) {
			tx = txs.front();
		}
		else {
			throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_WRONG_PARAM,
										"Couldn't find transaction with hash: " + req.txid + '.'};
		}
		CryptoNote::TransactionPrefix transaction = *static_cast<const TransactionPrefix *>(&tx);

		// get tx pub key
		Crypto::PublicKey txPubKey = getTransactionPublicKeyFromExtra(transaction.extra);

		// obtain key derivation
		Crypto::KeyDerivation derivation;
		if (!Crypto::generate_key_derivation(txPubKey, viewKey, derivation)) {
			throw JsonRpc::JsonRpcError{
					CORE_RPC_ERROR_CODE_WRONG_PARAM,
					"Failed to generate key derivation from supplied parameters"
			};
		}

		// look for outputs
		uint64_t received(0);
		size_t keyIndex(0);
		std::vector<TransactionOutput> outputs;
		try {
			for (const TransactionOutput &o : transaction.outputs) {
				if (o.target.type() == typeid(KeyOutput)) {
					const KeyOutput out_key = boost::get<KeyOutput>(o.target);
					Crypto::PublicKey pubkey;
					derive_public_key(derivation, keyIndex, address.spendPublicKey, pubkey);
					if (pubkey == out_key.key) {
						received += o.amount;
						outputs.push_back(o);
					}
				}
				++keyIndex;
			}
		} catch (...) {
			throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_INTERNAL_ERROR, "Unknown error"};
		}
		res.amount = received;
		res.outputs = outputs;

		Crypto::Hash blockHash;
		uint32_t blockHeight;
		if (m_core.getBlockContainingTx(txid, blockHash, blockHeight)) {
			res.confirmations = m_protocolQuery.getObservedHeight() - blockHeight;
		}

		res.status = CORE_RPC_STATUS_OK;

		return true;
	}

	bool RpcServer::onCheckTxProof(const COMMAND_RPC_CHECK_TX_PROOF::request &req,
								   COMMAND_RPC_CHECK_TX_PROOF::response &res)
	{
		// parse txid
		Crypto::Hash txid;
		if (!parse_hash256(req.tx_id, txid)) {
			throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_WRONG_PARAM, "Failed to parse txid"};
		}

		// parse address
		CryptoNote::AccountPublicAddress address;
		if (!m_core.currency().parseAccountAddressString(req.dest_address, address)) {
			throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_WRONG_PARAM,
										"Failed to parse address " + req.dest_address + '.'};
		}

		// parse pubkey r*A & signature
		const size_t header_len = strlen("ProofV1");
		if (req.signature.size() < header_len || req.signature.substr(0, header_len) != "ProofV1") {
			throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_WRONG_PARAM,
										"Signature header check error"};
		}
		Crypto::PublicKey rA;
		Crypto::Signature sig;
		const size_t rA_len =
				Tools::Base58::encode(std::string((const char *) &rA, sizeof(Crypto::PublicKey))).size();
		const size_t sig_len =
				Tools::Base58::encode(std::string((const char *) &sig, sizeof(Crypto::Signature)))
						.size();
		std::string rA_decoded;
		std::string sig_decoded;
		if (!Tools::Base58::decode(req.signature.substr(header_len, rA_len), rA_decoded)) {
			throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_WRONG_PARAM, "Signature decoding error"};
		}
		if (!Tools::Base58::decode(req.signature.substr(header_len + rA_len, sig_len), sig_decoded)) {
			throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_WRONG_PARAM, "Signature decoding error"};
		}
		if (sizeof(Crypto::PublicKey) != rA_decoded.size()
			|| sizeof(Crypto::Signature) != sig_decoded.size()) {
			throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_WRONG_PARAM, "Signature decoding error"};
		}
		memcpy(&rA, rA_decoded.data(), sizeof(Crypto::PublicKey));
		memcpy(&sig, sig_decoded.data(), sizeof(Crypto::Signature));

		// fetch tx pubkey
		Transaction tx;

		std::vector<uint32_t> out;
		std::vector<Crypto::Hash> tx_ids;
		tx_ids.push_back(txid);
		std::list<Crypto::Hash> missed_txs;
		std::list<Transaction> txs;
		m_core.getTransactions(tx_ids, txs, missed_txs, true);

		if (1 == txs.size()) {
			tx = txs.front();
		}
		else {
			throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_WRONG_PARAM,
										"transaction wasn't found. Hash = " + req.tx_id + '.'};
		}
		CryptoNote::TransactionPrefix transaction = *static_cast<const TransactionPrefix *>(&tx);

		Crypto::PublicKey R = getTransactionPublicKeyFromExtra(transaction.extra);
		if (R == NULL_PUBLIC_KEY) {
			throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_INTERNAL_ERROR,
										"Tx pubkey was not found"};
		}

		// check signature
		bool r = Crypto::check_tx_proof(txid, R, address.viewPublicKey, rA, sig);
		res.signature_valid = r;

		if (r) {
			// obtain key derivation by multiplying scalar 1 to the pubkey r*A included in the signature
			Crypto::KeyDerivation derivation;
			if (!Crypto::generate_key_derivation(rA, I, derivation)) {
				throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_INTERNAL_ERROR,
											"Failed to generate key derivation"};
			}

			// look for outputs
			uint64_t received(0);
			size_t keyIndex(0);
			std::vector<TransactionOutput> outputs;
			try {
				for (const TransactionOutput &o : transaction.outputs) {
					if (o.target.type() == typeid(KeyOutput)) {
						const KeyOutput out_key = boost::get<KeyOutput>(o.target);
						Crypto::PublicKey pubkey;
						derive_public_key(derivation, keyIndex, address.spendPublicKey, pubkey);
						if (pubkey == out_key.key) {
							received += o.amount;
							outputs.push_back(o);
						}
					}
					++keyIndex;
				}
			} catch (...) {
				throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_INTERNAL_ERROR, "Unknown error"};
			}
			res.received_amount = received;
			res.outputs = outputs;

			Crypto::Hash blockHash;
			uint32_t blockHeight;
			if (m_core.getBlockContainingTx(txid, blockHash, blockHeight)) {
				res.confirmations = m_protocolQuery.getObservedHeight() - blockHeight;
			}
		}
		else {
			res.received_amount = 0;
		}

		res.status = CORE_RPC_STATUS_OK;

		return true;
	}

	bool RpcServer::onCheckReserveProof(const COMMAND_RPC_CHECK_RESERVE_PROOF::request &req,
										COMMAND_RPC_CHECK_RESERVE_PROOF::response &res)
	{
		// parse address
		CryptoNote::AccountPublicAddress address;
		if (!m_core.currency().parseAccountAddressString(req.address, address)) {
			throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_WRONG_PARAM,
										"Failed to parse address " + req.address + '.'};
		}

		// parse sugnature
		static constexpr char header[] = "ReserveProofV1";
		const size_t header_len = strlen(header);
		if (req.signature.size() < header_len || req.signature.substr(0, header_len) != header) {
			throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_INTERNAL_ERROR,
										"Signature header check error"};
		}

		std::string sig_decoded;
		if (!Tools::Base58::decode(req.signature.substr(header_len), sig_decoded)) {
			throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_INTERNAL_ERROR,
										"Signature decoding error"};
		}

		BinaryArray ba;
		if (!Common::fromHex(sig_decoded, ba)) {
			throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_INTERNAL_ERROR, "Proof decoding error"};
		}

		RESERVE_PROOF proof_decoded;
		if (!fromBinaryArray(proof_decoded, ba)) {
			throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_INTERNAL_ERROR,
										"BinaryArray decoding error"};
		}

		std::vector<RESERVE_PROOF_ENTRY> &proofs = proof_decoded.proofs;

		// compute signature prefix hash
		std::string prefix_data = req.message;
		prefix_data.append((const char *) &address, sizeof(CryptoNote::AccountPublicAddress));
		for (size_t i = 0; i < proofs.size(); ++i) {
			prefix_data.append((const char *) &proofs[i].key_image, sizeof(Crypto::PublicKey));
		}
		Crypto::Hash prefix_hash;
		Crypto::cn_fast_hash(prefix_data.data(), prefix_data.size(), prefix_hash);

		// fetch txes
		std::vector<Crypto::Hash> transactionHashes;
		for (size_t i = 0; i < proofs.size(); ++i) {
			transactionHashes.push_back(proofs[i].txid);
		}
		std::list<Hash> missed_txs;
		std::list<Transaction> txs;
		m_core.getTransactions(transactionHashes, txs, missed_txs);
		std::vector<Transaction> transactions;
		std::copy(txs.begin(), txs.end(), std::inserter(transactions, transactions.end()));

		// check spent status
		res.total = 0;
		res.spent = 0;
		for (size_t i = 0; i < proofs.size(); ++i) {
			const RESERVE_PROOF_ENTRY &proof = proofs[i];

			CryptoNote::TransactionPrefix tx =
					*static_cast<const TransactionPrefix *>(&transactions[i]);

			if (proof.index_in_tx >= tx.outputs.size()) {
				throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_INTERNAL_ERROR,
											"index_in_tx is out of bound"};
			}

			const KeyOutput out_key = boost::get<KeyOutput>(tx.outputs[proof.index_in_tx].target);

			// get tx pub key
			Crypto::PublicKey txPubKey = getTransactionPublicKeyFromExtra(tx.extra);

			// check singature for shared secret
			if (!Crypto::check_tx_proof(prefix_hash, address.viewPublicKey, txPubKey,
										proof.shared_secret, proof.shared_secret_sig)) {
				// throw JsonRpc::JsonRpcError{
				//    CORE_RPC_ERROR_CODE_INTERNAL_ERROR,
				//    "Failed to check singature for shared secret"
				//};
				res.good = false;
				return true;
			}

			// check signature for key image
			const std::vector<const Crypto::PublicKey *> &pubs = {&out_key.key};
			if (!Crypto::check_ring_signature(prefix_hash, proof.key_image, &pubs[0], 1,
											  &proof.key_image_sig)) {
				// throw JsonRpc::JsonRpcError{
				//    CORE_RPC_ERROR_CODE_INTERNAL_ERROR,
				//    "Failed to check signature for key image"
				//};
				res.good = false;
				return true;
			}

			// check if the address really received the fund
			Crypto::KeyDerivation derivation;
			if (!Crypto::generate_key_derivation(proof.shared_secret, I, derivation)) {
				throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_INTERNAL_ERROR,
											"Failed to generate key derivation"};
			}

			try {
				Crypto::PublicKey pubkey;
				derive_public_key(derivation, proof.index_in_tx, address.spendPublicKey, pubkey);
				if (pubkey == out_key.key) {
					uint64_t amount = tx.outputs[proof.index_in_tx].amount;
					res.total += amount;

					if (m_core.is_key_image_spent(proof.key_image)) {
						res.spent += amount;
					}
				}
			} catch (...) {
				throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_INTERNAL_ERROR, "Unknown error"};
			}
		}

		// check signature for address spend keys
		Crypto::Signature sig = proof_decoded.signature;
		if (!Crypto::check_signature(prefix_hash, address.spendPublicKey, sig)) {
			res.good = false;
			return true;
		}

		return true;
	}

	bool RpcServer::onValidateAddress(const COMMAND_RPC_VALIDATE_ADDRESS::request &req,
									  COMMAND_RPC_VALIDATE_ADDRESS::response &res)
	{
		AccountPublicAddress acc = boost::value_initialized<AccountPublicAddress>();
		bool r = m_core.currency().parseAccountAddressString(req.address, acc);
		res.isvalid = r;
		if (r) {
			res.address = m_core.currency().accountAddressAsString(acc);
			res.spendPublicKey = Common::podToHex(acc.spendPublicKey);
			res.viewPublicKey = Common::podToHex(acc.viewPublicKey);
		}
		res.status = CORE_RPC_STATUS_OK;
		return true;
	}

	bool RpcServer::onVerifyMessage(const COMMAND_RPC_VERIFY_MESSAGE::request &req,
									COMMAND_RPC_VERIFY_MESSAGE::response &res)
	{
		Crypto::Hash hash;
		Crypto::cn_fast_hash(req.message.data(), req.message.size(), hash);

		AccountPublicAddress acc = boost::value_initialized<AccountPublicAddress>();
		if (!m_core.currency().parseAccountAddressString(req.address, acc)) {
			throw JsonRpc::JsonRpcError(CORE_RPC_ERROR_CODE_WRONG_PARAM,
										std::string("Failed to parse address"));
		}

		const size_t header_len = strlen("SigV1");
		if (req.signature.size() < header_len || req.signature.substr(0, header_len) != "SigV1") {
			throw JsonRpc::JsonRpcError(CORE_RPC_ERROR_CODE_WRONG_PARAM,
										std::string("Signature header check error"));
		}

		std::string decoded;
		Crypto::Signature s;
		if (!Tools::Base58::decode(req.signature.substr(header_len), decoded)
			|| sizeof(s) != decoded.size()) {
			throw JsonRpc::JsonRpcError(CORE_RPC_ERROR_CODE_WRONG_PARAM,
										std::string("Signature decoding error"));
			return false;
		}

		memcpy(&s, decoded.data(), sizeof(s));

		res.sig_valid = Crypto::check_signature(hash, acc.spendPublicKey, s);
		res.status = CORE_RPC_STATUS_OK;

		return true;
	}

	bool RpcServer::onGetDifficultyStat(const COMMAND_RPC_GET_DIFFICULTY_STAT::request &req,
										COMMAND_RPC_GET_DIFFICULTY_STAT::response &res)
	{
		try {
			if (!m_core.get_difficulty_stat(req.height, IMinerHandler::stat_period::hour,
											res.hour.block_num, res.hour.avg_solve_time,
											res.hour.stddev_solve_time, res.hour.outliers_num,
											res.hour.avg_diff, res.hour.min_diff, res.hour.max_diff))
				throw std::runtime_error("Failed to get hour difficulty statistics");
			if (!m_core.get_difficulty_stat(req.height, IMinerHandler::stat_period::day,
											res.day.block_num, res.day.avg_solve_time,
											res.day.stddev_solve_time, res.day.outliers_num,
											res.day.avg_diff, res.day.min_diff, res.day.max_diff))
				throw std::runtime_error("Failed to get day difficulty statistics");
			if (!m_core.get_difficulty_stat(req.height, IMinerHandler::stat_period::week,
											res.week.block_num, res.week.avg_solve_time,
											res.week.stddev_solve_time, res.week.outliers_num,
											res.week.avg_diff, res.week.min_diff, res.week.max_diff))
				throw std::runtime_error("Failed to get week difficulty statistics");
			if (!m_core.get_difficulty_stat(req.height, IMinerHandler::stat_period::month,
											res.month.block_num, res.month.avg_solve_time,
											res.month.stddev_solve_time, res.month.outliers_num,
											res.month.avg_diff, res.month.min_diff, res.month.max_diff))
				throw std::runtime_error("Failed to get month difficulty statistics");
			if (!m_core.get_difficulty_stat(req.height, IMinerHandler::stat_period::halfyear,
											res.halfyear.block_num, res.halfyear.avg_solve_time,
											res.halfyear.stddev_solve_time, res.halfyear.outliers_num,
											res.halfyear.avg_diff, res.halfyear.min_diff,
											res.halfyear.max_diff))
				throw std::runtime_error("Failed to get halfyear difficulty statistics");
			if (!m_core.get_difficulty_stat(req.height, IMinerHandler::stat_period::year,
											res.year.block_num, res.year.avg_solve_time,
											res.year.stddev_solve_time, res.year.outliers_num,
											res.year.avg_diff, res.year.min_diff, res.year.max_diff))
				throw std::runtime_error("Failed to get year difficulty statistics");

			res.blocks30.block_num = 30;
			if (!m_core.get_difficulty_stat(req.height, IMinerHandler::stat_period::by_block_number,
											res.blocks30.block_num, res.blocks30.avg_solve_time,
											res.blocks30.stddev_solve_time, res.blocks30.outliers_num,
											res.blocks30.avg_diff, res.blocks30.min_diff,
											res.blocks30.max_diff))
				throw std::runtime_error("Failed to get difficulty statistics for 30 blocks");
			res.blocks720.block_num = 720;
			if (!m_core.get_difficulty_stat(req.height, IMinerHandler::stat_period::by_block_number,
											res.blocks720.block_num, res.blocks720.avg_solve_time,
											res.blocks720.stddev_solve_time, res.blocks720.outliers_num,
											res.blocks720.avg_diff, res.blocks720.min_diff,
											res.blocks720.max_diff))
				throw std::runtime_error("Failed to get difficulty statistics for 720 blocks");
			res.blocks5040.block_num = 5040;
			if (!m_core.get_difficulty_stat(req.height, IMinerHandler::stat_period::by_block_number,
											res.blocks5040.block_num, res.blocks5040.avg_solve_time,
											res.blocks5040.stddev_solve_time,
											res.blocks5040.outliers_num, res.blocks5040.avg_diff,
											res.blocks5040.min_diff, res.blocks5040.max_diff))
				throw std::runtime_error("Failed to get difficulty statistics for 5040 blocks");
			res.blocks21900.block_num = 21900;
			if (!m_core.get_difficulty_stat(req.height, IMinerHandler::stat_period::by_block_number,
											res.blocks21900.block_num, res.blocks21900.avg_solve_time,
											res.blocks21900.stddev_solve_time,
											res.blocks21900.outliers_num, res.blocks21900.avg_diff,
											res.blocks21900.min_diff, res.blocks21900.max_diff))
				throw std::runtime_error("Failed to get difficulty statistics for 21900 blocks");
			res.blocks131400.block_num = 131400;
			if (!m_core.get_difficulty_stat(req.height, IMinerHandler::stat_period::by_block_number,
											res.blocks131400.block_num, res.blocks131400.avg_solve_time,
											res.blocks131400.stddev_solve_time,
											res.blocks131400.outliers_num, res.blocks131400.avg_diff,
											res.blocks131400.min_diff, res.blocks131400.max_diff))
				throw std::runtime_error("Failed to get difficulty statistics for 131400 blocks");
			res.blocks262800.block_num = 262800;
			if (!m_core.get_difficulty_stat(req.height, IMinerHandler::stat_period::by_block_number,
											res.blocks262800.block_num, res.blocks262800.avg_solve_time,
											res.blocks262800.stddev_solve_time,
											res.blocks262800.outliers_num, res.blocks262800.avg_diff,
											res.blocks262800.min_diff, res.blocks262800.max_diff))
				throw std::runtime_error("Failed to get difficulty statistics for 262800 blocks");
		} catch (std::system_error &e) {
			res.status = e.what();
			return false;
		} catch (std::exception &e) {
			res.status = "Error: " + std::string(e.what());
			return false;
		}

		res.status = CORE_RPC_STATUS_OK;

		return true;
	}

} // namespace CryptoNote
