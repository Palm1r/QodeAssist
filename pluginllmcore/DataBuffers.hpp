// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "SSEBuffer.hpp"
#include <QString>

namespace QodeAssist::PluginLLMCore {

struct DataBuffers
{
    SSEBuffer rawStreamBuffer;
    QString responseContent;

    void clear()
    {
        rawStreamBuffer.clear();
        responseContent.clear();
    }
};

} // namespace QodeAssist::PluginLLMCore
