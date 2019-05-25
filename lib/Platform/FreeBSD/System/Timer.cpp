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

#include <cassert>
#include <stdexcept>
#include <string>
#include <sys/errno.h>
#include <sys/event.h>
#include <sys/stdint.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <System/ErrorMessage.h>
#include <System/InterruptedException.h>
#include "Dispatcher.h"
#include "Timer.h"

namespace System {

Timer::Timer()
    : dispatcher(nullptr)
{
}

Timer::Timer(Dispatcher &dispatcher)
    : dispatcher(&dispatcher),
      context(nullptr),
      timer(-1)
{
}

Timer::Timer(Timer &&other) noexcept
    : dispatcher(other.dispatcher)
{
    if (other.dispatcher != nullptr) {
        assert(other.context == nullptr);
        timer = other.timer;
        context = nullptr;
        other.dispatcher = nullptr;
    }
}

Timer::~Timer()
{
    assert(dispatcher == nullptr || context == nullptr);
}

Timer &Timer::operator=(Timer &&other) noexcept
{
    assert(dispatcher == nullptr || context == nullptr);

    dispatcher = other.dispatcher;
    if (other.dispatcher != nullptr) {
        assert(other.context == nullptr);
        timer = other.timer;
        context = nullptr;
        other.dispatcher = nullptr;
        other.timer = -1;
    }

    return *this;
}

void Timer::sleep(std::chrono::nanoseconds duration)
{
    assert(dispatcher != nullptr);
    assert(context == nullptr);

    if (dispatcher->interrupted()) {
        throw InterruptedException();
    }

    OperationContext timerContext;
    timerContext.context = dispatcher->getCurrentContext();
    timerContext.interrupted = false;
    timer = dispatcher->getTimer();

    struct kevent event;
    EV_SET(&event,
           timer,
           EVFILT_TIMER,
           EV_ADD | EV_ENABLE | EV_ONESHOT,
           NOTE_NSECONDS,
           duration.count(),
           &timerContext);

    if (kevent(dispatcher->getKqueue(), &event, 1, nullptr, 0, nullptr) == -1) {
        throw std::runtime_error("Timer::stop, kevent failed, " + lastErrorMessage());
    }

    context = &timerContext;
    dispatcher->getCurrentContext()->interruptProcedure = [&] {
        assert(dispatcher != nullptr);
        assert(context != nullptr);

        auto *timerContext = static_cast<OperationContext*>(context);
        if (!timerContext->interrupted) {
            struct kevent event;
            EV_SET(&event, timer, EVFILT_TIMER, EV_DELETE, 0, 0, NULL);

            if (kevent(dispatcher->getKqueue(), &event, 1, nullptr, 0, nullptr) == -1) {
                throw std::runtime_error("Timer::stop, kevent failed, " + lastErrorMessage());
            }

            dispatcher->pushContext(timerContext->context);
            timerContext->interrupted = true;
        }
    };

    dispatcher->dispatch();
    dispatcher->getCurrentContext()->interruptProcedure = nullptr;
    assert(dispatcher != nullptr);
    assert(timerContext.context == dispatcher->getCurrentContext());
    assert(context == &timerContext);
    context = nullptr;
    timerContext.context = nullptr;
    dispatcher->pushTimer(timer);
    if (timerContext.interrupted) {
        throw InterruptedException();
    }
}

} // namespace System
