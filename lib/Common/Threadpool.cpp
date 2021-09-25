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

#include <cassert>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <system_error>

#include <boost/thread/thread.hpp>

#include <Common/Threadpool.h>

#if defined(_MSC_VER)
#define THREADV __declspec(thread)
#else
#define THREADV __thread
#endif

static THREADV int iDepth = 0;
static THREADV bool bIsLeaf = false;

namespace Tools {
    QThreadpool::QThreadpool() : bRunning(true), iActive(0)
    {
        boost::thread::attributes sAttrs;
        sAttrs.set_stack_size(0x400000);
        iMax = boost::thread::hardware_concurrency();
        size_t uIndex = iMax;
        while (uIndex--) {
            sThreads.push_back(boost::thread(sAttrs, boost::bind(&QThreadpool::run, this)));
        }
    }

    QThreadpool::~QThreadpool()
    {
        {
            const boost::unique_lock<boost::mutex> lock(sMut);
            bRunning = false;
            sHasWork.notify_all();
        }

        for (auto &sThread : sThreads) {
            sThread.join();
        }
    }

    void QThreadpool::submit(QWaiter *sWaiter, std::function<void()> UFu, bool bLLeaf)
    {
        FEntry sE = {sWaiter, UFu, bLLeaf};
        boost::unique_lock<boost::mutex> lock(sMut);

        if (!bLLeaf && (iActive == iMax) && !sQueue.empty() || iDepth > 0) {
            lock.unlock();
            ++iDepth;
            bIsLeaf = bLLeaf;
            UFu();
            --iDepth;
            bIsLeaf = false;
        } else {
            if (sWaiter) {
                sWaiter->increase();
            }

            if (bLLeaf) {
                sQueue.push_front(sE);
            } else {
                sQueue.push_back(sE);
            }

            sHasWork.notify_one();
        }
    }

    int QThreadpool::getMaxConcurrency()
    {
        return iMax;
    }

    QThreadpool::QWaiter::~QWaiter()
    {
        try {
            boost::unique_lock<boost::mutex> lock(sMt);
            if (iNum) {
                std::cout << "wait should have been called before waiter dtor - waiting now" <<
                          std::endl;
            }
        } catch (...) {

        }

        try {
            wait();
        } catch (std::exception &e) {
            // Todo: Add exception output
        }
    }

    bool QThreadpool::QWaiter::wait()
    {
        boost::unique_lock<boost::mutex> lock(sMt);
        while (iNum) {
            sCv.wait(lock);
        }

        return !error();
    }

    void QThreadpool::QWaiter::increase()
    {
        boost::unique_lock<boost::mutex> lock(sMt);
        iNum++;
    }

    void QThreadpool::QWaiter::decrease()
    {
        boost::unique_lock<boost::mutex> lock(sMt);
        iNum--;
        if (!iNum) {
            sCv.notify_all();
        }
    }

    void QThreadpool::run()
    {
        boost::unique_lock<boost::mutex> lock(sMut);
        while (bRunning) {
            FEntry sE;
            while (sQueue.empty() && bRunning) {
                sHasWork.wait(lock);
            }

            if (!bRunning) {
                break;
            }

            iActive++;
            sE = std::move(sQueue.front());
            sQueue.pop_front();
            lock.unlock();
            ++iDepth;
            bIsLeaf = sE.bLeaf;
            try {
                sE.UFu();
            } catch (std::exception &e) {
                sE.sWaiterObj->setError();
                try {
                    std::cout << "Exception in threadpool job: " << e.what() << std::endl;
                } catch (...) {}

            }
            --iDepth;
            bIsLeaf = false;

            if (sE.sWaiterObj) {
                sE.sWaiterObj->decrease();
            }

            lock.lock();
            iActive--;
        }
    }

}