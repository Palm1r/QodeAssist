// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <optional>
#include <vector>

#include <QString>
#include <QStringList>

#include "ProviderInstance.hpp"

namespace QodeAssist::Providers {

class ProviderInstanceLoader
{
public:
    struct LoadResult
    {
        std::vector<ProviderInstance> instances;
        QStringList errors;
        QStringList warnings;
    };

    static LoadResult load(const QString &qrcPrefix, const QString &userDir);

    static std::optional<ProviderInstance> parseFile(
        const QString &path, QString *error);
};

} // namespace QodeAssist::Providers
