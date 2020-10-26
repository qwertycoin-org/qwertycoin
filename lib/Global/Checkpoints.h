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
    { 20000,"d32286163e2a5cfbbab35007438e7bf90564afee70c715930606710e96f2ce19"},
    { 30000,"1d32bbca2149eeb27ff9e2c19d1b6ace4a160764839a4da7031328f7ea49e6f6"},
    { 40000,"60a9694b18cf470bcfd9f36f32ad01f86b5538fe1f88a3bd2717ca6ad1c7ce80"},//v2.0.0
    { 45000,"0e504e82e6fe247b0b5dae9fc5bd3e82982e61610fd674d8e1f8ee3c85000c15"},
    { 46000,"51e2f9e09278cb66b08efcecfdf3208053ee53030c6fc5a6a647565a6b318cd5"},//v3.0.0
    { 75000,"dfe99ff9e3eaa55fd24797cd2639e156e5222a10a0c14f690f4007cbe6ce3477"},
    {110520,"a2d08d171265aa0c4ee9e5e5baebd191940f86ccc7b01cd8ebfa2276db98736c"},//v4.0.0
    {125000,"5d551f52c0d36ed11fd1d57261e5be373a17e2fb5b67b9af54e6c3b3b192a1b8"},
    {173136,"36a9ab5f39cf4a1d612c4e10c1a72b7d147ab88f92e511af84265b7d04804dc0"},//v4.0.2
    {200000,"22d739a610c5cb7ca4a2debd19b4bac38c0e0df2393de67ab131eec0b6537ec2"},
    {225000,"aecccbbeef7067e1150f7357869a3a47f4f39f84bf90e48cbb9b18c443956ce0"},
    {250721,"a119b644ec0a35c77f8cf7ed6a0ff4d37b7a279ac8fadfb660e83cc795ddb7de"},//v5.0.0
    {275213,"ba51659f63ef31685c0b9e816d9c3b9455fe20cc34aa8ef09a186153e83547a9"},//v5.1.3
    {300000,"a02e87603e60f3186a232e7d8405d4d1d130bfc2d1d007e7beed8c14a09026eb"},
    {308323,"12253186704f9898f390fcfe2550ee98f2331dbf9edda9c55ca8fb218f3eca37"},//v5.1.4
    {325000,"8c8171a4c00e57468f485f62cbe777d3dfefa018b5e084fc2986cdd48eeff8bb"},
    {350000,"309e444aa88e588b59c7c9053a552fe76994d52c37648e7da4760aa26fe99301"},
    {375000,"37384327794fb7d825a3dd7186919bc6b0d64063a4919baef540923fdd6642d2"},
    {400000,"c787cfb334eb48560ceffb0402324d9a049f40f54fcd899196d002926b5d374b"},
    {425000,"28b28405a980d2e98920bcd260b027b0ff838acf2311cb1e7b0aa9ea092f53bc"},
    {450000,"d0f32e38d52d9c91ddee93883e07ff95c8d6f415b96ec1eed36b8632776726cd"},
    {475000,"8a6a4dd092ff41167d9c082a5da73323a3c7a46c8539a18018b2275500bd632c"},
    {500000,"432b0c6cfbb5967950efc2f2358f42dc85ddbc5dd270b619cd1f466970a41236"},
    {525000,"e8229c68131ef9f488af2f13995ea070853c8800e9028d4902bdbfb4bba39aeb"},
    {550000,"8f95f91fc021440339c94f7e38ed66d675ac98666abccb56a96e3aaafad7fd15"},
    {575000,"cd65475ed2e9c1fc9cbf36e87f54cf037fe492e38dcf4b20878e4360cac5cd0d"},
    {600000,"d74f649bb99983a93571b4dfa06a9f34616fc74f1e743dd4131bd0abbbc207f9"},
    {625000,"f825a8664973e941f7b5e29c976a18ea065fb9dc99a45e3f3373c38086dc7fb8"},
    {650000,"7d2db8650d40babede87c4d03a9536bb5f3eaa8e99bfca20f5e9bf9133649aa5"},
    {675000,"86150fd97564aac1bec74b427ee4810e1f81bbb2c8eaaa60de7f96d62896705c"},
    {685000,"652cabbff321880c41ac7cffc09bdab03433a7846cb6a48d42a5d098a0ffbcc8"},
    {700001,"4bcf7467df9591f392b6135dd606059e334b616f8108352c5f22eacefc4fdf40"}, //v6.0.0
    {703160,"2a98e7335910efdea4ab5a289d4ee8545c4894927170f80a216ef8e220485a64"}
};

} // namespace CryptoNote
