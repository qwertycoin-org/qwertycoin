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


#include "common.h"

bool memis0(const void* p, size_t n)
{
	for (size_t i = 0; i < n; i++)
		if (((const uint8_t*)p)[i])
			return false;
	return true;
}

namespace beam
{
	Blob::Blob(const ByteBuffer& bb)
	{
		if ((n = (uint32_t)bb.size()) != 0)
			p = &bb.at(0);
	}

	void Blob::Export(ByteBuffer& x) const
	{
		if (n)
		{
			x.resize(n);
			memcpy(&x.at(0), p, n);
		}
		else
			x.clear();
	}

	int Blob::cmp(const Blob& x) const
	{
		int nRet = memcmp(p, x.p, std::min(n, x.n));
		if (nRet)
			return nRet;

		if (n < x.n)
			return -1;

		return (n > x.n);
	}


	thread_local Exc::Checkpoint* Exc::Checkpoint::s_pTop = nullptr;

	Exc::Checkpoint::Checkpoint()
	{
		m_pNext = s_pTop;
		s_pTop = this;
	}

	Exc::Checkpoint::~Checkpoint()
	{
		s_pTop = m_pNext;
	}

	uint32_t Exc::Checkpoint::DumpAll(std::ostream& os)
	{
		uint32_t ret = 0;
		for (Checkpoint* p = s_pTop; p; p = p->m_pNext)
		{
			os << " <- ";
			p->Dump(os);

			if (!ret)
				ret = p->get_Type();
		}
		return ret;
	}

	void Exc::CheckpointTxt::Dump(std::ostream& os) {
		os << m_sz;
	}

	void Exc::Fail()
	{
		Fail("Error");
	}

	void Exc::Fail(const char* sz)
	{
		std::ostringstream os;
		os << sz << ": ";

		uint32_t nType = Checkpoint::DumpAll(os);

		Exc exc(os.str());
		exc.m_Type = nType;

		throw exc;
	}

}
