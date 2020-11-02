//
// Created by exploshot on 12.10.20.
//

#pragma once

#include <cstdint>
#include <string>
#include <vector>

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
}
