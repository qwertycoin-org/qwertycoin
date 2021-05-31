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

#include <iostream>
#include <ctime>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#endif

#ifdef __MACH__
#include <mach/clock.h>
#include <mach/mach.h>
#endif


namespace Common {
	inline uint64_t getNanoSeconds()
	{
		#if defined(_MSC_VER)
		return ::GetTickCount64() * 1000000;
		#elif defined(__MACH__)
		clock_serv_t sClock;
		mach_timespec_t sTimeSpec;

		host_get_clock_service(mach_host_self(), SYSTEM_CLOCK, &sClock);
		clock_get_time(sClock, &sTimeSpec);
		mach_port_deallocate(mach_task_self(), sClock);

		return ((uint64_t)sTimeSpec.tv_sec * 1000000000) + (sTimeSpec.tv_nsec);
		#else
		struct timespec sTimeSpec;
		if(clock_gettime(CLOCK_MONOTONIC, &sTimeSpec) != 0) {
			return 0;
		}

		return ((uint64_t)sTimeSpec.tv_sec * 1000000000) + (sTimeSpec.tv_nsec);
		#endif
	}

	inline uint64_t getTickCount()
	{
		return getNanoSeconds() / 1000000;
	}

#define TIME_MEASURE_START(uTime) uint64_t uTime = getTickCount();
#define TIME_MEASURE_PAUSE(uTime) uTime = getTickCount() - uTime;
#define TIME_MEASURE_RESTART(uTime) uTime = getTickCount() - uTime;
#define TIME_MEASURE_FINISH(uTime) uTime = getTickCount() - uTime;
} // namespace Common