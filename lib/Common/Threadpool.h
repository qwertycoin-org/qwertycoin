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

#include <cstddef>
#include <deque>
#include <functional>
#include <utility>
#include <stdexcept>
#include <vector>

#include <boost/thread/condition_variable.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>

namespace Tools {
    class QThreadpool
    {
    public:
        static QThreadpool &getInstance()
        {
            static QThreadpool sInstance;

            return sInstance;
        }

        /**
         * The waiter lets the caller know when all of its
         * tasks are completed.
         */
        class QWaiter
        {
            boost::mutex sMt;
            boost::condition_variable sCv;
            int iNum;
        public:
            void increase();

            void decrease();

            void wait();

            QWaiter() : iNum(0) {}

            ~QWaiter();
        };

        /**
         * Submit a task to the pool. The waiter pointer may be
         * NULL if the caller doesn't care to wait for the
         * task to finish.
         *
         * @param sWaiter
         * @param UFu
         */
        void submit(QWaiter *sWaiter, std::function<void()> UFu);

        int getMaxConcurrency();

    private:
        QThreadpool();

        ~QThreadpool();

        typedef struct FEntry
        {
            QWaiter *sWaiterObj;
            std::function<void()> UFu;
        } FEntry;

        std::deque<FEntry> sQueue;
        boost::condition_variable sHasWork;
        boost::mutex sMut;
        std::vector<boost::thread> sThreads;
        int iActive;
        int iMax;
        bool bRunning;

        void run();
    };
}