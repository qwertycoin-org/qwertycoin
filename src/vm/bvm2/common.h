// Copyright 2018 The Beam Team
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// Modified by the Qwertycoin developers, 2026.


#pragma once

#include <assert.h>
#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string.h>
#include <type_traits>
#include <utility>
#include <vector>

#ifndef BEAM_VERIFY
#	ifdef  NDEBUG
#		define BEAM_VERIFY(x) ((void)(x))
#	else
#		define BEAM_VERIFY(x) assert(x)
#	endif
#endif

#define IMPLEMENT_GET_PARENT_OBJ(parent_class, this_var) \
	parent_class& get_ParentObj() const { \
		parent_class*  p = (parent_class*) (((uint8_t*) this) + 1 - (uint8_t*) (&((parent_class*) 1)->this_var)); \
		assert(this == &p->this_var);  \
		return *p; \
	}

#ifndef _countof
#	define _countof(_Array) (sizeof(_Array) / sizeof(_Array[0]))
#endif

inline void memset0(void* p, size_t n) { memset(p, 0, n); }
bool memis0(const void* p, size_t n);

template <typename T>
inline void ZeroObject(T& x)
{
	static_assert(std::is_trivially_destructible_v<T>);
	memset0(&x, sizeof(x));
}

#define COMPARISON_VIA_CMP \
	template <typename T> bool operator < (const T& x) const { return cmp(x) < 0; } \
	template <typename T> bool operator > (const T& x) const { return cmp(x) > 0; } \
	template <typename T> bool operator <= (const T& x) const { return cmp(x) <= 0; } \
	template <typename T> bool operator >= (const T& x) const { return cmp(x) >= 0; } \
	template <typename T> bool operator == (const T& x) const { return cmp(x) == 0; } \
	template <typename T> bool operator != (const T& x) const { return cmp(x) != 0; }

namespace Cast
{
	template <typename T> inline T& NotConst(const T& x) { return (T&) x; }
	template <typename T> inline T* NotConst(const T* p) { return (T*) p; }

	template <typename TT, typename T> inline TT& Up(T& x)
	{
		TT& ret = (TT&) x;
		[[maybe_unused]] T& unused = ret;
		return ret;
	}

	template <typename TT, typename T> inline TT* Up(T* p)
	{
		TT* ret = (TT*) p;
		[[maybe_unused]] T* unused = ret;
		return ret;
	}

	template <typename TT, typename T> inline TT& Down(T& x)
	{
		return x;
	}

	template <typename TT, typename T> inline const TT& Down(const T& x)
	{
		return x;
	}

	template <typename TT, typename T> inline TT& Reinterpret(T& x)
	{
		static_assert(sizeof(TT) == sizeof(T));
		return (TT&)x;
	}

}

namespace beam
{
	typedef uint64_t Timestamp;
	typedef uint64_t Height;
	typedef uint64_t Amount;
	typedef std::vector<uint8_t> ByteBuffer;

	struct Blob
	{
		const void* p = nullptr;
		uint32_t n = 0;

		Blob() = default;
		Blob(const void* p_, uint32_t n_) :p(p_), n(n_) {}
		Blob(const ByteBuffer& bb);
		template <size_t nBytes_>
		Blob(const std::array<uint8_t, nBytes_>& x) : p(x.data()), n(static_cast<uint32_t>(x.size())) {}

		void Export(ByteBuffer&) const;

		int cmp(const Blob&) const;
		COMPARISON_VIA_CMP
	};

	template <typename T>
	struct TemporarySwap
	{
		T& m_var0;
		T& m_var1;

		TemporarySwap(T& v0, T& v1)
			:m_var0(v0)
			,m_var1(v1)
		{
			std::swap(m_var0, m_var1);
		}

		~TemporarySwap()
		{
			std::swap(m_var0, m_var1);
		}
	};

	struct Exc
		:public std::runtime_error
	{
		Exc(std::string&& s) :std::runtime_error(std::move(s)) {}
		uint32_t m_Type;

		static void Fail();
		static void Fail(const char*);

		static void Test(bool b)
		{
			if (!b)
				Fail();
		}

		class Checkpoint
		{
			static thread_local Checkpoint* s_pTop;
			Checkpoint* m_pNext;
		public:
			Checkpoint();
			~Checkpoint();
			virtual void Dump(std::ostream&) = 0;
			virtual uint32_t get_Type() { return 0; }

			static uint32_t DumpAll(std::ostream&);
		};

		class CheckpointTxt :public Checkpoint {
			const char* m_sz;
		public:
			CheckpointTxt(const char* sz) :m_sz(sz) {}
			void Dump(std::ostream&) override;
		};

	};
}

namespace std
{
	template <typename TDst, typename TSrc>
	inline void setmax(TDst& a, TSrc b) {
		if (a < b)
			a = b;
	}

	template <typename TDst, typename TSrc>
	inline void setmin(TDst& a, TSrc b) {
		if (a > b)
			a = b;
	}
}
