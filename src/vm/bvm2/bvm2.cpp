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


#include "bvm2.h"
#include "../gas.h"
#include "../host_api.h"
#include <algorithm>
#include <sodium.h>

extern "C" {
#include "crypto/keccak.h"
}

namespace beam {
namespace bvm2 {

	namespace
	{
		void TaggedHash(crypto::hash& res, const char* szTag, std::initializer_list<Blob> parts)
		{
			ByteBuffer buf(szTag, szTag + strlen(szTag) + 1);
			for (const Blob& x : parts)
			{
				uint32_t n = ByteOrder::to_le(x.n);
				const auto* pN = reinterpret_cast<const uint8_t*>(&n);
				buf.insert(buf.end(), pN, pN + sizeof(n));
				const auto* p = reinterpret_cast<const uint8_t*>(x.p);
				buf.insert(buf.end(), p, p + x.n);
			}
			crypto::cn_fast_hash(buf.data(), buf.size(), res);
		}

		int32_t Sign(int x)
		{
			return (x > 0) - (x < 0);
		}
	}

	void get_ShaderID(ShaderID& sid, const Blob& data)
	{
		TaggedHash(sid, "qwc.vm.shader", { data });
	}

	void get_CidViaSid(ContractID& cid, const ShaderID& sid, const Blob& args)
	{
		TaggedHash(cid, "qwc.vm.cid", { ToBlob(sid), args });
	}

	void get_Cid(ContractID& cid, const Blob& data, const Blob& args)
	{
		ShaderID sid;
		get_ShaderID(sid, data);
		get_CidViaSid(cid, sid, args);
	}

#pragma pack (push, 1)
	struct Processor::Header
	{
		static const uint32_t s_Version = 2;

		uint32_t m_Version;
		uint32_t m_NumMethods;
		uint32_t m_hdrData0;
		uint32_t m_hdrTable0;

		static const uint32_t s_MethodsMin = 2;
		static const uint32_t s_MethodsMax = 1U << 28;

		uint32_t m_pMethod[s_MethodsMin];
	};
#pragma pack (pop)

	void Processor::InitBase(uint32_t nStackBytes)
	{
		ZeroObject(m_Code);
		ZeroObject(m_Data);
		ZeroObject(m_LinearMem);
		m_Instruction.m_p0 = m_Instruction.m_p1 = nullptr;

		m_vStack.assign((nStackBytes + sizeof(Wasm::Word) - 1) / sizeof(Wasm::Word), 0);

		m_Stack.m_pPtr = m_vStack.empty() ? nullptr : &m_vStack.front();
		m_Stack.m_BytesMax = nStackBytes;
		m_Stack.m_BytesCurrent = m_Stack.m_BytesMax;
		m_Stack.m_Pos = 0;
		m_Stack.m_PosMin = 0;

		m_Heap.Clear();

		m_DataProcessor.m_Map.Clear();
	}

	void ProcessorContract::InitStackPlus(uint32_t nStackBytes)
	{
		InitBase(vm::limits::stack_size + nStackBytes);
	}

	const Processor::Header& Processor::ParseMod()
	{
		Exc::Test(m_Code.n >= sizeof(Header));
		const Header& hdr = *reinterpret_cast<const Header*>(m_Code.p);

		Exc::Test(ByteOrder::from_le(hdr.m_Version) == hdr.s_Version);
		uint32_t nMethods = ByteOrder::from_le(hdr.m_NumMethods);
		Exc::Test((nMethods - Header::s_MethodsMin <= Header::s_MethodsMax - Header::s_MethodsMin));

		uint32_t nHdrSize = sizeof(Header) + sizeof(Wasm::Word) * (nMethods - Header::s_MethodsMin);
		Exc::Test(nHdrSize <= m_Code.n);

		m_Data.n = m_Code.n - nHdrSize;
		m_Data.p = reinterpret_cast<const uint8_t*>(m_Code.p) + nHdrSize;
		m_prData0 = ByteOrder::from_le(hdr.m_hdrData0);

		m_prTable0 = ByteOrder::from_le(hdr.m_hdrTable0);
		Exc::Test(m_prTable0 <= m_Code.n);

		return hdr;
	}

	void ProcessorContract::CallFar(const ContractID& cid, uint32_t iMethod, Wasm::Word pArgs, uint32_t nArgs, uint32_t nFlags)
	{
		Exc::CheckpointTxt cp("CallFar");

		Exc::Test(m_FarCalls.m_Stack.size() < vm::limits::far_call_depth);

		auto nRetAddr = get_Ip();
		const ContractID* pCidContext = &cid;

		uint32_t nFlagsDst = 0;

		if (CallFarFlags::GlobalLockRO & nFlags)
			nFlagsDst |= FarCalls::Flags::s_GlobalRO;

		if (!m_FarCalls.m_Stack.empty())
		{
			if (CallFarFlags::SelfBlock & nFlags)
				nFlagsDst |= FarCalls::Flags::s_CallerBlocked;
			if (CallFarFlags::SelfLockRO & nFlags)
				nFlagsDst |= FarCalls::Flags::s_CallerLockedRO;

			auto it = m_FarCalls.m_Stack.rbegin();

			{
				FarCalls::Frame& f = *it;

				if (CallFarFlags::InheritContext & nFlags)
					pCidContext = &f.m_Cid;

				if (FarCalls::Flags::s_GlobalRO & f.m_Flags)
					nFlagsDst |= FarCalls::Flags::s_GlobalRO;
			}

			for (uint32_t nFlagsSrc = nFlagsDst; ; )
			{
				FarCalls::Frame& f = *it;

				if (f.m_Cid == *pCidContext)
				{
					if (FarCalls::Flags::s_CallerBlocked & nFlagsSrc)
						Exc::Fail("reentry blocked");

					if (FarCalls::Flags::s_CallerLockedRO & nFlagsSrc)
						nFlagsDst |= FarCalls::Flags::s_LockedRO;

					if (FarCalls::Flags::s_LockedRO & f.m_Flags)
						nFlagsDst |= FarCalls::Flags::s_LockedRO;

					break;
				}

				if (m_FarCalls.m_Stack.rend() == ++it)
					break;

				nFlagsSrc = f.m_Flags;
			}
		}

		auto& x = *m_FarCalls.m_Stack.Create_back();
		x.m_Cid = *pCidContext;
		x.m_Method = iMethod;
		x.m_FarRetAddr = nRetAddr;
		x.m_StackBytesMax = m_Stack.m_BytesMax;
		x.m_StackBytesRet = m_Stack.m_BytesCurrent;
		x.m_Flags = nFlagsDst;
		ZeroObject(x.m_Args);

		x.m_StackPosMin = m_Stack.m_PosMin;
		m_Stack.m_PosMin = m_Stack.m_Pos;

		get_Storage().LoadVar(ToBlob(cid), m_Code);
		m_Code.Export(x.m_Body);
		m_Code = x.m_Body;

		const Header& hdr = ParseMod();
		Exc::Test(iMethod < ByteOrder::from_le(hdr.m_NumMethods));

		m_Stack.Push(pArgs);
		m_Stack.Push(0);

		uint32_t nAddr = ByteOrder::from_le(hdr.m_pMethod[iMethod]);
		OnCall(nAddr);
	}

