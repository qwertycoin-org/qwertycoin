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
#include <functional>
#include "wasm_interpreter.h"
#include "containers.h"
#include "../limits.h"
#include "crypto/crypto.h"
#include "crypto/hash.h"

namespace Shaders {

	typedef crypto::hash HashValue;
	typedef crypto::hash ContractID;
	typedef crypto::hash ShaderID;
	typedef crypto::public_key PubKey;
	typedef crypto::signature Signature;
	using beam::Amount;
	using beam::Height;
	using beam::Timestamp;

	template<bool bToShader, typename T>
	inline void ConvertOrd(T& x)
	{
		if constexpr (bToShader)
			x = beam::ByteOrder::to_le(x);
		else
			x = beam::ByteOrder::from_le(x);
	}

#include "bvm2_shared.h"
}

namespace beam {
namespace bvm2 {

	using Shaders::PubKey;
	using Shaders::Signature;
	using Shaders::ContractID;
	using Shaders::ShaderID;
	using Shaders::HashValue;
	using Shaders::Amount;
	using Shaders::Height;
	using Shaders::HashObj;
	using Shaders::CallFarFlags;
	using Shaders::ParentHeader;

	struct ErrorSubType
	{
		static const uint32_t NoCharge = 1;
		static const uint32_t Internal = 2;
		static const uint32_t FundsIO = 4;
	};

	inline Blob ToBlob(const crypto::hash& x) { return Blob(&x, sizeof(x)); }

	void get_ShaderID(ShaderID&, const Blob& data);
	void get_Cid(ContractID&, const Blob& data, const Blob& args);
	void get_CidViaSid(ContractID&, const ShaderID&, const Blob& args);

	namespace Storage
	{
		struct IBase
		{
			virtual ~IBase() = default;
			virtual void LoadVar(const Blob&, Blob& res) = 0;
			virtual uint32_t SaveVar(const Blob&, const Blob& val) = 0;
			virtual uint32_t OnLog(const Blob&, const Blob& val) = 0;
		};

		struct Dummy
			:public IBase
		{
			void LoadVar(const Blob&, Blob& res) override {
				res.n = 0;
			}
			uint32_t SaveVar(const Blob&, const Blob& val) override {
				return 0;
			}
			uint32_t OnLog(const Blob&, const Blob& val) override {
				return 0;
			}
		};

	}

	namespace App
	{
		struct IHost
		{
			virtual ~IHost() = default;
			virtual void AddText(const std::string& id, const std::string& value) = 0;
			virtual void AddNum(const std::string& id, uint64_t value) = 0;
			virtual void AddBlob(const std::string& id, const Blob& value) = 0;
			virtual void OpenGroup(const std::string& id) = 0;
			virtual void CloseGroup() = 0;
			virtual bool GetText(const std::string& id, std::string& value) = 0;
			virtual bool GetNum(const std::string& id, uint64_t& value) = 0;
			virtual bool ReadVar(const Blob& key, std::string& value) = 0;
			virtual void GenerateCall(const ContractID&, uint32_t iMethod, const Blob& args, Amount deposit) = 0;
			virtual void GenerateDeploy(const Blob& code, const Blob& args, Amount deposit) = 0;
			virtual void GenerateCallSigned(const ContractID&, uint32_t iMethod, const Blob& args, Amount deposit, const PubKey* signers, uint32_t count) = 0;
			virtual void GenerateDeploySigned(const Blob& code, const Blob& args, Amount deposit, const PubKey* signers, uint32_t count) = 0;
			virtual bool DerivePk(const ContractID&, const HashValue& keyId, PubKey&) = 0;
		};
	}

