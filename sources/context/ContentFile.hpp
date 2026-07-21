// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QString>

namespace QodeAssist::Context {

struct ContentFile
{
    QString filename;
    QString content;
    QString path;
};

} // namespace QodeAssist::Context
