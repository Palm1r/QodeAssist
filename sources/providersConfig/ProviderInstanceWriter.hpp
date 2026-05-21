// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

#include "ProviderInstance.hpp"

namespace QodeAssist::Providers {

class ProviderInstanceWriter
{
public:
    [[nodiscard]] static QString toToml(const ProviderInstance &inst);
    [[nodiscard]] static QString writeToUserDir(
        const ProviderInstance &inst,
        const QString &previousPath,
        QString *errorOut = nullptr);
    [[nodiscard]] static QString pickUserFilePath(
        const QString &userDir,
        const QString &name,
        const QString &previousPath);
    [[nodiscard]] static QString deriveBaseName(const QString &name);
};

} // namespace QodeAssist::Providers