	class Processor
		:public Wasm::Processor
		,public Storage::Dummy
	{
	protected:

		std::vector<Wasm::Word> m_vStack;
		void InitBase(uint32_t nStackBytes);

		class Heap
		{
			struct Entry
			{
				struct Size
					:public intrusive::set_base_hook<uint32_t>
				{
					IMPLEMENT_GET_PARENT_OBJ(Entry, m_Size)
				} m_Size;

				struct Pos
					:public intrusive::set_base_hook<uint32_t>
				{
					IMPLEMENT_GET_PARENT_OBJ(Entry, m_Pos)
				} m_Pos;
			};

			typedef intrusive::multiset<Entry::Size> MapSize;
			typedef intrusive::multiset<Entry::Pos> MapPos;

			MapSize m_mapSize;
			MapPos m_mapFree;
			MapPos m_mapAllocated;

			void Insert(Entry&, bool bFree);
			void Remove(Entry&, bool bFree);
			void Delete(Entry&, bool bFree);
			void UpdateSizeFree(Entry&, uint32_t newVal);
			void TryMerge(Entry&);
			Entry* Create(uint32_t nPos, uint32_t nSize, bool bFree);

		public:

			std::vector<uint8_t> m_vMem;

			~Heap() { Clear(); }

			bool Alloc(uint32_t&, uint32_t size);
			void Free(uint32_t);
			void Clear();
			void OnGrow(uint32_t nOld, uint32_t nNew);

			uint32_t get_UnusedAtEnd(uint32_t nEnd) const;

			void swap(Heap&);

		} m_Heap;

		bool HeapAllocEx(uint32_t&, uint32_t size);
		void HeapFreeEx(uint32_t);
		void HeapReserveStrict(uint32_t);

		struct VarKey
		{
			typedef Shaders::KeyTag Tag;

			uint8_t m_p[sizeof(ContractID) + 1 + vm::limits::var_key];
			uint32_t m_Size;

			void Set(const ContractID&);
			void Append(uint8_t nTag, const Blob&);

			Blob ToBlob() const { return Blob(m_p, m_Size); }
		};

		virtual uint32_t get_HeapLimit() { return 0; }

		template <typename T> const T& get_AddrAsR(uint32_t nOffset) {
			return *reinterpret_cast<const T*>(get_AddrR(nOffset, sizeof(T)));
		}
		template <typename T> T& get_AddrAsW(uint32_t nOffset) {
			return *reinterpret_cast<T*>(get_AddrW(nOffset, sizeof(T)));
		}

		struct Header;
		const Header& ParseMod();

		const char* RealizeStr(Wasm::Word, uint32_t& nLenOut);

		void DischargeMemOp(uint32_t size);
		virtual void DischargeUnits(uint64_t) {}

		struct DataProcessor
		{
			struct Base
				:public intrusive::set_base_hook<uint32_t>
			{
				virtual ~Base() {}
				virtual void Write(const uint8_t*, uint32_t) = 0;
				virtual uint32_t Read(uint8_t*, uint32_t) = 0;
				virtual void Clone(std::unique_ptr<Base>&) const = 0;
			};

			typedef intrusive::multiset_autoclear<Base> Map;
			Map m_Map;

			struct Instance;

			Base& FindStrict(uint32_t);

		} m_DataProcessor;

		uint32_t AddHash(std::unique_ptr<DataProcessor::Base>&&);
		uint32_t CloneHash(const DataProcessor::Base&);

	public:

		virtual Storage::IBase& get_Storage() {
			return *this;
		}

		struct Compiler
		{
			enum class Kind { Contract, App };

			explicit Compiler(Kind kind = Kind::Contract) :m_Kind(kind) {}

			void Compile(ByteBuffer&, const Blob&);

		private:
			Kind m_Kind;
			bool ResolveBinding(Wasm::Compiler& c, Wasm::Compiler::PerImportFunc&);
			void ResolveBindings(Wasm::Compiler&);
			static int32_t get_PublicMethodIdx(const Wasm::Compiler::Vec<char>& sName);
		};

		static void Compile(ByteBuffer&, const Blob&);
		static void CompileApp(ByteBuffer&, const Blob&);
	};

	struct ProcessorPlus;

	class ProcessorContract
		:public Processor
	{
	protected:

		friend struct ProcessorPlus;

		void SetVarKey(VarKey&);
		void SetVarKey(VarKey&, uint8_t nTag, const Blob&);
		void SetVarKeyFromShader(VarKey&, uint8_t nTag, const Blob&, bool bW);

		struct FarCalls
		{
			struct Flags
			{
				static const uint32_t s_CallerBlocked = 1;
				static const uint32_t s_CallerLockedRO = 2;
				static const uint32_t s_HeapSwap = 4;
				static const uint32_t s_LockedRO = 8;
				static const uint32_t s_GlobalRO = 0x10;
			};

			struct Frame
				:public boost::intrusive::list_base_hook<>
			{
				ContractID m_Cid;
				uint32_t m_Method;
				ByteBuffer m_Body;
				Wasm::Word m_FarRetAddr;
				Wasm::Word m_StackPosMin;
				Wasm::Word m_StackBytesMax;
				Wasm::Word m_StackBytesRet;

				uint32_t m_Flags;
				Heap m_Heap;
				Blob m_Args;
			};

			intrusive::list_autoclear<Frame> m_Stack;

		} m_FarCalls;

		bool LoadFixedOrZero(const VarKey&, uint8_t* pVal, uint32_t);
		uint32_t SaveNnz(const VarKey&, const uint8_t* pVal, uint32_t);

		void TestCanWrite();
		uint32_t SaveVarInternal(const Blob&, const Blob& val);

		void InvokeExt(uint32_t) override;
		void OnRet(Wasm::Word nRetAddr) override;
		uint32_t get_HeapLimit() override;
		void DischargeUnits(uint64_t) override;

		void HandleAmount(Amount, bool bLock);
		void HandleAmountInner(Amount, bool bLock);
		void HandleAmountOuter(Amount, bool bLock);

		void TestVarSize(uint32_t n)
		{
			Exc::Test(n <= vm::limits::var_value);
		}

		void OnRetFar();

		virtual App::IHost* get_AppHost() { return nullptr; }
		App::IHost& RequireAppHost();

	public:

		struct FundsIO
		{
			Amount m_Deposit = 0;
			Amount m_Locked = 0;
			Amount m_Unlocked = 0;
		};

		void InitStackPlus(uint32_t nStackBytesExtra);

		FundsIO* m_pFundsIO = nullptr;
		Height m_Height = 0;
		ParentHeader m_ParentHdr = {};
		std::function<bool(Height, ParentHeader&)> m_HdrInfoProvider;
		bool m_HdrInfoActive = false;
		std::vector<PubKey> m_vPks;
		struct SignerOrigin
		{
			ContractID cid;
			uint32_t method;
			bool nested;
		};
		std::vector<SignerOrigin> m_vSignerOrigins;

		void AddRemoveShader(const ContractID&, const Blob*);

		bool IsDone() const { return m_FarCalls.m_Stack.empty(); }

		uint64_t m_Charge = 0;

		void CallFar(const ContractID&, uint32_t iMethod, Wasm::Word pArgs, uint32_t nArgs, uint32_t nFlags);
	};

}
}
