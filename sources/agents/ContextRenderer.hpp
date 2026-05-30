// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QString>

namespace QodeAssist::Templates::ContextRenderer {

struct Bindings
{
    QString projectDir;
    QString configDir;
    QString language;
};

QString render(const QString &templateSource, const Bindings &bindings, QString *error = nullptr);

} // namespace QodeAssist::Templates::ContextRenderer
