// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The Qwertycoin developers
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

#include "TestNode.h"
#include "NetworkConfiguration.h"

#include <future>
#include <memory>
#include <thread>

#include <System/Dispatcher.h>


namespace CryptoNote {
class core;
class CryptoNoteProtocolHandler;
class NodeServer;
class Currency;
}

namespace Tests {

class InProcTestNode : public TestNode {
public:
  InProcTestNode(const TestNodeConfiguration& cfg, const CryptoNote::Currency& currency);
  ~InProcTestNode();

  virtual bool startMining(size_t threadsCount, const std::string &address) override;
  virtual bool stopMining() override;
  virtual bool stopDaemon() override;
  virtual bool getBlockTemplate(const std::string &minerAddress, CryptoNote::Block &blockTemplate, uint64_t &difficulty) override;
  virtual bool submitBlock(const std::string& block) override;
  virtual bool getTailBlockId(Crypto::Hash &tailBlockId) override;
  virtual bool makeINode(std::unique_ptr<CryptoNote::INode>& node) override;
  virtual uint64_t getLocalHeight() override;

private:

  void workerThread(std::promise<std::string>& initPromise);

  std::unique_ptr<CryptoNote::core> core;
  std::unique_ptr<CryptoNote::CryptoNoteProtocolHandler> protocol;
  std::unique_ptr<CryptoNote::NodeServer> p2pNode;

  std::thread m_thread;
  const CryptoNote::Currency& m_currency;
  TestNodeConfiguration m_cfg;
};

}
