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

#include <queue>

#include "IntrusiveLinkedList.h"

#include "System/Event.h"
#include "System/InterruptedException.h"

namespace CryptoNote {

template<class MessageType> class MessageQueue {
public:
  MessageQueue(System::Dispatcher& dispatcher);

  const MessageType& front();
  void pop();
  void push(const MessageType& message);

  void stop();

  typename IntrusiveLinkedList<MessageQueue<MessageType>>::hook& getHook();

private:
  void wait();
  std::queue<MessageType> messageQueue;
  System::Event event;
  bool stopped;

  typename IntrusiveLinkedList<MessageQueue<MessageType>>::hook hook;
};

template<class MessageQueueContainer, class MessageType>
class MesageQueueGuard {
public:
  MesageQueueGuard(MessageQueueContainer& container, MessageQueue<MessageType>& messageQueue) : container(container), messageQueue(messageQueue) {
    container.addMessageQueue(messageQueue);
  }

  MesageQueueGuard(const MesageQueueGuard& other) = delete;
  MesageQueueGuard& operator=(const MesageQueueGuard& other) = delete;

  ~MesageQueueGuard() {
    container.removeMessageQueue(messageQueue);
  }
private:
  MessageQueueContainer& container;
  MessageQueue<MessageType>& messageQueue;
};

template<class MessageType>
MessageQueue<MessageType>::MessageQueue(System::Dispatcher& dispatcher) : event(dispatcher), stopped(false) {}

template<class MessageType>
void MessageQueue<MessageType>::wait() {
  if (messageQueue.empty()) {
    if (stopped) {
      throw System::InterruptedException();
    }

    event.clear();
    while (!event.get()) {
      event.wait();
    }
  }
}

template<class MessageType>
const MessageType& MessageQueue<MessageType>::front() {
  wait();
  return messageQueue.front();
}

template<class MessageType>
void MessageQueue<MessageType>::pop() {
  wait();
  messageQueue.pop();
}

template<class MessageType>
void MessageQueue<MessageType>::push(const MessageType& message) {
  messageQueue.push(message);
  event.set();
}

template<class MessageType>
void MessageQueue<MessageType>::stop() {
  stopped = true;
  event.set();
}

template<class MessageType>
typename IntrusiveLinkedList<MessageQueue<MessageType>>::hook& MessageQueue<MessageType>::getHook() {
  return hook;
}

}
