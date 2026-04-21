// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QString>

#pragma once

namespace QodeAssist::PluginLLMCore {

enum RequestType { CodeCompletion, Chat, Embedding, QuickRefactoring };

using RequestID = QString;
}
