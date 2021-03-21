//
// Created by exploshot on 12.10.20.
//

#pragma once

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

#include <boost/filesystem.hpp>

#if (defined(__APPLE__))
#	define SYS_MEMINFO_MAC

#	include <mach/vm_statistics.h>
#	include <mach/mach_types.h>
#	include <mach/mach_init.h>
#	include <mach/mach_host.h>
#	include <mach/mach.h>

#elif (defined(_WIN32) || defined(WIN32) || defined(__CYGWIN__) || defined(__MINGW32__) || defined(_WIN64))
#   define SYS_MEMINFO_WIN

#   include <windows.h>
#   include <psapi.h>

#   undef min
#   undef max
#else
#	define SYS_MEMINFO_NIX

#	include <unistd.h>
#	include <sys/types.h>
#	include <sys/sysinfo.h>

#endif

#if defined _MSC_VER
#if defined HARDWARE_INFO_DLL
#if defined HARDWARE_INFO_BUILDING
#define HARDWARE_INFO_LINKAGE __declspec(dllexport)
#define HARDWARE_INFO_LINKAGE_INTERNAL
#else
#define HARDWARE_INFO_LINKAGE __declspec(dllimport)
#define HARDWARE_INFO_LINKAGE_INTERNAL
#endif
#else
#define HARDWARE_INFO_LINKAGE
#define HARDWARE_INFO_LINKAGE_INTERNAL
#endif // Building a DLL vs. Static Library
#else
#define HARDWARE_INFO_LINKAGE __attribute__((visibility("default")))
#define HARDWARE_INFO_LINKAGE_INTERNAL __attribute__((visibility("hidden")))
#endif

namespace Tools {
	namespace CPU {
		enum class Architecture
		{
			x64,
			arm,
			itanium,
			x86,
			unknown,
		};

		enum class ByteOrder
		{
			little,
			big,
		};

		enum class InstructionSet
		{
			s3d_now,
			s3d_now_extended,
			mmx,
			mmx_extended,
			sse,
			sse2,
			sse3,
			ssse3,
			sse4a,
			sse41,
			sse42,
			aes,

			avx,
			avx2,

			avx_512,
			avx_512_f,
			avx_512_cd,
			avx_512_pf,
			avx_512_er,
			avx_512_vl,
			avx_512_bw,
			avx_512_bq,
			avx_512_dq,
			avx_512_ifma,
			avx_512_vbmi,

			hle,

			bmi1,
			bmi2,
			adx,
			mpx,
			sha,
			prefetch_wt1,

			fma3,
			fma4,

			xop,

			rd_rand,

			x64,
			x87_fpu,
		};

		enum class CacheType
		{
			unified,
			instruction,
			data,
			trace,
		};

		struct Quantities
		{
			// Hyperthreads.
			uint32_t logical;
			// Physical "cores".
			uint32_t physical;
			// Physical CPU units/packages/sockets.
			uint32_t packages;
		};

		struct Cache
		{
			std::size_t size;
			std::size_t line_size;
			std::uint8_t associativity;
			CacheType type;
		};

		// Returns the quantity of CPU at various gradation.
		HARDWARE_INFO_LINKAGE Quantities quantities();

		// Get CPU cache properties
		HARDWARE_INFO_LINKAGE Cache cache(unsigned int level);

		// Returns the architecture of the current CPU.
		HARDWARE_INFO_LINKAGE std::string architecture() noexcept;

		// Returns the current frequency of the current CPU in Hz.
		HARDWARE_INFO_LINKAGE uint64_t frequency() noexcept;

		// Returns the current byte order of the current CPU.
		HARDWARE_INFO_LINKAGE ByteOrder byteOrder() noexcept;

		// Returns the CPU's vendor.
		HARDWARE_INFO_LINKAGE std::string vendor();

		// Returns the CPU's vendor according to the CPUID instruction
		HARDWARE_INFO_LINKAGE std::string vendorId();

		// Returns the CPU's model name.
		HARDWARE_INFO_LINKAGE std::string modelName();

		// Returns whether an instruction set is supported by the current CPU.
		// `noexcept` on Windows
		HARDWARE_INFO_LINKAGE bool instructionSetSupported(InstructionSet set);