	void ProcessorContract::OnRet(Wasm::Word nRetAddr)
	{
		if (nRetAddr)
			Processor::OnRet(nRetAddr);
		else
			OnRetFar();
	}

	void ProcessorContract::OnRetFar()
	{
		Exc::Test(m_Stack.m_Pos == m_Stack.m_PosMin);

		auto& x = m_FarCalls.m_Stack.back();
		Exc::Test(m_Stack.m_BytesCurrent == x.m_StackBytesRet);

		Wasm::Word nRetAddr = x.m_FarRetAddr;

		m_Stack.m_PosMin = x.m_StackPosMin;
		m_Stack.m_BytesMax = x.m_StackBytesMax;

		if (FarCalls::Flags::s_HeapSwap & x.m_Flags)
		{
			m_Heap.swap(x.m_Heap);
			m_LinearMem = m_Heap.m_vMem;
		}

		if (x.m_Args.n)
		{
			memcpy(Cast::NotConst(x.m_Args.p), m_Stack.get_AliasPtr(), x.m_Args.n);
			m_Stack.AliasFree(x.m_Args.n);
		}

		m_FarCalls.m_Stack.Delete(x);

		if (!m_FarCalls.m_Stack.empty())
		{
			m_Code = m_FarCalls.m_Stack.back().m_Body;
			ParseMod();

			Processor::OnRet(nRetAddr);
		}
	}

	uint32_t ProcessorContract::get_HeapLimit()
	{
		return vm::limits::heap_size;
	}

	void Processor::VarKey::Set(const ContractID& cid)
	{
		memcpy(m_p, &cid, sizeof(cid));
		m_Size = sizeof(cid);
	}

	void Processor::VarKey::Append(uint8_t nTag, const Blob& blob)
	{
		m_p[m_Size++] = nTag;

		assert(m_Size + blob.n <= _countof(m_p));
		memcpy(m_p + m_Size, blob.p, blob.n);
		m_Size += blob.n;
	}

	void ProcessorContract::SetVarKey(VarKey& vk)
	{
		vk.Set(m_FarCalls.m_Stack.back().m_Cid);
	}

	void ProcessorContract::SetVarKey(VarKey& vk, uint8_t nTag, const Blob& blob)
	{
		SetVarKey(vk);
		vk.Append(nTag, blob);
	}


	void Processor::Compile(ByteBuffer& res, const Blob& src)
	{
		Compiler c;
		c.Compile(res, src);
	}

	void Processor::CompileApp(ByteBuffer& res, const Blob& src)
	{
		Compiler c(Compiler::Kind::App);
		c.Compile(res, src);
	}

	App::IHost& ProcessorContract::RequireAppHost()
	{
		App::IHost* pHost = get_AppHost();
		Exc::Test(pHost);
		return *pHost;
	}

#define STR_MATCH(vec, txt) ((vec.n == sizeof(txt)-1) && !memcmp(vec.p, txt, sizeof(txt)-1))

	int32_t Processor::Compiler::get_PublicMethodIdx(const Wasm::Compiler::Vec<char>& sName)
	{
		if (STR_MATCH(sName, "Ctor"))
			return 0;
		if (STR_MATCH(sName, "Dtor"))
			return 1;

		static const char szPrefix[] = "Method_";
		const uint32_t nPrefix = sizeof(szPrefix) - 1;
		if (sName.n <= nPrefix || sName.n > nPrefix + 9)
			return -1;

		if (memcmp(sName.p, szPrefix, nPrefix))
			return -1;

		int32_t res = 0;
		for (uint32_t i = nPrefix; i < sName.n; i++)
		{
			char c = sName.p[i];
			if (c < '0' || c > '9')
				return -1;
			res = res * 10 + (c - '0');
		}

		return res;
	}

	void Processor::DischargeMemOp(uint32_t size)
	{
		DischargeUnits(vm::gas::mem_op(size));
	}

	void ProcessorContract::DischargeUnits(uint64_t n)
	{
		if (m_Charge < n)
		{
			struct MyCheckpoint :public Exc::Checkpoint
			{
				void Dump(std::ostream& os) override {
					os << "Discharge";
				}

				uint32_t get_Type() override {
					return ErrorSubType::NoCharge;
				}

			} cp;

			Exc::Fail("no Charge");
		}

		m_Charge -= n;
	}

