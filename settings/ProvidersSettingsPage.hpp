// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <memory>

#include <QObject>
#include <QString>

namespace Core { class IOptionsPage; }

namespace QodeAssist::Providers {
class ProviderInstanceFactory;
class ProviderSecretsStore;
class ProviderLauncher;
}

namespace QodeAssist::Settings {

class ProvidersPageNavigator : public QObject
{
    Q_OBJECT
public:
    explicit ProvidersPageNavigator(QObject *parent = nullptr);

    void requestSelectInstance(const QString &name);
    QString takePendingSelection();

signals:
    void selectInstanceRequested(const QString &name);

private:
    QString m_pending;
};

std::unique_ptr<Core::IOptionsPage> createProvidersSettingsPage(
    Providers::ProviderInstanceFactory *instanceFactory,
    Providers::ProviderSecretsStore *secrets,
    Providers::ProviderLauncher *launcher,
    ProvidersPageNavigator *navigator);

} // namespace QodeAssist::Settings
