/* 
 * Copyright (C) 2025 Petr Mironychev
 *
 * This file is part of QodeAssist.
 *
 * QodeAssist is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * QodeAssist is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with QodeAssist. If not, see <https://www.gnu.org/licenses/>.
 */

#include "SSEBuffer.hpp"

namespace QodeAssist::LLMCore {

QStringList SSEBuffer::processData(const QByteArray &data)
{
    m_buffer += QString::fromUtf8(data);

    QStringList lines = m_buffer.split('\n');
    m_buffer = lines.takeLast();

    lines.removeAll(QString());

    return lines;
}

void SSEBuffer::clear()
{
    m_buffer.clear();
}

QString SSEBuffer::currentBuffer() const
{
    return m_buffer;
}

bool SSEBuffer::hasIncompleteData() const
{
    return !m_buffer.isEmpty();
}

} // namespace QodeAssist::LLMCore
