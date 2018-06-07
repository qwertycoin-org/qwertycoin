// Copyright (c) 2018, The Qwertycoin developers
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
  uint32_t index;
  const char* blockId;
};

const std::initializer_list<CheckpointData> CHECKPOINTS = {
  { 5000,"f44ad7e79c4dc4b8cce6100460444f8fc61096a6cf4d9a53107abde0b8750b4f"},
  { 8473,"e131c96f6105604ea3f7dd2b55d73d430b28bed133112c9992a38c26b5239e14"},
  {10000,"fb021fd69f78a60a365c16692777e7f699215404366545e072eba7dddbf1d61d"},
  {15000,"fb8ad336e6518aa6a5f874a15276561249d36a35afd08f0f1fab78333d9e4149"},
  {20000,"d32286163e2a5cfbbab35007438e7bf90564afee70c715930606710e96f2ce19"},
  {25000,"c064cd86be01c32c86e0c7a6295023628acfeaa2786e2873a03343bc6f6a8d41"},
  {30000,"1d32bbca2149eeb27ff9e2c19d1b6ace4a160764839a4da7031328f7ea49e6f6"},
  {35000,"173269a8b2ed188ef95a876acaf807cbbc4817191f036063ecf92112f46b5cbf"},
  {40000,"60a9694b18cf470bcfd9f36f32ad01f86b5538fe1f88a3bd2717ca6ad1c7ce80"},
  {40001,"17132deac10a3661f56eb4b19dbaea77cbf2ef8a131f78df04c7df023b687158"},
  {45000,"0e504e82e6fe247b0b5dae9fc5bd3e82982e61610fd674d8e1f8ee3c85000c15"},
  {46000,"51e2f9e09278cb66b08efcecfdf3208053ee53030c6fc5a6a647565a6b318cd5"},
  {46001,"15853b6a46d3fcf1554935dadc8988a0de95b0a9e711cfb845d6fe3fde5aa98a"},
  {50000,"5599ca3d8ac58f377187b5a43c00103496244ec95299e4ae3dff3be6016488bf"},
  {55000,"b90e836c7ad95a6184869f16406161ac541c4ee3f2436447668a7f43362da733"},
  {60000,"796e0037bed22ec317dc165bdfcc331322fe3fcc7ca162827ab374a7d17107f0"},
  {65000,"d03c367bc1d87bea553346245ae60865175371b449dea11b79b7cfa28453d892"},
  {70000,"9b035668d72b17ceaabbd4b341089f090aa5ebad37f33513509d9f6e102c7c0c"},
  {75000,"dfe99ff9e3eaa55fd24797cd2639e156e5222a10a0c14f690f4007cbe6ce3477"},
  {80000,"02bdbd6cab951c0685796598b353524d450f947c58d8e1a8efbc05c334663ad7"},
  {85000,"9b6d12df949ad48cb6dfa178c05eacba072c6e9379fd074dddf723728923fc4b"},
  {85314,"78e773f1a6d8b96c70f1fbee2d88792a201126591f91274f0d7c809c1d650bbc"}
};
}
