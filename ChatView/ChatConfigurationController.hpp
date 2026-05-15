// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QStringList>

namespace QodeAssist::Chat {

class ChatConfigurationController : public QObject
{
    Q_OBJECT

public:
    explicit ChatConfigurationController(QObject *parent = nullptr);

    QStringList availableConfigurations() const;
    QString currentConfiguration() const;

    void loadAvailableConfigurations();
    void applyConfiguration(const QString &configName);

signals:
    void availableConfigurationsChanged();
    void currentConfigurationChanged();

private:
    void updateCurrentConfiguration();

    QStringList m_availableConfigurations;
    QString m_currentConfiguration;
};

} // namespace QodeAssist::Chat
