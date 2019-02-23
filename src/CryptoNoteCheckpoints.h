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
	{ 10000,"fb021fd69f78a60a365c16692777e7f699215404366545e072eba7dddbf1d61d"},
	{ 20000,"d32286163e2a5cfbbab35007438e7bf90564afee70c715930606710e96f2ce19"},
	{ 30000,"1d32bbca2149eeb27ff9e2c19d1b6ace4a160764839a4da7031328f7ea49e6f6"},
	{ 40001,"17132deac10a3661f56eb4b19dbaea77cbf2ef8a131f78df04c7df023b687158"},//2.0
	{ 46001,"15853b6a46d3fcf1554935dadc8988a0de95b0a9e711cfb845d6fe3fde5aa98a"},//3.0
	{ 50000,"5599ca3d8ac58f377187b5a43c00103496244ec95299e4ae3dff3be6016488bf"},
	{100000,"d5b20487d7a4b76d80dc2ae5e51da7feb9190a88264492ecc2f965e4f30b737d"},
	{110521,"da28b56d63c1be49b6c89f16b4c3d2da3da14dcc98ec38079aedef9b83e627ce"},//4.0
	{150000,"0d693d183d662d53101d9729a5e03f4fe87a378c4dacbb642842cdaf6a75c224"},
	{173136,"36a9ab5f39cf4a1d612c4e10c1a72b7d147ab88f92e511af84265b7d04804dc0"},//4.0.2
	{200000,"22d739a610c5cb7ca4a2debd19b4bac38c0e0df2393de67ab131eec0b6537ec2"},
	{250000,"49f018137426816a623422aa0c7fd36fa294d59f8c2abce052c3e19d9ee2ddd8"},
	{250721,"a119b644ec0a35c77f8cf7ed6a0ff4d37b7a279ac8fadfb660e83cc795ddb7de"},//5.0
	{260000,"cb4b1fd2734dda8ca121d4bf7e8b8612a1c1e88f26d8aac226caf3f10023b567"},
	{267380,"4d05e311dc3683f7ffc72f78d5472f0f03284ad2e2fb0b77b4fa15e6826e8a0a"}
	};
}
