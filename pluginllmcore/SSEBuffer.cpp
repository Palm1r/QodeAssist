// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SSEBuffer.hpp"

namespace QodeAssist::PluginLLMCore {

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

} // namespace QodeAssist::PluginLLMCore
