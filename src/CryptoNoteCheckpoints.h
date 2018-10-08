// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018, The TurtleCoin Developers, The Qwertycoin Developers
//
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
#include <initializer_list>

namespace CryptoNote {
struct CheckpointData {
  uint32_t height;
  const char* blockId;
};

const std::initializer_list<CheckpointData> CHECKPOINTS = {  
	{  5000,"f44ad7e79c4dc4b8cce6100460444f8fc61096a6cf4d9a53107abde0b8750b4f"},
	{ 10000,"fb021fd69f78a60a365c16692777e7f699215404366545e072eba7dddbf1d61d"},
	{ 40000,"60a9694b18cf470bcfd9f36f32ad01f86b5538fe1f88a3bd2717ca6ad1c7ce80"},//2.0
	{ 40001,"17132deac10a3661f56eb4b19dbaea77cbf2ef8a131f78df04c7df023b687158"},
	{ 46000,"51e2f9e09278cb66b08efcecfdf3208053ee53030c6fc5a6a647565a6b318cd5"},//3.0
	{ 46001,"15853b6a46d3fcf1554935dadc8988a0de95b0a9e711cfb845d6fe3fde5aa98a"},
	{110520,"a2d08d171265aa0c4ee9e5e5baebd191940f86ccc7b01cd8ebfa2276db98736c"},//4.0.1
	{170000,"b7df3e82d6798ab93bf56314ef619ae9709af1750530446e4981637c46c91f1f"},
	{173136,"36a9ab5f39cf4a1d612c4e10c1a72b7d147ab88f92e511af84265b7d04804dc0"},//4.0.2
	{174788,"f34fae873f86b6dda45c5b83396f7e6cd4d5cb6f9283569ec5c28a34c30dd4fb"}
	};
}
