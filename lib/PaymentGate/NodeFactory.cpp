// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2023, The Qwertycoin Group.
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

#include <future>
#include <memory>
#include <NodeRpcProxy/NodeRpcProxy.h>
#include <PaymentGate/NodeFactory.h>

namespace PaymentService {

class NodeRpcStub: public CryptoNote::INode
{
public:
    ~NodeRpcStub() override = default;

    bool addObserver(CryptoNote::INodeObserver *observer) override { return true; }
    bool removeObserver(CryptoNote::INodeObserver *observer) override { return true; }

    void init(const Callback &callback) override { }
    bool shutdown() override { return true; }

    size_t getPeerCount() const override { return 0; }
    uint32_t getLastLocalBlockHeight() const override { return 0; }
    uint32_t getLastKnownBlockHeight() const override { return 0; }
    uint32_t getLocalBlockCount() const override { return 0; }
    uint32_t getKnownBlockCount() const override { return 0; }
    uint64_t getLastLocalBlockTimestamp() const override { return 0; }
    uint32_t getNodeHeight() const override { return 0; }
    uint64_t getMinimalFee() const override{ return 0; }

    CryptoNote::BlockHeaderInfo getLastLocalBlockHeaderInfo() const override
    {
        return CryptoNote::BlockHeaderInfo{};
    }
    uint32_t getGRBHeight() const override { return 0; };

    void relayTransaction(const CryptoNote::Transaction &transaction,
                          const Callback &callback) override
    {
        callback(std::error_code());
    }

    void getRandomOutsByAmounts(
        std::vector<uint64_t> &&amounts,
        uint64_t outsCount,
        std::vector<CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>&result,
        const Callback &callback) override
    {
    }

    void getNewBlocks(
        std::vector<Crypto::Hash> &&knownBlockIds,
        std::vector<CryptoNote::block_complete_entry> &newBlocks,
        uint32_t &startHeight,
        const Callback &callback) override
    {
        startHeight = 0;
        callback(std::error_code());
    }

    void getTransactionOutsGlobalIndices(const Crypto::Hash &transactionHash,
                                         std::vector<uint32_t> &outsGlobalIndices,
                                         const Callback &callback) override
    {
    }

    void queryBlocks(std::vector<Crypto::Hash> &&knownBlockIds,
                     uint64_t timestamp,
                     std::vector<CryptoNote::BlockShortEntry> &newBlocks,
                     uint32_t &startHeight,
                     const Callback &callback) override
    {
        startHeight = 0;
        callback(std::error_code());
    };

    void getPoolSymmetricDifference(
        std::vector<Crypto::Hash> &&knownPoolTxIds,
        Crypto::Hash knownBlockId,
        bool &isBcActual,
        std::vector<std::unique_ptr<CryptoNote::ITransactionReader>> &newTxs,
        std::vector<Crypto::Hash> &deletedTxIds,
        const Callback &callback) override
    {
        isBcActual = true;
        callback(std::error_code());
    }

    void getBlocks(const std::vector<uint32_t> &blockHeights,
                   std::vector<std::vector<CryptoNote::BlockDetails>> &blocks,
                   const Callback &callback) override { }

    void getBlocks(const std::vector<Crypto::Hash> &blockHashes,
                   std::vector<CryptoNote::BlockDetails> &blocks,
                   const Callback &callback) override { }

    void getBlocks(uint64_t timestampBegin,
                   uint64_t timestampEnd,
                   uint32_t blocksNumberLimit,
                   std::vector<CryptoNote::BlockDetails> &blocks,
                   uint32_t &blocksNumberWithinTimestamps,
                   const Callback &callback) override { }

    void getTransactions(const std::vector<Crypto::Hash> &transactionHashes,
                         std::vector<CryptoNote::TransactionDetails> &transactions,
                         const Callback &callback) override { }

    void getPoolTransactions(uint64_t timestampBegin,
                             uint64_t timestampEnd,
                             uint32_t transactionsNumberLimit,
                             std::vector<CryptoNote::TransactionDetails> &transactions,
                             uint64_t &transactionsNumberWithinTimestamps,
                             const Callback &callback) override { }

    void getTransactionsByPaymentId(const Crypto::Hash &paymentId,
                                    std::vector<CryptoNote::TransactionDetails> &transactions,
                                    const Callback &callback) override { }

    void getMultisignatureOutputByGlobalIndex(uint64_t amount,
                                              uint32_t gindex,
                                              CryptoNote::MultisignatureOutput &out,
                                              const Callback &callback) override { }

    void isSynchronized(bool &syncStatus, const Callback &callback) override { }
};

class NodeInitObserver
{
public:
    NodeInitObserver()
    {
        initFuture = initPromise.get_future();
    }

    void initCompleted(std::error_code result)
    {
        initPromise.set_value(result);
    }

    void waitForInitEnd()
    {
        std::error_code ec = initFuture.get();
        if (ec) {
            throw std::system_error(ec);
        }
    }

private:
    std::promise<std::error_code> initPromise;
    std::future<std::error_code> initFuture;
};

CryptoNote::INode *NodeFactory::createNode(const std::string &daemonAddress, uint16_t daemonPort)
{
    std::unique_ptr<CryptoNote::INode> node(new CryptoNote::NodeRpcProxy(daemonAddress,daemonPort));

    NodeInitObserver initObserver;
    node->init(std::bind(&NodeInitObserver::initCompleted, &initObserver, std::placeholders::_1));
    initObserver.waitForInitEnd();

    return node.release();
}

CryptoNote::INode *NodeFactory::createNodeStub()
{
    return new NodeRpcStub();
}

} // namespace PaymentService
