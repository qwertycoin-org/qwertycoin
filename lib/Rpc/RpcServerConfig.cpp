// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The Qwertycoin developers
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

#include <Common/CommandLine.h>
#include <Rpc/RpcServerConfig.h>
#include <CryptoNoteConfig.h>
#include <android.h>

namespace CryptoNote {

namespace {

const std::string DEFAULT_RPC_IP = "127.0.0.1";
const uint16_t DEFAULT_RPC_PORT = RPC_DEFAULT_PORT;

const command_line::arg_descriptor<std::string> arg_rpc_bind_ip = {
    "rpc-bind-ip",
    "",
    DEFAULT_RPC_IP
};
const command_line::arg_descriptor<uint16_t> arg_rpc_bind_port = {
    "rpc-bind-port",
    "",
    DEFAULT_RPC_PORT
};

} // namespace

RpcServerConfig::RpcServerConfig()
    : bindIp(DEFAULT_RPC_IP),
      bindPort(DEFAULT_RPC_PORT)
{
}

void RpcServerConfig::init(const boost::program_options::variables_map &vm)
{
    bindIp = command_line::get_arg(vm, arg_rpc_bind_ip);
    bindPort = command_line::get_arg(vm, arg_rpc_bind_port);
}

void RpcServerConfig::initOptions(boost::program_options::options_description &desc)
{
    command_line::add_arg(desc, arg_rpc_bind_ip);
    command_line::add_arg(desc, arg_rpc_bind_port);
}

std::string RpcServerConfig::getBindAddress() const
{
    return bindIp + ":" + std::to_string(bindPort);
}

} // namespace CryptoNote
