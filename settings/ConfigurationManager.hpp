// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QString>
#include <QVector>

namespace QodeAssist::Settings {

enum class ConfigurationType { CodeCompletion, Chat, QuickRefactor };

inline constexpr int CONFIGURATION_FORMAT_VERSION = 1;

struct AIConfiguration
{
    QString id;
    QString name;
    QString provider;
    QString model;
    QString templateName;
    QString url;
    // Empty = use the template's endpoint; non-empty = override path.
    QString customEndpoint;
    ConfigurationType type;
    int formatVersion = CONFIGURATION_FORMAT_VERSION;
    bool isPredefined = false;
};

class ConfigurationManager : public QObject
{
    Q_OBJECT

public:
    static ConfigurationManager &instance();

    bool loadConfigurations(ConfigurationType type);
    bool saveConfiguration(const AIConfiguration &config);
    bool deleteConfiguration(const QString &id, ConfigurationType type);

    QVector<AIConfiguration> configurations(ConfigurationType type) const;
    AIConfiguration getConfigurationById(const QString &id, ConfigurationType type) const;

    QString getConfigurationDirectory(ConfigurationType type) const;
    
    static QVector<AIConfiguration> getPredefinedConfigurations(ConfigurationType type);

signals:
    void configurationsChanged(ConfigurationType type);

private:
    explicit ConfigurationManager(QObject *parent = nullptr);
    ~ConfigurationManager() override = default;

    bool ensureDirectoryExists(ConfigurationType type) const;
    QString configurationTypeToString(ConfigurationType type) const;

    QVector<AIConfiguration> m_ccConfigurations;
    QVector<AIConfiguration> m_caConfigurations;
    QVector<AIConfiguration> m_qrConfigurations;
};

} // namespace QodeAssist::Settings

