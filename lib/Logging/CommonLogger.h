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

#include <set>
#include <Logging/ILogger.h>

namespace Logging {

class CommonLogger : public ILogger
{
public:
    virtual void enableCategory(const std::string &category);
    virtual void disableCategory(const std::string &category);
    virtual void setMaxLevel(Level level);

    void setPattern(const std::string &pattern);

    void operator()(const std::string &category,
                    Level level,
                    boost::posix_time::ptime time,
                    const std::string &body) override;

protected:
    explicit CommonLogger(Level level);

    virtual void doLogString(const std::string &message);

protected:
    std::set<std::string> m_disabledCategories;
    Level m_logLevel;
    std::string m_pattern;
};

} // namespace Logging
