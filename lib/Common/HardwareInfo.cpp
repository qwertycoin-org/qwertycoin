//
// Created by exploshot on 12.10.20.
//

#ifndef _WIN32

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <sys/utsname.h>
#include <unistd.h>

#endif

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <thread>
#include <windows.h>

#endif

#if __APPLE__
#include <sys/param.h>
#include <sys/sysctl.h>
#endif

#include <vector>

#include <Common/HardwareInfo.h>

using namespace Tools::CPU;

#ifdef _WIN32

static std::vector<SYSTEM_LOGICAL_PROCESSOR_INFORMATION> cpuinfoBuffer()
{
	std::vector<SYSTEM_LOGICAL_PROCESSOR_INFORMATION> buffer;

	DWORD byte_count = 0;
	GetLogicalProcessorInformation(nullptr, &byte_count);
	buffer.resize(byte_count / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION));
	GetLogicalProcessorInformation(buffer.data(), &byte_count);

	return buffer;
}

#endif

std::string Tools::CPU::architecture() noexcept
{
	#ifdef _WIN32
	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);

	switch (sysInfo.wProcessorArchitecture)
	{
		case PROCESSOR_ARCHITECTURE_AMD64:
			return "x64";
		case PROCESSOR_ARCHITECTURE_ARM:
			return "ARM";
		case PROCESSOR_ARCHITECTURE_IA64:
			return "Itanium";
		case PROCESSOR_ARCHITECTURE_INTEL:
			return "x86";
		default:
			return "unknown";
	}
	#endif

	#ifndef _WIN32
	utsname buf;
	if (uname(&buf) == -1) {
		return "unknown";
	}

	if (!strcmp(buf.machine, "x86_64"))
		return "x64";
	else if (strstr(buf.machine, "arm") == buf.machine)
		return "ARM";
	else if (!strcmp(buf.machine, "ia64") || !strcmp(buf.machine, "IA64"))
		return "Itanium";
	else if (!strcmp(buf.machine, "i686"))
		return "x86";
	else
		return "unknown";
	#endif
}

Tools::CPU::Quantities Tools::CPU::quantities()
{
	Quantities ret{};

	#ifdef _WIN32
		SYSTEM_INFO sysinfo;
		GetSystemInfo(&sysinfo);
		ret.logical = sysinfo.dwNumberOfProcessors;
		ret.physical = ret.logical / 2;
	#elif __APPLE__
		int nm[2];
		size_t len = 4;
		uint32_t count;

		nm[0] = CTL_HW; nm[1] = HW_AVAILCPU;
		sysctl(nm, 2, &count, &len, NULL, 0);

		if(count < 1) {
		    nm[1] = HW_NCPU;
		    sysctl(nm, 2, &count, &len, NULL, 0);
		    if(count < 1) { count = 1; }
		}
		ret.logical = count;
		ret.physical = ret.logical / 2;
	#else
		ret.logical = sysconf(_SC_NPROCESSORS_ONLN);

		std::ifstream cpuinfo("/proc/cpuinfo");

		if (!cpuinfo.is_open() || !cpuinfo) {
			return ret;
		}

		std::vector<unsigned int> packageIds;
		for (std::string line; std::getline(cpuinfo, line);) {
			if (line.find("physical id") == 0) {
				const auto physicalId = std::strtoul(line.c_str() + line.find_first_of("1234567890"), nullptr, 10);
				if (std::find(packageIds.begin(), packageIds.end(), physicalId) == packageIds.end()) {
					packageIds.emplace_back(physicalId);
				}
			}
		}

		ret.packages = packageIds.size();
		ret.physical = ret.logical / ret.packages;
	#endif
	return ret;
}




