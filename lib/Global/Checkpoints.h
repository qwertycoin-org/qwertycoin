// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018, The TurtleCoin Developers
// Copyright (c) 2018-2020, The Qwertycoin Group.
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

struct CheckpointData
{
    uint32_t height;
    const char *blockId;
};

const std::initializer_list<CheckpointData> CHECKPOINTS = {
    {     1,"6f783fc255bd66ed936e2aa03ce4ae908ff4e93cd4b7816d2734b92b7ff8f83d"},
    {    10,"28808dcf876eb0d03b97a057c0c140ebc692683e38d946903923212d769aae31"},
    {   100,"7a2fd678850b5513648b89bceac345af2d77739cf3689a4deb49a0e407cd9bb3"},
    {  1000,"ec5920066d972658840a29bf7cb723cf92c35883046538fc04f0604f0ee217b3"},
    { 10000,"fb021fd69f78a60a365c16692777e7f699215404366545e072eba7dddbf1d61d"},
    { 15000,"fb8ad336e6518aa6a5f874a15276561249d36a35afd08f0f1fab78333d9e4149"},
    { 20000,"d32286163e2a5cfbbab35007438e7bf90564afee70c715930606710e96f2ce19"},
    { 30000,"1d32bbca2149eeb27ff9e2c19d1b6ace4a160764839a4da7031328f7ea49e6f6"},
    { 40000,"60a9694b18cf470bcfd9f36f32ad01f86b5538fe1f88a3bd2717ca6ad1c7ce80"},//v2.0.0
    { 45000,"0e504e82e6fe247b0b5dae9fc5bd3e82982e61610fd674d8e1f8ee3c85000c15"},
    { 46000,"51e2f9e09278cb66b08efcecfdf3208053ee53030c6fc5a6a647565a6b318cd5"},//v3.0.0
    { 50000,"5599ca3d8ac58f377187b5a43c00103496244ec95299e4ae3dff3be6016488bf"},
    {100000,"d5b20487d7a4b76d80dc2ae5e51da7feb9190a88264492ecc2f965e4f30b737d"},
    {110520,"a2d08d171265aa0c4ee9e5e5baebd191940f86ccc7b01cd8ebfa2276db98736c"},//v4.0.0
    {120000,"b31f8f2284f51cecf8028876ddd6df7c812b7fbc274ff7bab303f1fcc3b3310b"},
    {160000,"d372a4f56bb020bbeef7dc3543ebde9076110bbc3390a70f302a4565743a2fcd"},
    {173136,"36a9ab5f39cf4a1d612c4e10c1a72b7d147ab88f92e511af84265b7d04804dc0"},//v4.0.2
    {180000,"f4b74b907bcf622b4b7353c22886b502fc0e3fdbd71ba6087b9248d80eddbdae"},
    {230000,"190891fd56763afbc9e7c609a3a90bcb801a504daf2e352060cf1f7dfb9f2896"},
    {240000,"522cbcb5341a1c4a804212923713227363671a2db3ea41f8957a15b6234b17f4"},
    {245000,"31cd9378eb1031574b504342fd73a6f57be94059d3023fdc7ec564d53de50b24"},
    {250721,"a119b644ec0a35c77f8cf7ed6a0ff4d37b7a279ac8fadfb660e83cc795ddb7de"},//v5.0.0
    {255000,"a6d7494587113d76e5c68e309c82e458443fe85383b2a4bdb86bcb82ea2a7345"},
    {260000,"cb4b1fd2734dda8ca121d4bf7e8b8612a1c1e88f26d8aac226caf3f10023b567"},
    {265000,"3439629cd14bcff43240cdd0444cd075fddd411d3a5ad0ce0aadab5434c920f2"},
    {270000,"3b8802ec65b809de52c78dd5c34c27d65fdcf6eaddaf2e295c63958c9061a4b6"},
    {275213,"ba51659f63ef31685c0b9e816d9c3b9455fe20cc34aa8ef09a186153e83547a9"},//v5.1.3
    {300000,"a02e87603e60f3186a232e7d8405d4d1d130bfc2d1d007e7beed8c14a09026eb"},
    {305000,"1a2aafc52c02add00d78acbb3fbabbe87764ce0d01808539df7110b3c167f3fe"},
    {308323,"12253186704f9898f390fcfe2550ee98f2331dbf9edda9c55ca8fb218f3eca37"},//v5.1.4
    {325000,"8c8171a4c00e57468f485f62cbe777d3dfefa018b5e084fc2986cdd48eeff8bb"},
    {350000,"309e444aa88e588b59c7c9053a552fe76994d52c37648e7da4760aa26fe99301"},
    {400000,"c787cfb334eb48560ceffb0402324d9a049f40f54fcd899196d002926b5d374b"},
    {450000,"d0f32e38d52d9c91ddee93883e07ff95c8d6f415b96ec1eed36b8632776726cd"},
    {500000,"432b0c6cfbb5967950efc2f2358f42dc85ddbc5dd270b619cd1f466970a41236"},
    {545000,"08ec3dd94c1e9ab03fa222cddfba2802c3f0d9f7aef26fb8a67c65058e7ec7ba"},
    {550000,"8f95f91fc021440339c94f7e38ed66d675ac98666abccb56a96e3aaafad7fd15"},
    {600000,"d74f649bb99983a93571b4dfa06a9f34616fc74f1e743dd4131bd0abbbc207f9"},
    {650000,"7d2db8650d40babede87c4d03a9536bb5f3eaa8e99bfca20f5e9bf9133649aa5"}
    /*{700000,"                                                              "} //v6.0.0 */
};

} // namespace CryptoNote