		// Retrieve all of the instruction sets this hardware supports
		HARDWARE_INFO_LINKAGE std::vector<InstructionSet> supportedInstructionSets();
	}

	namespace Memory {
		class MemInfo
		{
		public:
			inline static double usedVirtMem();

			inline static double usedPhysMem();

			inline static double usedVirtMemMax();

			inline static double usedPhysMemMax();

			inline static double usedSysMem();

			inline static double freeSysMem();

			inline static double sysMem();

		private:
			inline static int parseLine(char *line)
			{
				int i = strlen(line);
				while (*line < '0' || *line > '9') line++;
				line[i - 3] = '\0';
				i = atoi(line);
				return i;
			}
		};

		double MemInfo::usedVirtMem()
		{
			#ifdef SYS_MEMINFO_NIX
			FILE *file = fopen("/proc/self/status", "r");
			int result = -1;
			char line[128];
			while (fgets(line, 128, file) != NULL) {
				if (strncmp(line, "VmSize:", 7) == 0) {
					result = parseLine(line);
					break;
				}
			}
			fclose(file);
			return static_cast<double>(result);
			#elif defined(SYS_MEMINFO_WIN)
			PROCESS_MEMORY_COUNTERS_EX pmc;
			GetProcessMemoryInfo(GetCurrentProcess(),(PROCESS_MEMORY_COUNTERS*) &pmc, sizeof(pmc));
			return static_cast<double>(pmc.WorkingSetSize)/1024.0;
			#elif defined(SYS_MEMINFO_MAC)
			struct mach_task_basic_info info;
			mach_msg_type_number_t infoCount = MACH_TASK_BASIC_INFO_COUNT;
			if ( task_info( mach_task_self( ), MACH_TASK_BASIC_INFO,(task_info_t)&info, &infoCount ) != KERN_SUCCESS ){
				return std::numeric_limits<double>::quiet_NaN();
			} else {
				return static_cast<double>(info.virtual_size)/1024.0;
			}
			#else
			return std::numeric_limits<double>::quiet_NaN();
			#endif
		}

		double MemInfo::usedPhysMem()
		{
			#ifdef SYS_MEMINFO_NIX
			FILE *file = fopen("/proc/self/status", "r");
			int result = -1;
			char line[128];
			while (fgets(line, 128, file) != NULL) {
				if (strncmp(line, "VmRSS:", 6) == 0) {
					result = parseLine(line);
					break;
				}
			}
			fclose(file);
			return static_cast<double>(result);
			#elif defined(SYS_MEMINFO_WIN)
			PROCESS_MEMORY_COUNTERS_EX pmc;
			GetProcessMemoryInfo(GetCurrentProcess(),(PROCESS_MEMORY_COUNTERS*) &pmc, sizeof(pmc));
			return  static_cast<double>(pmc.WorkingSetSize)/1024.0;
			#elif defined(SYS_MEMINFO_MAC)
			struct mach_task_basic_info info;
			mach_msg_type_number_t infoCount = MACH_TASK_BASIC_INFO_COUNT;
			if ( task_info( mach_task_self( ), MACH_TASK_BASIC_INFO,(task_info_t)&info, &infoCount ) != KERN_SUCCESS ){
				return std::numeric_limits<double>::quiet_NaN();
			} else {
				return static_cast<double>(info.resident_size)/1024.0;
			}
			#else
			return std::numeric_limits<double>::quiet_NaN();
			#endif
		}

		double MemInfo::usedVirtMemMax()
		{
			#ifdef SYS_MEMINFO_NIX
			FILE *file = fopen("/proc/self/status", "r");
			int result = -1;
			char line[128];
			while (fgets(line, 128, file) != NULL) {
				if (strncmp(line, "VmPeak:", 7) == 0) {
					result = parseLine(line);
					break;
				}
			}
			fclose(file);
			return static_cast<double>(result);
			#elif defined(SYS_MEMINFO_WIN)
			PROCESS_MEMORY_COUNTERS_EX pmc;
			GetProcessMemoryInfo(GetCurrentProcess(),(PROCESS_MEMORY_COUNTERS*) &pmc, sizeof(pmc));
			return  static_cast<double>(pmc.PeakPagefileUsage)/1024.0;
			#elif defined(SYS_MEMINFO_MAC)
			return std::numeric_limits<double>::quiet_NaN();
			#else
			return std::numeric_limits<double>::quiet_NaN();
			#endif
		}

