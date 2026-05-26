// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

namespace QodeAssist::Templates::ContextRenderer {

struct Bindings
{
    QString projectDir;
    QString homeDir;
};

QString render(const QString &templateSource, const Bindings &bindings,
               QString *error = nullptr);

} // namespace QodeAssist::Templates::ContextRenderer
