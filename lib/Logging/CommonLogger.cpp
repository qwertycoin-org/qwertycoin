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

#include <Logging/CommonLogger.h>

namespace Logging {

namespace {

std::string formatPattern(const std::string &pattern,
                          const std::string &category,
                          Level level,
                          boost::posix_time::ptime time)
{
    std::stringstream s;

    for (const char *p = pattern.c_str(); p && *p != 0; ++p) {
        if (*p == '%') {
            ++p;
            switch (*p) {
            case 0:
                break;
            case 'C':
                s << category;
                break;
            case 'D':
                s << time.date();
                break;
            case 'T':
                s << time.time_of_day();
                break;
            case 'L':
                s << std::setw(7) << std::left << ILogger::LEVEL_NAMES[level];
                break;
            default:
                s << *p;
            }
        } else {
            s << *p;
        }
    }

    return s.str();
}

} // namespace

void CommonLogger::enableCategory(const std::string &category)
{
    m_disabledCategories.erase(category);
}

void CommonLogger::disableCategory(const std::string &category)
{
    m_disabledCategories.insert(category);
}

void CommonLogger::setMaxLevel(Level level)
{
    m_logLevel = level;
}

void CommonLogger::setPattern(const std::string &pattern)
{
    this->m_pattern = pattern;
}

void CommonLogger::operator()(const std::string &category,
                              Level level,
                              boost::posix_time::ptime time,
                              const std::string &body)
{
    if (level <= m_logLevel && m_disabledCategories.count(category) == 0) {
        std::string body2 = body;
        if (!m_pattern.empty()) {
            size_t insertPos = 0;
            if (!body2.empty() && body2[0] == ILogger::COLOR_DELIMETER) {
                size_t delimPos = body2.find(ILogger::COLOR_DELIMETER, 1);
                if (delimPos != std::string::npos) {
                    insertPos = delimPos + 1;
                }
            }

            body2.insert(insertPos, formatPattern(m_pattern, category, level, time));
        }

        doLogString(body2);
    }
}

CommonLogger::CommonLogger(Level level)
    : m_logLevel(level),
      m_pattern("%D %T %L [%C] ")
{
}

void CommonLogger::doLogString(const std::string &message)
{
}

} // namespace Logging
