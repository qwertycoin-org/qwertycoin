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

#pragma once

#include <string>
#include <system_error>

namespace CryptoNote {

namespace error {

enum class BlockchainExplorerErrorCodes : int
{
    NOT_INITIALIZED = 1,
    ALREADY_INITIALIZED,
    INTERNAL_ERROR,
    REQUEST_ERROR
};

class BlockchainExplorerErrorCategory : public std::error_category
{
    BlockchainExplorerErrorCategory() = default;

public:
    static BlockchainExplorerErrorCategory INSTANCE;

    const char *name() const noexcept override;

    std::string message(int ev) const override;

    std::error_condition default_error_condition(int ev) const noexcept override;
};

} //namespace error

} //namespace CryptoNote

inline std::error_code make_error_code(CryptoNote::error::BlockchainExplorerErrorCodes e)
{
    using namespace CryptoNote;
    return std::error_code{static_cast<int>(e), error::BlockchainExplorerErrorCategory::INSTANCE};
}
