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

#include <algorithm>
#include <Logging/LoggerGroup.h>

namespace Logging {

LoggerGroup::LoggerGroup(Level level)
    : CommonLogger(level)
{
}

void LoggerGroup::addLogger(ILogger &logger)
{
    m_loggers.push_back(&logger);
}

void LoggerGroup::removeLogger(ILogger &logger)
{
    m_loggers.erase(std::remove(m_loggers.begin(), m_loggers.end(), &logger), m_loggers.end());
}

void LoggerGroup::operator()(const std::string &category,
                             Level level,
                             boost::posix_time::ptime time,
                             const std::string &body)
{
    if (level <= m_logLevel && m_disabledCategories.count(category) == 0) {
        for (auto &logger : m_loggers) {
            (*logger)(category, level, time, body);
        }
    }
}

} // namespace Logging
