/* 
 * Copyright (C) 2024-2025 Petr Mironychev
 *
 * This file is part of QodeAssist.
 *
 * QodeAssist is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * QodeAssist is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with QodeAssist. If not, see <https://www.gnu.org/licenses/>.
 */

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
    QString endpointMode;
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

