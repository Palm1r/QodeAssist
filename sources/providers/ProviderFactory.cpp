// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "ProviderFactory.hpp"

#include <QHash>

namespace QodeAssist::Providers::ProviderFactory {

namespace {

QHash<QString, FactoryFn> &table()
{
    static QHash<QString, FactoryFn> t;
    return t;
}

} // namespace

void registerType(const QString &name, FactoryFn fn)
{
    if (name.isEmpty() || !fn) return;
    table().insert(name, std::move(fn));
}

Provider *create(const QString &name, QObject *parent)
{
    auto it = table().constFind(name);
    if (it == table().constEnd()) return nullptr;
    return it.value()(parent);
}

QStringList knownNames()
{
    return table().keys();
}

void clear()
{
    table().clear();
}

} // namespace QodeAssist::Providers::ProviderFactory