		double MemInfo::usedPhysMemMax()
		{
			#ifdef SYS_MEMINFO_NIX
			FILE *file = fopen("/proc/self/status", "r");
			int result = -1;
			char line[128];
			while (fgets(line, 128, file) != NULL) {
				if (strncmp(line, "VmHWM:", 6) == 0) {
					result = parseLine(line);
					break;
				}
			}
			fclose(file);
			return static_cast<double>(result);
			#elif defined(SYS_MEMINFO_WIN)
			PROCESS_MEMORY_COUNTERS_EX pmc;
			GetProcessMemoryInfo(GetCurrentProcess(),(PROCESS_MEMORY_COUNTERS*) &pmc, sizeof(pmc));
			return  static_cast<double>(pmc.PeakWorkingSetSize)/1024.0;
			#elif defined(SYS_MEMINFO_MAC)
			struct rusage rusage;
			getrusage( RUSAGE_SELF, &rusage );
			return static_cast<double>(rusage.ru_maxrss)/1024;
			#else
			return std::numeric_limits<double>::quiet_NaN();
			#endif
		}

		double MemInfo::usedSysMem()
		{
			#ifdef SYS_MEMINFO_NIX
			struct sysinfo memInfo;
			sysinfo(&memInfo);
			return static_cast<double>(memInfo.totalram - memInfo.freeram) * memInfo.mem_unit / 1024.0;
			#elif defined(SYS_MEMINFO_WIN)
			MEMORYSTATUSEX memInfo;
		    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
			GlobalMemoryStatusEx(&memInfo);
			return  static_cast<double>(memInfo.ullTotalPhys-memInfo.ullAvailPhys)/1024.0;
			#elif defined(SYS_MEMINFO_MAC)
			return std::numeric_limits<double>::quiet_NaN();
			#else
			return std::numeric_limits<double>::quiet_NaN();
			#endif
		}

		double MemInfo::freeSysMem()
		{
			#ifdef SYS_MEMINFO_NIX
			struct sysinfo memInfo;
			sysinfo(&memInfo);
			return static_cast<double>(memInfo.freeram) * memInfo.mem_unit / 1024.0;
			#elif defined(SYS_MEMINFO_WIN)
			MEMORYSTATUSEX memInfo;
		    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
		    GlobalMemoryStatusEx(&memInfo);
		    return  static_cast<double>(memInfo.ullAvailPhys)/1024.0;
			#elif defined(SYS_MEMINFO_MAC)
		    return std::numeric_limits<double>::quiet_NaN();
			#else
		    return std::numeric_limits<double>::quiet_NaN();
			#endif
		}

		double MemInfo::sysMem()
		{
			#ifdef SYS_MEMINFO_NIX
			struct sysinfo memInfo;
			sysinfo(&memInfo);
			return static_cast<double>(memInfo.totalram) * memInfo.mem_unit / 1024.0;
			#elif defined(SYS_MEMINFO_WIN)
			MEMORYSTATUSEX memInfo;
			memInfo.dwLength = sizeof(MEMORYSTATUSEX);
			GlobalMemoryStatusEx(&memInfo);
			return  static_cast<double>(memInfo.ullTotalPhys)/1024.0;
			#elif defined(SYS_MEMINFO_MAC)
			return std::numeric_limits<double>::quiet_NaN();
			#else
			return std::numeric_limits<double>::quiet_NaN();
			#endif
		}
	}

	namespace Storage {
		class SpaceInfo
		{
		public:
			inline static uintmax_t freeSpace (boost::filesystem::path path);
			inline static uintmax_t availableSpace (boost::filesystem::path path);
			inline static uintmax_t capacitySpace (boost::filesystem::path path);
		};

		uintmax_t SpaceInfo::freeSpace(boost::filesystem::path path)
		{
			return boost::filesystem::space(path).free;
		}

		uintmax_t SpaceInfo::availableSpace(boost::filesystem::path path)
		{
			return boost::filesystem::space(path).available;
		}

		uintmax_t SpaceInfo::capacitySpace(boost::filesystem::path path)
		{
			return boost::filesystem::space(path).capacity;
		}
	}
}
