// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QString>

namespace QodeAssist::Templates::ContextRenderer {

struct Bindings
{
    QString projectDir;
    QString homeDir;
    // Role id selected at runtime (e.g. in the chat). Used by the no-arg
    // `{{ agent_role() }}` template callback; empty falls back to "developer".
    QString roleId;
};

QString render(const QString &templateSource, const Bindings &bindings,
               QString *error = nullptr);

} // namespace QodeAssist::Templates::ContextRenderer
