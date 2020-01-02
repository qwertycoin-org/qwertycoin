// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
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

#include <BlockchainExplorer/BlockchainExplorerErrors.h>

namespace CryptoNote {

namespace error {

BlockchainExplorerErrorCategory BlockchainExplorerErrorCategory::INSTANCE;

const char *BlockchainExplorerErrorCategory::name() const noexcept
{
    return "BlockchainExplorerErrorCategory";
}

std::string BlockchainExplorerErrorCategory::message(int ev) const
{
    switch (ev) {
    case static_cast<int>(BlockchainExplorerErrorCodes::NOT_INITIALIZED):
        return "Object was not initialized";
    case static_cast<int>(BlockchainExplorerErrorCodes::ALREADY_INITIALIZED):
        return "Object has been already initialized";
    case static_cast<int>(BlockchainExplorerErrorCodes::INTERNAL_ERROR):
        return "Internal error";
    case static_cast<int>(BlockchainExplorerErrorCodes::REQUEST_ERROR):
        return "Error in request parameters";
    default:
        return "Unknown error";
    }
}

std::error_condition BlockchainExplorerErrorCategory::default_error_condition(int ev) const noexcept
{
    return std::error_condition{ev, *this};
}

} //namespace error

} //namespace CryptoNote
