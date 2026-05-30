// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "AgentRouter.hpp"

#include <QFileInfo>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QRegularExpression>

#include "AgentFactory.hpp"

namespace QodeAssist::AgentRouter {

namespace {

QRegularExpression compiledGlob(const QString &pattern)
{
    static QHash<QString, QRegularExpression> cache;
    static QMutex mutex;
    QMutexLocker lock(&mutex);
    const auto it = cache.constFind(pattern);
    if (it != cache.constEnd())
        return *it;
    const QRegularExpression re(
        QRegularExpression::anchoredPattern(
            QRegularExpression::wildcardToRegularExpression(
                pattern, QRegularExpression::NonPathWildcardConversion)),
        QRegularExpression::CaseInsensitiveOption);
    cache.insert(pattern, re);
    return re;
}

bool matchesAnyGlob(const QStringList &patterns, const QString &subject)
{
    if (subject.isEmpty())
        return false;
    for (const QString &pat : patterns) {
        const QRegularExpression re = compiledGlob(pat);
        if (re.isValid() && re.match(subject).hasMatch())
            return true;
    }
    return false;
}

bool matchesFilePatterns(const QStringList &patterns, const QString &filePath)
{
    if (patterns.isEmpty())
        return true;
    if (filePath.isEmpty())
        return false;
    const QString name = QFileInfo(filePath).fileName();
    return matchesAnyGlob(patterns, name) || matchesAnyGlob(patterns, filePath);
}

bool matchesPathPatterns(const QStringList &patterns, const QString &filePath)
{
    if (patterns.isEmpty())
        return true;
    if (filePath.isEmpty())
        return false;
    return matchesAnyGlob(patterns, filePath);
}

bool matchesProjectNames(const QStringList &names, const QString &projectName)
{
    if (names.isEmpty())
        return true; // dimension unconstrained
    if (projectName.isEmpty())
        return false;
    // Project names are user-facing identifiers, not paths — case
    // sensitive comparison matches what ProjectExplorer hands us.
    return names.contains(projectName);
}

} // namespace

bool matches(const AgentConfig::Match &m, const Context &ctx)
{
    if (m.isEmpty())
        return true; // explicit catch-all
    return matchesFilePatterns(m.filePatterns, ctx.filePath)
           && matchesPathPatterns(m.pathPatterns, ctx.filePath)
           && matchesProjectNames(m.projectNames, ctx.projectName);
}

QString pickAgent(
    const QStringList &roster, const Context &ctx, const AgentFactory &factory)
{
    for (const QString &name : roster) {
        const AgentConfig *cfg = factory.configByName(name);
        if (!cfg)
            continue; // stale roster entry — silently skip
        if (matches(cfg->match, ctx))
            return name;
    }
    return {};
}

} // namespace QodeAssist::AgentRouter
