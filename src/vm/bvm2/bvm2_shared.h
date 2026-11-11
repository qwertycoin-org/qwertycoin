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


#pragma pack (push, 1)

struct HashObj;

struct KeyTag
{
	static const uint8_t Internal = 0;
	static const uint8_t LockedAmount = 1;
	static const uint8_t InternalStealth = 8;

	static const uint32_t s_MaxSize = 256;
};

struct CallFarFlags
{
	static const uint32_t InheritContext = 1;
	static const uint32_t SelfBlock = 0x200;
	static const uint32_t SelfLockRO = 0x400;
	static const uint32_t GlobalLockRO = 0x800;
};

struct ParentHeader
{
	Height m_Height;
	HashValue m_Hash;
	HashValue m_Prev;
	Timestamp m_Timestamp;

	template <bool bToShader>
	void Convert()
	{
		ConvertOrd<bToShader>(m_Height);
		ConvertOrd<bToShader>(m_Timestamp);
	}
};

#pragma pack (pop)