	void Processor::Compiler::Compile(ByteBuffer& res, const Blob& src)
	{
		Exc::CheckpointTxt cp("Wasm/compile");

		Exc::Test(src.n <= vm::limits::module_size);

		Wasm::Reader inp;
		inp.m_p0 = reinterpret_cast<const uint8_t*>(src.p);
		inp.m_p1 = inp.m_p0 + src.n;

		Wasm::Compiler c;
		c.Parse(inp);

		ResolveBindings(c);

		typedef std::map<uint32_t, uint32_t> MethodMap;
		MethodMap hdrMap;

		for (uint32_t i = 0; i < c.m_Exports.size(); i++)
		{
			auto& x = c.m_Exports[i];
			if (x.m_Kind)
				continue;

			int32_t iMethod = get_PublicMethodIdx(x.m_sName);
			if (iMethod < 0)
				continue;

			Exc::Test(hdrMap.end() == hdrMap.find(iMethod));
			hdrMap[iMethod] = x.m_Idx;
		}

		uint32_t nNumMethods = static_cast<uint32_t>(hdrMap.size());

		Exc::Test(nNumMethods >= Header::s_MethodsMin);
		Exc::Test(hdrMap.rbegin()->first == nNumMethods - 1);

		for (auto it = hdrMap.begin(); hdrMap.end() != it; it++)
		{
			uint32_t iFunc = it->second;
			Exc::Test(iFunc < c.m_Functions.size());
			c.m_Functions[iFunc].m_Dep.m_Include = true;
		}

		uint32_t nSizeHdr = sizeof(Header) + sizeof(Wasm::Word) * (nNumMethods - Header::s_MethodsMin);
		c.m_Result.resize(nSizeHdr);
		auto* pHdr = reinterpret_cast<Header*>(&c.m_Result.front());

		pHdr->m_Version = ByteOrder::to_le(Header::s_Version);
		pHdr->m_NumMethods = ByteOrder::to_le(nNumMethods);


		c.Build();

		pHdr = reinterpret_cast<Header*>(&c.m_Result.front());
		pHdr->m_hdrTable0 = ByteOrder::to_le(c.m_cmplTable0);

		for (auto it = hdrMap.begin(); hdrMap.end() != it; it++)
		{
			uint32_t iMethod = it->first;
			uint32_t iFunc = it->second;
			pHdr->m_pMethod[iMethod] = ByteOrder::to_le(c.m_Labels.m_Items[iFunc]);
		}

		if (c.m_Data.empty())
			pHdr->m_hdrData0 = 0;
		else
		{
			uint32_t nPos = static_cast<uint32_t>(c.m_Result.size());

			Wasm::Word val = c.m_cmplData0 + nSizeHdr - nPos;
			pHdr->m_hdrData0 = ByteOrder::to_le(val);

			c.m_Result.resize(nPos + c.m_Data.size());
			memcpy(&c.m_Result.front() + nPos, &c.m_Data.front(), c.m_Data.size());
		}

		Exc::Test(c.m_Result.size() <= vm::limits::compiled_size);

		res = std::move(c.m_Result);
	}



	template <typename T>
	struct ParamWrap {
		typedef T Type;
		Type V;
		Wasm::Processor& operator = (Wasm::Processor& p) {
			V = p.m_Stack.Pop<Type>();
			return p;
		}

		static_assert(std::numeric_limits<T>::is_integer);
		static_assert(sizeof(T) <= sizeof(Wasm::Word) * 2);

		static const uint8_t s_Code = sizeof(T) <= sizeof(Wasm::Word) ?
			Wasm::TypeCode::i32 :
			Wasm::TypeCode::i64;

		static void TestRetType(const Wasm::Compiler::Vec<uint8_t>& v) {
			Exc::Test((1 == v.n) && (s_Code == *v.p));
		}
	};

	template <typename T>
	struct ParamWrap<T*>
		:public ParamWrap<Wasm::Word>
	{
		Wasm::Processor& operator = (Wasm::Processor& p) {
			return ParamWrap<Wasm::Word>::operator = (p);
		}
	};

	template <typename T>
	struct ParamWrap<T&>
		:public ParamWrap<Wasm::Word>
	{
		Wasm::Processor& operator = (Wasm::Processor& p) {
			return ParamWrap<Wasm::Word>::operator = (p);
		}
	};

	template <>
	struct ParamWrap<void>
	{
		typedef void Type;

		static void TestRetType(const Wasm::Compiler::Vec<uint8_t>& v) {
			Exc::Test(!v.n);
		}
	};

	template <typename TRes> struct Caller {
		template <typename TArgs, typename TProcessor>
		static void Call(TProcessor& me, const TArgs& args) {
			auto ret = args.Call(me);
			me.m_Stack.template Push<TRes>(ret);
		}
	};
	template <> struct Caller<void> {
		template <typename TArgs, typename TProcessor>
		static void Call(TProcessor& me, const TArgs& args) {
			args.Call(me);
		}
	};

	using Shaders::KeyTag;

#define MACRO_NOP
#define MACRO_COMMA ,
#define PAR_DECL(type, name) ParamWrap<type>::Type name
#define THE_MACRO(id, ret, name) \
		typedef ParamWrap<ret>::Type RetType_##name; \
		RetType_##name OnMethod_##name(BVMOp_##name(PAR_DECL, MACRO_COMMA));

	struct ProcessorPlus
		:public ProcessorContract
	{
		typedef ProcessorPlus TProcessor;
		static TProcessor& From(ProcessorContract& p)
		{
			static_assert(sizeof(TProcessor) == sizeof(p));
			return Cast::Up<TProcessor>(p);
		}

		void InvokeExtPlus(uint32_t nBinding);

		BVMOpsAll_Common(THE_MACRO)
		BVMOpsAll_Contract(THE_MACRO)
		BVMOpsAll_App(THE_MACRO)
	};

#undef THE_MACRO
#undef PAR_DECL

#define PAR_PASS(type, name) m_##name.V
#define PAR_DECL(type, name) ParamWrap<type> m_##name;
#define PAR_ASSIGN(type, name) args.m_##name =
#define PAR_DUMP(type, name) << "," #name "=" << static_cast<int64_t>(m_##name.V)

#define THE_MACRO(id, ret, name) \
		case id: { \
			DischargeUnits(vm::gas::host_call_cost(static_cast<vm::host_call>(id))); \
			struct Args :public Exc::Checkpoint { \
				BVMOp_##name(PAR_DECL, MACRO_NOP) \
				RetType_##name Call(TProcessor& me) const { return me.OnMethod_##name(BVMOp_##name(PAR_PASS, MACRO_COMMA)); } \
				void Dump(std::ostream& os) override { \
					os << #name BVMOp_##name(PAR_DUMP, MACRO_NOP); \
				} \
			} args; \
			BVMOp_##name(PAR_ASSIGN, MACRO_NOP) *this; \
			Caller<RetType_##name>::Call(*this, args); \
		} break;

	void ProcessorPlus::InvokeExtPlus(uint32_t nBinding)
	{
		switch (nBinding)
		{
		BVMOpsAll_Common(THE_MACRO)
		BVMOpsAll_Contract(THE_MACRO)
		BVMOpsAll_App(THE_MACRO)
		default:
			Wasm::Processor::InvokeExt(nBinding);
		}
	}

#undef THE_MACRO
#undef PAR_PASS
#undef PAR_DECL
#undef PAR_ASSIGN
#undef PAR_DUMP

	void ProcessorContract::InvokeExt(uint32_t nBinding)
	{
		if (nBinding == 0x46 && !m_HdrInfoActive)
			Wasm::Processor::InvokeExt(nBinding);
		ProcessorPlus::From(*this).InvokeExtPlus(nBinding);
	}

	void TestStackPtr(const Wasm::Compiler::GlobalVar& x)
	{
		Exc::Test(x.CanBeStackPtr());
	}

