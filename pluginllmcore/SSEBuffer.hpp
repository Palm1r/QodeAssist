// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>
#include <QStringList>

namespace QodeAssist::PluginLLMCore {

class SSEBuffer
{
public:
    SSEBuffer() = default;

    QStringList processData(const QByteArray &data);

    void clear();
    QString currentBuffer() const;
    bool hasIncompleteData() const;

private:
    QString m_buffer;
};

} // namespace QodeAssist::PluginLLMCore
