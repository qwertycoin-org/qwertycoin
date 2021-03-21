// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018, The TurtleCoin Developers
// Copyright (c) 2018-2021, The Qwertycoin Group.
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
    { 40000,"60a9694b18cf470bcfd9f36f32ad01f86b5538fe1f88a3bd2717ca6ad1c7ce80"},//v2.0.0
    { 46000,"51e2f9e09278cb66b08efcecfdf3208053ee53030c6fc5a6a647565a6b318cd5"},//v3.0.0
    {110520,"a2d08d171265aa0c4ee9e5e5baebd191940f86ccc7b01cd8ebfa2276db98736c"},//v4.0.0
    {125000,"5d551f52c0d36ed11fd1d57261e5be373a17e2fb5b67b9af54e6c3b3b192a1b8"},
    {173136,"36a9ab5f39cf4a1d612c4e10c1a72b7d147ab88f92e511af84265b7d04804dc0"},//v4.0.2
    {250721,"a119b644ec0a35c77f8cf7ed6a0ff4d37b7a279ac8fadfb660e83cc795ddb7de"},//v5.0.0
    {275213,"ba51659f63ef31685c0b9e816d9c3b9455fe20cc34aa8ef09a186153e83547a9"},//v5.1.3
    {308323,"12253186704f9898f390fcfe2550ee98f2331dbf9edda9c55ca8fb218f3eca37"},//v5.1.4
    {700001,"4bcf7467df9591f392b6135dd606059e334b616f8108352c5f22eacefc4fdf40"},//v6.0.0
    {750000,"adace1ecbaab8c8f8b86f41c93aa7fcceffe7b80bcee5346befc534e610ba9b9"},
    {800000,"2221d6b752c6fa1ef5fbfaf46fb1571de34b97c240cd110357ac8bf92154e583"}
};

} // namespace CryptoNote
