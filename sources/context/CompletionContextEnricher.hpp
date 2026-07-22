// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QString>
#include <QStringList>

#include "IDocumentReader.hpp"

namespace QodeAssist::Context {

class ICompletionEnricher
{
public:
    virtual ~ICompletionEnricher() = default;

    virtual QString enrichmentFor(
        const DocumentInfo &documentInfo,
        const QString &prefix,
        int line,
        int column,
        int maxTokens)
        = 0;
};

class SemanticContextEnricher : public ICompletionEnricher
{
public:
    QString enrichmentFor(
        const DocumentInfo &documentInfo,
        const QString &prefix,
        int line,
        int column,
        int maxTokens) override;
};

QString clampSectionsToTokenBudget(const QStringList &sections, int maxTokens);
QStringList identifiersNearCursor(const QString &prefix, int maxIdentifiers);

} // namespace QodeAssist::Context
