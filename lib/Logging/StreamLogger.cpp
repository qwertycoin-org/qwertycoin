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

#include <iostream>
#include <sstream>
#include <Logging/StreamLogger.h>

namespace Logging {

StreamLogger::StreamLogger(Level level)
    : CommonLogger(level),
      m_stream(nullptr)
{
}

StreamLogger::StreamLogger(std::ostream &stream, Level level)
    : CommonLogger(level),
      m_stream(&stream)
{
}

void StreamLogger::attachToStream(std::ostream &stream)
{
    this->m_stream = &stream;
}

void StreamLogger::doLogString(const std::string &message)
{
    if (m_stream != nullptr && m_stream->good()) {
        std::lock_guard<std::mutex> lock(m_mutex);
        bool readingText = true;
        for(size_t charPos = 0; charPos < message.size(); ++charPos) {
            if (message[charPos] == ILogger::COLOR_DELIMETER) {
                readingText = !readingText;
            } else if (readingText) {
                *m_stream << message[charPos];
            }
        }

        *m_stream << std::flush;
    }
}

} // namespace Logging