	bool Processor::Compiler::ResolveBinding(Wasm::Compiler& c, Wasm::Compiler::PerImportFunc& x)
	{
		if (!STR_MATCH(x.m_sMod, "env"))
			return false;

		Exc::Test(x.m_TypeIdx < c.m_Types.size());
		const auto& tp = c.m_Types[x.m_TypeIdx];

#define PAR_TYPECODE(type, name) ParamWrap<type>::s_Code,
#define THE_MACRO(id, ret, name) \
		if (STR_MATCH(x.m_sName, #name)) { \
			x.m_Binding = id; \
			const uint8_t pSig[] = { BVMOp_##name(PAR_TYPECODE, MACRO_NOP) 0 }; \
			Exc::Test(tp.m_Args.n == _countof(pSig) - 1); \
			Exc::Test(!memcmp(tp.m_Args.p, pSig, sizeof(pSig) - 1)); \
			ParamWrap<ret>::TestRetType(tp.m_Rets); \
			return true; \
		}

		BVMOpsAll_Common(THE_MACRO)

		if (Kind::App == m_Kind)
		{
			BVMOpsAll_App(THE_MACRO)
		}
		else
		{
			BVMOpsAll_Contract(THE_MACRO)
		}

#undef THE_MACRO
#undef PAR_TYPECODE

		return false;
	}

	void Processor::Compiler::ResolveBindings(Wasm::Compiler& c)
	{
		Exc::CheckpointTxt cp("Bindings");

		for (uint32_t i = 0; i < c.m_ImportFuncs.size(); i++)
			Exc::Test(ResolveBinding(c, c.m_ImportFuncs[i]));

		bool bStackPtrImported = false;
		for (uint32_t i = 0; i < c.m_ImportGlobals.size(); i++)
		{
			auto& x = c.m_ImportGlobals[i];

			if (STR_MATCH(x.m_sMod, "env"))
			{
				if (STR_MATCH(x.m_sName, "__stack_pointer"))
				{
					TestStackPtr(x);
					x.m_Binding = static_cast<uint32_t>(Wasm::VariableType::StackPointer);
					bStackPtrImported = true;
				}
			}

		}

		if (!c.m_Globals.empty())
		{
			Exc::Test(!bStackPtrImported && (1 <= c.m_Globals.size()));

			for (auto& g : c.m_Globals)
			{
				auto& x = c.m_ImportGlobals.emplace_back();
				ZeroObject(x);
				if (!bStackPtrImported && g.CanBeStackPtr())
				{
					x.m_Binding = static_cast<uint32_t>(Wasm::VariableType::StackPointer);
					bStackPtrImported = true;
				}
				else
				{
					x.m_Binding = static_cast<uint32_t>(-1);
				}

				Cast::Down<Wasm::Compiler::GlobalVar>(x) = g;
			}
			c.m_Globals.clear();
		}

	}


#define BVM_METHOD_PAR_DECL(type, name) ParamWrap<type>::Type name
#define BVM_METHOD(name) ProcessorPlus::RetType_##name ProcessorPlus::OnMethod_##name(BVMOp_##name(BVM_METHOD_PAR_DECL, MACRO_COMMA))

	BVM_METHOD(Memcpy)
	{
		DischargeMemOp(size);
		memmove(get_AddrW(pDst, size), get_AddrR(pSrc, size), size);
		return pDst;
	}

	BVM_METHOD(Memset)
	{
		DischargeMemOp(size);
		memset(get_AddrW(pDst, size), val, size);
		return pDst;
	}

	BVM_METHOD(Memcmp)
	{
		DischargeMemOp(size);
		return Sign(memcmp(get_AddrR(p1, size), get_AddrR(p2, size), size));
	}

	BVM_METHOD(Memis0)
	{
		DischargeMemOp(size);
		return !!memis0(get_AddrR(p, size), size);
	}

	const char* Processor::RealizeStr(Wasm::Word sz, uint32_t& nLenOut)
	{
		Exc::CheckpointTxt cp("string/realize");

		uint32_t n;
		auto sz_ = reinterpret_cast<const char*>(get_AddrExVar(sz, n, false));

		auto* p = reinterpret_cast<const char*>(memchr(sz_, 0, n));
		Exc::Test(p);
		nLenOut = static_cast<uint32_t>(p - sz_);

		DischargeMemOp(nLenOut + 1);

		return sz_;
	}

	BVM_METHOD(Strlen)
	{
		uint32_t n;
		RealizeStr(sz, n);
		return n;
	}

	BVM_METHOD(Strcmp)
	{
		uint32_t n1, n2;
		const char* s1 = RealizeStr(sz1, n1);
		const char* s2 = RealizeStr(sz2, n2);
		return Sign(strcmp(s1, s2));
	}

	BVM_METHOD(StackAlloc)
	{
		m_Stack.AliasAlloc(size);
		return m_Stack.get_AlasSp();
	}

	BVM_METHOD(StackFree)
	{
		m_Stack.AliasFree(size);
	}

	bool Processor::HeapAllocEx(uint32_t& res, uint32_t size)
	{
		DischargeUnits(vm::gas::heap_op);
		DischargeMemOp(size);

		if (m_Heap.Alloc(res, size))
			return true;

		uint32_t nSizeOld = m_LinearMem.n;
		uint32_t nReserve = m_Heap.get_UnusedAtEnd(nSizeOld);
		assert(nReserve < size);
		uint32_t nSizeNew = nSizeOld + size - nReserve;

		uint32_t nHeapMax = get_HeapLimit();
		if ((nHeapMax < nSizeNew) || (nSizeNew < nSizeOld))
			return false;

		std::setmax(nSizeNew, 0x1000U);
		std::setmax(nSizeNew, static_cast<uint32_t>(m_Heap.m_vMem.size()) * 2);
		std::setmin(nSizeNew, nHeapMax);
		if (nSizeNew <= nSizeOld)
			return false;

		HeapReserveStrict(nSizeNew);

		return m_Heap.Alloc(res, size);
	}

	void Processor::HeapReserveStrict(uint32_t n)
	{
		uint32_t nOld = m_LinearMem.n;
		assert(nOld < n);

		m_Heap.m_vMem.resize(n, 0);
		m_LinearMem = m_Heap.m_vMem;

		m_Heap.OnGrow(nOld, n);
	}

	void Processor::HeapFreeEx(uint32_t res)
	{
		DischargeUnits(vm::gas::heap_op);
		m_Heap.Free(res);
	}

	BVM_METHOD(Heap_Alloc)
	{
		size = Stack::AlignUp(size);

		uint32_t val;
		if (!HeapAllocEx(val, size))
			return 0;

		return Wasm::MemoryType::Global | val;
	}

	BVM_METHOD(Heap_Free)
	{
		HeapFreeEx(pPtr ^ Wasm::MemoryType::Global);
	}

	bool Processor::Heap::Alloc(uint32_t& retVal, uint32_t size)
	{
		auto it = m_mapSize.lower_bound(size, Entry::Size::Comparator());
		if (m_mapSize.end() == it)
			return false;

		auto& e = it->get_ParentObj();
		assert(e.m_Size.m_Key >= size);
		retVal = e.m_Pos.m_Key;

		if (e.m_Size.m_Key == size)
		{
			Remove(e, true);
			Insert(e, false);
		}
		else
		{
			Create(e.m_Pos.m_Key, size, false);

			e.m_Pos.m_Key += size;
			UpdateSizeFree(e, e.m_Size.m_Key - size);
		}

		return true;
	}

	Processor::Heap::Entry* Processor::Heap::Create(uint32_t nPos, uint32_t nSize, bool bFree)
	{
		auto* p = new Heap::Entry;
		p->m_Pos.m_Key = nPos;
		p->m_Size.m_Key = nSize;
		Insert(*p, bFree);
		return p;
	}

	void Processor::Heap::Free(uint32_t ptr)
	{
		auto it = m_mapAllocated.find(ptr, Entry::Pos::Comparator());
		Exc::Test(m_mapAllocated.end() != it);

		auto& e = it->get_ParentObj();
		Remove(e, false);
		Insert(e, true);

		TryMerge(e);

		it = MapPos::s_iterator_to(e.m_Pos);
		if (m_mapFree.begin() != it)
		{
			--it;
			TryMerge(it->get_ParentObj());
		}
	}

	void Processor::Heap::UpdateSizeFree(Entry& e, uint32_t newVal)
	{
		m_mapSize.erase(MapSize::s_iterator_to(e.m_Size));
		e.m_Size.m_Key = newVal;
		m_mapSize.insert(e.m_Size);
	}

	void Processor::Heap::TryMerge(Entry& e)
	{
		auto it = m_mapFree.find(e.m_Pos.m_Key + e.m_Size.m_Key, Entry::Pos::Comparator());
		if (m_mapFree.end() != it)
		{
			auto& e2 = it->get_ParentObj();
			UpdateSizeFree(e, e.m_Size.m_Key + e2.m_Size.m_Key);
			Delete(e2, true);
		}
	}

	void Processor::Heap::Insert(Entry& e, bool bFree)
	{
		if (bFree)
		{
			m_mapFree.insert(e.m_Pos);
			m_mapSize.insert(e.m_Size);
		}
		else
			m_mapAllocated.insert(e.m_Pos);
	}

	void Processor::Heap::Remove(Entry& e, bool bFree)
	{
		if (bFree)
		{
			m_mapFree.erase(MapPos::s_iterator_to(e.m_Pos));
			m_mapSize.erase(MapSize::s_iterator_to(e.m_Size));
		}
		else
			m_mapAllocated.erase(MapPos::s_iterator_to(e.m_Pos));
	}

	void Processor::Heap::Delete(Entry& e, bool bFree)
	{
		Remove(e, bFree);
		delete &e;
	}

	void Processor::Heap::Clear()
	{
		while (!m_mapFree.empty())
			Delete(m_mapFree.begin()->get_ParentObj(), true);

		while (!m_mapAllocated.empty())
			Delete(m_mapAllocated.begin()->get_ParentObj(), false);

		m_vMem.clear();
	}

	uint32_t Processor::Heap::get_UnusedAtEnd(uint32_t nEnd) const
	{
		if (!m_mapFree.empty())
		{
			auto& e = m_mapFree.rbegin()->get_ParentObj();
			if (e.m_Pos.m_Key + e.m_Size.m_Key == nEnd)
				return e.m_Size.m_Key;
		}
		return 0;
	}

	void Processor::Heap::OnGrow(uint32_t nOld, uint32_t nNew)
	{
		assert(nOld < nNew);

		if (!m_mapFree.empty())
		{
			auto& e = m_mapFree.rbegin()->get_ParentObj();
			assert(e.m_Pos.m_Key + e.m_Size.m_Key <= nOld);

			if (e.m_Pos.m_Key + e.m_Size.m_Key == nOld)
			{
				UpdateSizeFree(e, nNew - e.m_Pos.m_Key);
				return;
			}
		}

		Create(nOld, nNew - nOld, true);
	}

	void Processor::Heap::swap(Heap& x)
	{
		m_mapAllocated.swap(x.m_mapAllocated);
		m_mapFree.swap(x.m_mapFree);
		m_mapSize.swap(x.m_mapSize);
		m_vMem.swap(x.m_vMem);
	}

	void ProcessorContract::SetVarKeyFromShader(VarKey& vk, uint8_t nTag, const Blob& key, bool bW)
	{
		Exc::Test(key.n <= vm::limits::var_key);
		SetVarKey(vk, nTag, key);

		if (bW)
		{
			switch (nTag)
			{
			case VarKey::Tag::Internal:
			case VarKey::Tag::InternalStealth:
				break;

			default:
				Exc::Fail();
			}
		}
	}

	BVM_METHOD(LoadVar)
	{
		VarKey vk;
		SetVarKeyFromShader(vk, nType, Blob(get_AddrR(pKey, nKey), nKey), false);

		uint8_t* pDst = get_AddrW(pVal, nVal);

		Blob res;
		get_Storage().LoadVar(vk.ToBlob(), res);

		uint32_t n = std::min(nVal, res.n);
		DischargeUnits(vm::gas::load_var_for(res.n));

		memcpy(pDst, res.p, n);
		return res.n;
	}

	BVM_METHOD(SaveVar)
	{
		DischargeUnits(vm::gas::save_var_for(nVal));

		TestVarSize(nVal);
		TestCanWrite();

		VarKey vk;
		SetVarKeyFromShader(vk, nType, Blob(get_AddrR(pKey, nKey), nKey), true);

		return get_Storage().SaveVar(vk.ToBlob(), Blob(get_AddrR(pVal, nVal), nVal));
	}

	BVM_METHOD(EmitLog)
	{
		DischargeUnits(vm::gas::log_for(nVal));

		TestVarSize(nVal);

		VarKey vk;
		SetVarKeyFromShader(vk, nType, Blob(get_AddrR(pKey, nKey), nKey), true);

		return get_Storage().OnLog(vk.ToBlob(), Blob(get_AddrR(pVal, nVal), nVal));
	}

	BVM_METHOD(CallFar)
	{

		auto nCalleeStackMax = m_Stack.m_BytesCurrent;
		uint8_t* pArgsPtr = nullptr;

		if (!(CallFarFlags::InheritContext & nFlags))
		{
			Exc::Test(iMethod >= 2);

			if (nArgs)
			{
				bool bIsStackPtr = ((Wasm::MemoryType::Mask & pArgs) == Wasm::MemoryType::Stack);
				if (bIsStackPtr)
				{
					uint32_t nSize;
					get_AddrExVar(pArgs, nSize, false);
					std::setmin(nArgs, nSize);

					Exc::Test(pArgs >= m_Stack.get_AlasSp());
				}

				if (pArgs != m_Stack.get_AlasSp())
				{
					pArgsPtr = get_AddrW(pArgs, nArgs);

					m_Stack.PushAlias(Blob(pArgsPtr, nArgs));
					pArgs = m_Stack.get_AlasSp();
				}

				if (!pArgsPtr)
					nCalleeStackMax = (pArgs & ~Wasm::MemoryType::Stack) + nArgs;
			}
			else
				pArgs = m_Stack.get_AlasSp();
		}

		auto nDepth = m_FarCalls.m_Stack.size();
		ProcessorContract::CallFar(get_AddrAsR<ContractID>(cid), iMethod, pArgs, nArgs, nFlags);

		if (!(CallFarFlags::InheritContext & nFlags) && (m_FarCalls.m_Stack.size() > nDepth))
		{
			auto& x = m_FarCalls.m_Stack.back();
			x.m_Flags |= FarCalls::Flags::s_HeapSwap;
			m_Heap.swap(x.m_Heap);
			ZeroObject(m_LinearMem);

			if (pArgsPtr)
			{
				x.m_Args.p = pArgsPtr;
				x.m_Args.n = nArgs;
			}

			m_Stack.m_BytesMax = nCalleeStackMax;
		}
	}

	BVM_METHOD(get_CallDepth)
	{
		return static_cast<uint32_t>(m_FarCalls.m_Stack.size());
	}

	BVM_METHOD(get_CallerCid)
	{
		auto& res = get_AddrAsW<ContractID>(cid);
		for (auto it = m_FarCalls.m_Stack.rbegin(); ; it++)
		{
			Exc::Test(m_FarCalls.m_Stack.rend() != it);
			if (!iCaller--)
			{
				res = it->m_Cid;
				break;
			}
		}
	}

	BVM_METHOD(Halt)
	{
		struct MyCheckpoint :public Exc::Checkpoint
		{
			void Dump(std::ostream& os) override {
				os << "Explicit Halt";
			}

			uint32_t get_Type() override {
				return ErrorSubType::Internal;
			}

		} cp;

		Exc::Fail();
	}

	BVM_METHOD(FundsLock)
	{
		HandleAmount(amount, true);
	}

	BVM_METHOD(FundsUnlock)
	{
		HandleAmount(amount, false);
	}

	BVM_METHOD(AddSig)
	{
		const PubKey& key = get_AddrAsR<PubKey>(pubKey);
		crypto::public_key signer_key;
		memcpy(&signer_key, &key, sizeof(signer_key));
		Exc::Test(crypto::check_prime_order_key(signer_key));
		const auto found = std::find(m_vPks.begin(), m_vPks.end(), key);
		if (found == m_vPks.end())
		{
			Exc::Test(m_vPks.size() < vm::limits::signers);
			m_vPks.push_back(key);
			const auto& frame = m_FarCalls.m_Stack.back();
			m_vSignerOrigins.push_back({frame.m_Cid, frame.m_Method, m_FarCalls.m_Stack.size() > 1});
		}
		else if (m_FarCalls.m_Stack.size() > 1)
			m_vSignerOrigins[found - m_vPks.begin()].nested = true;
	}

	BVM_METHOD(get_Height)
	{
		return m_Height;
	}

	BVM_METHOD(get_ParentHdr)
	{
		auto& res = get_AddrAsW<ParentHeader>(hdr);
		res = m_ParentHdr;
		res.Convert<true>();
	}

	BVM_METHOD(get_HdrInfo)
	{
		auto& res = get_AddrAsW<ParentHeader>(hdr);
		auto requested = res;
		requested.Convert<false>();
		const Height height = requested.m_Height;
		res = {};
		if (height == 0 || height >= m_Height)
			return 0;
		if (height == m_ParentHdr.m_Height)
			res = m_ParentHdr;
		else if (!m_HdrInfoProvider || !m_HdrInfoProvider(height, res) || res.m_Height != height)
		{
			res = {};
			return 0;
		}
		res.Convert<true>();
		return 1;
	}

	BVM_METHOD(CheckSignature)
	{
		return crypto::check_signature(get_AddrAsR<HashValue>(hash), get_AddrAsR<PubKey>(pubKey), get_AddrAsR<Signature>(sig)) ? 1 : 0;
	}


	struct Processor::DataProcessor::Instance
	{
		static uint32_t CopyOut(uint8_t* p, uint32_t n, const uint8_t* pRes, uint32_t nRes)
		{
			std::setmin(n, nRes);
			memcpy(p, pRes, n);
			return n;
		}

		struct Sha256
			:public Base
		{
			crypto_hash_sha256_state m_State;

			Sha256() { crypto_hash_sha256_init(&m_State); }

			void Write(const uint8_t* p, uint32_t n) override
			{
				crypto_hash_sha256_update(&m_State, p, n);
			}
			uint32_t Read(uint8_t* p, uint32_t n) override
			{
				auto s = m_State;
				uint8_t hv[crypto_hash_sha256_BYTES];
				crypto_hash_sha256_final(&s, hv);
				return CopyOut(p, n, hv, sizeof(hv));
			}
			void Clone(std::unique_ptr<Base>& pOut) const override
			{
				auto pRes = std::make_unique<Sha256>();
				pRes->m_State = m_State;
				pOut = std::move(pRes);
			}
		};

		struct Blake2b
			:public Base
		{
			crypto_generichash_blake2b_state m_State;
			uint32_t m_ResultSize = 0;

			bool Init(const void* pPersonal, uint32_t nPersonal, uint32_t nResultSize)
			{
				if ((nResultSize < crypto_generichash_blake2b_BYTES_MIN) || (nResultSize > crypto_generichash_blake2b_BYTES_MAX))
					return false;

				uint8_t pPers[crypto_generichash_blake2b_PERSONALBYTES] = { 0 };
				memcpy(pPers, pPersonal, std::min<size_t>(sizeof(pPers), nPersonal));

				m_ResultSize = nResultSize;
				return !crypto_generichash_blake2b_init_salt_personal(&m_State, nullptr, 0, nResultSize, nullptr, pPers);
			}

			void Write(const uint8_t* p, uint32_t n) override
			{
				crypto_generichash_blake2b_update(&m_State, p, n);
			}
			uint32_t Read(uint8_t* p, uint32_t n) override
			{
				auto s = m_State;
				uint8_t hv[crypto_generichash_blake2b_BYTES_MAX];
				crypto_generichash_blake2b_final(&s, hv, m_ResultSize);
				return CopyOut(p, n, hv, m_ResultSize);
			}
			void Clone(std::unique_ptr<Base>& pOut) const override
			{
				auto pRes = std::make_unique<Blake2b>();
				pRes->m_State = m_State;
				pRes->m_ResultSize = m_ResultSize;
				pOut = std::move(pRes);
			}
		};

		template <uint32_t nBits>
		struct Keccak
			:public Base
		{
			static const uint32_t s_Bytes = nBits / 8;
			ByteBuffer m_Buf;

			void Write(const uint8_t* p, uint32_t n) override
			{
				Exc::Test(n <= vm::limits::heap_size - m_Buf.size());
				m_Buf.insert(m_Buf.end(), p, p + n);
			}
			uint32_t Read(uint8_t* p, uint32_t n) override
			{
				static const uint8_t s_Empty = 0;
				uint8_t hv[s_Bytes];
				keccak(m_Buf.empty() ? &s_Empty : m_Buf.data(), m_Buf.size(), hv, s_Bytes);
				return CopyOut(p, n, hv, s_Bytes);
			}
			void Clone(std::unique_ptr<Base>& pOut) const override
			{
				auto pRes = std::make_unique<Keccak>();
				pRes->m_Buf = m_Buf;
				pOut = std::move(pRes);
			}
		};

	};

	Processor::DataProcessor::Base& Processor::DataProcessor::FindStrict(uint32_t key)
	{
		auto it = m_Map.find(key, Base::Comparator());
		Exc::Test(m_Map.end() != it);
		return *it;
	}

	uint32_t Processor::AddHash(std::unique_ptr<DataProcessor::Base>&& p)
	{
		if (m_DataProcessor.m_Map.size() >= vm::limits::hash_objects)
			return 0;

		DischargeUnits(vm::gas::hash_op);

		uint32_t ret = m_DataProcessor.m_Map.empty() ? 1 : (m_DataProcessor.m_Map.rbegin()->m_Key + 1);
		p->m_Key = ret;
		m_DataProcessor.m_Map.insert(*p.release());

		return ret;
	}

	uint32_t Processor::CloneHash(const DataProcessor::Base& x)
	{
		std::unique_ptr<DataProcessor::Base> pHash;
		x.Clone(pHash);
		return AddHash(std::move(pHash));
	}

	BVM_METHOD(HashCreateSha256)
	{
		return AddHash(std::make_unique<DataProcessor::Instance::Sha256>());
	}

	BVM_METHOD(HashCreateBlake2b)
	{
		auto pRet = std::make_unique<DataProcessor::Instance::Blake2b>();
		if (!pRet->Init(get_AddrR(pPersonal, nPersonal), nPersonal, nResultSize))
			return 0;

		return AddHash(std::move(pRet));
	}

	BVM_METHOD(HashCreateKeccak)
	{
		switch (nBits)
		{
		case 256:
			return AddHash(std::make_unique<DataProcessor::Instance::Keccak<256> >());
		case 384:
			return AddHash(std::make_unique<DataProcessor::Instance::Keccak<384> >());
		case 512:
			return AddHash(std::make_unique<DataProcessor::Instance::Keccak<512> >());
		}

		return 0;
	}

	BVM_METHOD(HashWrite)
	{
		DischargeUnits(vm::gas::hash_write_for(size));

		m_DataProcessor.FindStrict(pHash).Write(get_AddrR(p, size), size);
	}

	BVM_METHOD(HashGetValue)
	{
		DischargeUnits(vm::gas::hash_read_for(size));

		uint8_t* pDst_ = get_AddrW(pDst, size);
		uint32_t n = m_DataProcessor.FindStrict(pHash).Read(pDst_, size);
		memset0(pDst_ + n, size - n);
	}

	BVM_METHOD(HashFree)
	{
		m_DataProcessor.m_Map.Delete(m_DataProcessor.FindStrict(pHash));
	}

	BVM_METHOD(HashClone)
	{
		return CloneHash(m_DataProcessor.FindStrict(pHash));
	}


	bool ProcessorContract::LoadFixedOrZero(const VarKey& vk, uint8_t* pVal, uint32_t n)
	{
		Blob res;
		get_Storage().LoadVar(vk.ToBlob(), res);

		if (n == res.n)
		{
			memcpy(pVal, res.p, n);
			return true;
		}

		memset0(pVal, n);
		return false;
	}

	void ProcessorContract::TestCanWrite()
	{
		assert(!m_FarCalls.m_Stack.empty());

		uint32_t nFlags = m_FarCalls.m_Stack.back().m_Flags;
		if ((FarCalls::Flags::s_LockedRO | FarCalls::Flags::s_GlobalRO) & nFlags)
			Exc::Fail("write lock");
	}

	uint32_t ProcessorContract::SaveVarInternal(const Blob& key, const Blob& val)
	{
		TestCanWrite();
		return get_Storage().SaveVar(key, val);
	}

	uint32_t ProcessorContract::SaveNnz(const VarKey& vk, const uint8_t* pVal, uint32_t n)
	{
		return SaveVarInternal(vk.ToBlob(), Blob(pVal, memis0(pVal, n) ? 0 : n));
	}

	void ProcessorContract::HandleAmount(Amount amount, bool bLock)
	{
		HandleAmountInner(amount, bLock);
		HandleAmountOuter(amount, bLock);
	}

	void ProcessorContract::HandleAmountInner(Amount amount, bool bLock)
	{
		struct MyCheckpoint :public Exc::Checkpoint
		{
			void Dump(std::ostream& os) override {
				os << "Funds I/O";
			}

			uint32_t get_Type() override {
				return ErrorSubType::FundsIO;
			}

		} cp;

		VarKey vk;
		SetVarKey(vk, VarKey::Tag::LockedAmount, Blob());

		uint8_t pVal[sizeof(Amount)];
		LoadFixedOrZero(vk, pVal, sizeof(pVal));

		Amount val0;
		memcpy(&val0, pVal, sizeof(val0));
		val0 = ByteOrder::from_be(val0);

		if (bLock)
		{
			val0 += amount;
			Exc::Test(val0 >= amount);
		}
		else
		{
			Exc::Test(val0 >= amount);
			val0 -= amount;
		}

		val0 = ByteOrder::to_be(val0);
		memcpy(pVal, &val0, sizeof(val0));
		SaveNnz(vk, pVal, sizeof(pVal));
	}

	void ProcessorContract::HandleAmountOuter(Amount amount, bool bLock)
	{
		if (!m_pFundsIO)
			return;

		struct MyCheckpoint :public Exc::Checkpoint
		{
			void Dump(std::ostream& os) override {
				os << "Funds I/O";
			}

			uint32_t get_Type() override {
				return ErrorSubType::FundsIO;
			}

		} cp;

		Amount& val = bLock ? m_pFundsIO->m_Locked : m_pFundsIO->m_Unlocked;
		val += amount;
		Exc::Test(val >= amount);

		if (bLock)
			Exc::Test(m_pFundsIO->m_Locked <= m_pFundsIO->m_Deposit);
	}


	BVM_METHOD(DocAddText)
	{
		uint32_t nId, nValue;
		const char* szIdPtr = RealizeStr(szId, nId);
		std::string sId(szIdPtr, nId);
		const char* szValuePtr = RealizeStr(szValue, nValue);
		RequireAppHost().AddText(sId, std::string(szValuePtr, nValue));
	}

	BVM_METHOD(DocAddNum)
	{
		uint32_t nId;
		const char* szIdPtr = RealizeStr(szId, nId);
		RequireAppHost().AddNum(std::string(szIdPtr, nId), value);
	}

	BVM_METHOD(DocAddBlob)
	{
		uint32_t nId;
		const char* szIdPtr = RealizeStr(szId, nId);
		std::string sId(szIdPtr, nId);
		DischargeMemOp(n);
		RequireAppHost().AddBlob(sId, Blob(get_AddrR(p, n), n));
	}

	BVM_METHOD(DocOpenGroup)
	{
		uint32_t nId;
		const char* szIdPtr = RealizeStr(szId, nId);
		RequireAppHost().OpenGroup(std::string(szIdPtr, nId));
	}

	BVM_METHOD(DocCloseGroup)
	{
		RequireAppHost().CloseGroup();
	}

	BVM_METHOD(DocGetText)
	{
		uint32_t nId;
		const char* szIdPtr = RealizeStr(szId, nId);
		std::string sValue;
		if (!RequireAppHost().GetText(std::string(szIdPtr, nId), sValue))
			return 0;

		if (nOutMax)
		{
			uint32_t nCopy = std::min<uint32_t>(nOutMax - 1, static_cast<uint32_t>(sValue.size()));
			DischargeMemOp(nCopy + 1);
			char* pDst = reinterpret_cast<char*>(get_AddrW(szOut, nCopy + 1));
			memcpy(pDst, sValue.data(), nCopy);
			pDst[nCopy] = 0;
		}

		return static_cast<uint32_t>(sValue.size()) + 1;
	}

	BVM_METHOD(DocGetNum)
	{
		uint32_t nId;
		const char* szIdPtr = RealizeStr(szId, nId);
		uint64_t value = 0;
		if (!RequireAppHost().GetNum(std::string(szIdPtr, nId), value))
			return 0;

		value = ByteOrder::to_le(value);
		memcpy(get_AddrW(pOut, sizeof(value)), &value, sizeof(value));
		return 1;
	}

	BVM_METHOD(VarRead)
	{
		uint8_t* pDst = get_AddrW(pVal, nVal);
		std::string sValue;
		if (!RequireAppHost().ReadVar(Blob(get_AddrR(pKey, nKey), nKey), sValue))
			return 0;

		uint32_t nCopy = std::min<uint32_t>(nVal, static_cast<uint32_t>(sValue.size()));
		DischargeMemOp(nCopy);
		memcpy(pDst, sValue.data(), nCopy);
		return static_cast<uint32_t>(sValue.size());
	}

	BVM_METHOD(GenerateCall)
	{
		DischargeMemOp(nArgs);
		RequireAppHost().GenerateCall(get_AddrAsR<ContractID>(cid), iMethod, Blob(get_AddrR(pArgs, nArgs), nArgs), deposit);
	}

	BVM_METHOD(GenerateDeploy)
	{
		DischargeMemOp(nCode + nArgs);
		RequireAppHost().GenerateDeploy(Blob(get_AddrR(pCode, nCode), nCode), Blob(get_AddrR(pArgs, nArgs), nArgs), deposit);
	}

	BVM_METHOD(GenerateCallSigned)
	{
		Exc::Test(nArgs <= vm::limits::args_size && nSigners <= vm::limits::signers);
		DischargeMemOp(nArgs + nSigners * sizeof(PubKey));
		RequireAppHost().GenerateCallSigned(get_AddrAsR<ContractID>(cid), iMethod, Blob(get_AddrR(pArgs, nArgs), nArgs), deposit,
			reinterpret_cast<const PubKey*>(get_AddrR(pSigners, nSigners * sizeof(PubKey))), nSigners);
	}

	BVM_METHOD(GenerateDeploySigned)
	{
		Exc::Test(nCode <= vm::limits::module_size && nArgs <= vm::limits::args_size && nSigners <= vm::limits::signers);
		DischargeMemOp(nCode + nArgs + nSigners * sizeof(PubKey));
		RequireAppHost().GenerateDeploySigned(Blob(get_AddrR(pCode, nCode), nCode), Blob(get_AddrR(pArgs, nArgs), nArgs), deposit,
			reinterpret_cast<const PubKey*>(get_AddrR(pSigners, nSigners * sizeof(PubKey))), nSigners);
	}

	BVM_METHOD(DerivePk)
	{
		Exc::Test(RequireAppHost().DerivePk(get_AddrAsR<ContractID>(cid), get_AddrAsR<HashValue>(keyId), get_AddrAsW<PubKey>(pubKey)));
	}

	void ProcessorContract::AddRemoveShader(const ContractID& cid, const Blob* pCode)
	{
		get_Storage().SaveVar(ToBlob(cid), pCode ? *pCode : Blob());
	}

}
}
