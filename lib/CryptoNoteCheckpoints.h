// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018, The TurtleCoin Developers
// Copyright (c) 2018-2019, The Qwertycoin Developers
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
	{     1,"6f783fc255bd66ed936e2aa03ce4ae908ff4e93cd4b7816d2734b92b7ff8f83d"},
	{    10,"28808dcf876eb0d03b97a057c0c140ebc692683e38d946903923212d769aae31"},
	{   100,"7a2fd678850b5513648b89bceac345af2d77739cf3689a4deb49a0e407cd9bb3"},
	{  1000,"ec5920066d972658840a29bf7cb723cf92c35883046538fc04f0604f0ee217b3"},
	{ 10000,"fb021fd69f78a60a365c16692777e7f699215404366545e072eba7dddbf1d61d"},
	{ 20000,"d32286163e2a5cfbbab35007438e7bf90564afee70c715930606710e96f2ce19"},
	{ 30000,"1d32bbca2149eeb27ff9e2c19d1b6ace4a160764839a4da7031328f7ea49e6f6"},
	{ 40000,"60a9694b18cf470bcfd9f36f32ad01f86b5538fe1f88a3bd2717ca6ad1c7ce80"},//2.0
	{ 46000,"51e2f9e09278cb66b08efcecfdf3208053ee53030c6fc5a6a647565a6b318cd5"},//3.0
	{ 50000,"5599ca3d8ac58f377187b5a43c00103496244ec95299e4ae3dff3be6016488bf"},
	{ 60000,"796e0037bed22ec317dc165bdfcc331322fe3fcc7ca162827ab374a7d17107f0"},
	{ 70000,"9b035668d72b17ceaabbd4b341089f090aa5ebad37f33513509d9f6e102c7c0c"},
	{ 80000,"02bdbd6cab951c0685796598b353524d450f947c58d8e1a8efbc05c334663ad7"},
	{ 90000,"96e00099a12e4e50a84d361942a05ff026f2e0f413f0f9afefc8af7085dde9da"},
	{100000,"d5b20487d7a4b76d80dc2ae5e51da7feb9190a88264492ecc2f965e4f30b737d"},
	{110520,"a2d08d171265aa0c4ee9e5e5baebd191940f86ccc7b01cd8ebfa2276db98736c"},//4.0
	{120000,"b31f8f2284f51cecf8028876ddd6df7c812b7fbc274ff7bab303f1fcc3b3310b"},
	{130000,"7c171f2a4d40a136ba9f309895cc695f957a783736f492a4bb9613905cd62256"},
	{140000,"60c532ab429aaba8027528d39499fb935d00c53a0b73559a03434b2fc6b01c9f"},
	{150000,"0d693d183d662d53101d9729a5e03f4fe87a378c4dacbb642842cdaf6a75c224"},
	{160000,"d372a4f56bb020bbeef7dc3543ebde9076110bbc3390a70f302a4565743a2fcd"},
	{173136,"36a9ab5f39cf4a1d612c4e10c1a72b7d147ab88f92e511af84265b7d04804dc0"},//4.0.2
	{180000,"f4b74b907bcf622b4b7353c22886b502fc0e3fdbd71ba6087b9248d80eddbdae"},
	{190000,"40fce151760a333b81a66c1e41c91d0b804fc7efb7c06fc5f9ecb1e20c8323e7"},
	{200000,"22d739a610c5cb7ca4a2debd19b4bac38c0e0df2393de67ab131eec0b6537ec2"},
	{210000,"ced2e2f563172da1805522b443397e9c85ee7a5aa49d5674a2d61b491e0f7e4f"},
	{220000,"2d7d54263c939bdaba1892946d9292b24cced789cbc5d14b6164ab8b57658f75"},
	{230000,"190891fd56763afbc9e7c609a3a90bcb801a504daf2e352060cf1f7dfb9f2896"},
	{240000,"522cbcb5341a1c4a804212923713227363671a2db3ea41f8957a15b6234b17f4"},
	{250721,"a119b644ec0a35c77f8cf7ed6a0ff4d37b7a279ac8fadfb660e83cc795ddb7de"},//5.0.0 was born
	{260000,"cb4b1fd2734dda8ca121d4bf7e8b8612a1c1e88f26d8aac226caf3f10023b567"},
	{275213,"ba51659f63ef31685c0b9e816d9c3b9455fe20cc34aa8ef09a186153e83547a9"},//5.1.3
	{280000,"c1c98dcc12f58cb5f75b9b2d80977b12f791dacbbdc3dbf7b90d0e4937b34a48"},
	{290000,"b20dbdb3b69b0c5d384ed049ec7e1d7d51b80ade3623f03f1f16c2524bfb6902"},
	{300000,"a02e87603e60f3186a232e7d8405d4d1d130bfc2d1d007e7beed8c14a09026eb"},
	{308323,"12253186704f9898f390fcfe2550ee98f2331dbf9edda9c55ca8fb218f3eca37"}, //5.1.4
	{310000,"1854d55e1600277c5acf74511ba5e62428c2483c704fa817195fe21d36fee3d4"},
	{313608,"9628b4b7a6500ca10a75acf5a4b27c5aa97cd9b32cee52b620e2e82c9c298edd"}
};
}
