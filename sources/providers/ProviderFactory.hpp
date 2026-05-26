// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QString>
#include <functional>

namespace QodeAssist::Providers {

class Provider;

namespace ProviderFactory {

using FactoryFn = std::function<Provider *(QObject *parent)>;

void registerType(const QString &name, FactoryFn fn);
Provider *create(const QString &name, QObject *parent);
QStringList knownNames();
void clear(); // for tests / shutdown

} // namespace ProviderFactory

} // namespace QodeAssist::Providers
