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

#include "ConfigurationManager.hpp"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QUuid>

#include <coreplugin/icore.h>

#include "Logger.hpp"

namespace QodeAssist::Settings {

ConfigurationManager::ConfigurationManager(QObject *parent)
    : QObject(parent)
{}

ConfigurationManager &ConfigurationManager::instance()
{
    static ConfigurationManager instance;
    return instance;
}

QVector<AIConfiguration> ConfigurationManager::getPredefinedConfigurations(
    ConfigurationType type)
{
    QVector<AIConfiguration> presets;

    AIConfiguration claudeOpus;
    claudeOpus.id = "preset_claude_opus";
    claudeOpus.name = "Claude Opus 4.5";
    claudeOpus.provider = "Claude";
    claudeOpus.model = "claude-opus-4-5-20251101";
    claudeOpus.url = "https://api.anthropic.com";
    claudeOpus.endpointMode = "Auto";
    claudeOpus.customEndpoint = "";
    claudeOpus.templateName = "Claude";
    claudeOpus.type = type;
    claudeOpus.isPredefined = true;

    AIConfiguration claudeSonnet;
    claudeSonnet.id = "preset_claude_sonnet";
    claudeSonnet.name = "Claude Sonnet 4.5";
    claudeSonnet.provider = "Claude";
    claudeSonnet.model = "claude-sonnet-4-5-20250929";
    claudeSonnet.url = "https://api.anthropic.com";
    claudeSonnet.endpointMode = "Auto";
    claudeSonnet.customEndpoint = "";
    claudeSonnet.templateName = "Claude";
    claudeSonnet.type = type;
    claudeSonnet.isPredefined = true;

    AIConfiguration claudeHaiku;
    claudeHaiku.id = "preset_claude_haiku";
    claudeHaiku.name = "Claude Haiku 4.5";
    claudeHaiku.provider = "Claude";
    claudeHaiku.model = "claude-haiku-4-5-20251001";
    claudeHaiku.url = "https://api.anthropic.com";
    claudeHaiku.endpointMode = "Auto";
    claudeHaiku.customEndpoint = "";
    claudeHaiku.templateName = "Claude";
    claudeHaiku.type = type;
    claudeHaiku.isPredefined = true;

    AIConfiguration codestral;
    codestral.id = "preset_codestral";
    codestral.name = "Codestral";
    codestral.provider = "Codestral";
    codestral.model = "codestral-2501";
    codestral.url = "https://codestral.mistral.ai";
    codestral.endpointMode = "Auto";
    codestral.customEndpoint = "";
    codestral.templateName = type == ConfigurationType::CodeCompletion ? "Mistral AI FIM" : "Mistral AI Chat";
    codestral.type = type;
    codestral.isPredefined = true;

    AIConfiguration mistral;
    mistral.id = "preset_mistral";
    mistral.name = "Mistral";
    mistral.provider = "Mistral AI";
    mistral.model = type == ConfigurationType::CodeCompletion ? "mistral-medium-latest" : "mistral-large-latest";
    mistral.url = "https://api.mistral.ai";
    mistral.endpointMode = "Auto";
    mistral.customEndpoint = "";
    mistral.templateName = type == ConfigurationType::CodeCompletion ? "Mistral AI FIM" : "Mistral AI Chat";
    mistral.type = type;
    mistral.isPredefined = true;

    AIConfiguration geminiFlash;
    geminiFlash.id = "preset_gemini_flash";
    geminiFlash.name = "Gemini 2.5 Flash";
    geminiFlash.provider = "Google AI";
    geminiFlash.model = "gemini-2.5-flash";
    geminiFlash.url = "https://generativelanguage.googleapis.com/v1beta";
    geminiFlash.endpointMode = "Auto";
    geminiFlash.customEndpoint = "";
    geminiFlash.templateName = "Google AI";
    geminiFlash.type = type;
    geminiFlash.isPredefined = true;

    AIConfiguration gpt52codex;
    gpt52codex.id = "preset_gpt52codex";
    gpt52codex.name = "gpt-5.2-codex";
    gpt52codex.provider = "OpenAI Responses";
    gpt52codex.model = "gpt-5.2-codex";
    gpt52codex.url = "https://api.openai.com";
    gpt52codex.endpointMode = "Auto";
    gpt52codex.customEndpoint = "";
    gpt52codex.templateName = "OpenAI Responses";
    gpt52codex.type = type;
    gpt52codex.isPredefined = true;

    presets.append(claudeSonnet);
    presets.append(claudeHaiku);
    presets.append(claudeOpus);
    presets.append(gpt52codex);
    presets.append(codestral);
    presets.append(mistral);
    presets.append(geminiFlash);

    return presets;
}

QString ConfigurationManager::configurationTypeToString(ConfigurationType type) const
{
    switch (type) {
    case ConfigurationType::CodeCompletion:
        return "code_completion";
    case ConfigurationType::Chat:
        return "chat";
    case ConfigurationType::QuickRefactor:
        return "quick_refactor";
    }
    return "unknown";
}

QString ConfigurationManager::getConfigurationDirectory(ConfigurationType type) const
{
    QString path = QString("%1/qodeassist/configurations/%2")
                       .arg(Core::ICore::userResourcePath().toFSPathString(),
                            configurationTypeToString(type));
    return path;
}

bool ConfigurationManager::ensureDirectoryExists(ConfigurationType type) const
{
    QDir dir(getConfigurationDirectory(type));
    if (!dir.exists()) {
        return dir.mkpath(".");
    }
    return true;
}

bool ConfigurationManager::loadConfigurations(ConfigurationType type)
{
    QVector<AIConfiguration> *configs = nullptr;
    switch (type) {
    case ConfigurationType::CodeCompletion:
        configs = &m_ccConfigurations;
        break;
    case ConfigurationType::Chat:
        configs = &m_caConfigurations;
        break;
    case ConfigurationType::QuickRefactor:
        configs = &m_qrConfigurations;
        break;
    }

    if (!configs) {
        return false;
    }

    configs->clear();

    QVector<AIConfiguration> predefinedConfigs = getPredefinedConfigurations(type);
    configs->append(predefinedConfigs);

    if (!ensureDirectoryExists(type)) {
        LOG_MESSAGE("Failed to create configuration directory");
        return false;
    }

    QDir dir(getConfigurationDirectory(type));
    QStringList filters;
    filters << "*.json";
    QFileInfoList files = dir.entryInfoList(filters, QDir::Files);

    for (const QFileInfo &fileInfo : files) {
        QFile file(fileInfo.absoluteFilePath());
        if (!file.open(QIODevice::ReadOnly)) {
            LOG_MESSAGE(QString("Failed to open configuration file: %1").arg(fileInfo.fileName()));
            continue;
        }

        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        file.close();

        if (!doc.isObject()) {
            LOG_MESSAGE(QString("Invalid configuration file: %1").arg(fileInfo.fileName()));
            continue;
        }

        QJsonObject obj = doc.object();
        AIConfiguration config;
        config.id = obj["id"].toString();
        config.name = obj["name"].toString();
        config.provider = obj["provider"].toString();
        config.model = obj["model"].toString();
        config.templateName = obj["template"].toString();
        config.url = obj["url"].toString();
        config.endpointMode = obj["endpointMode"].toString();
        config.customEndpoint = obj["customEndpoint"].toString();
        config.type = type;
        config.formatVersion = obj.value("formatVersion").toInt(1);
        config.isPredefined = false;

        if (config.id.isEmpty() || config.name.isEmpty()) {
            LOG_MESSAGE(QString("Invalid configuration data in file: %1").arg(fileInfo.fileName()));
            continue;
        }

        configs->append(config);
    }

    emit configurationsChanged(type);
    return true;
}

bool ConfigurationManager::saveConfiguration(const AIConfiguration &config)
{
    if (!ensureDirectoryExists(config.type)) {
        LOG_MESSAGE("Failed to create configuration directory");
        return false;
    }

    QJsonObject obj;
    obj["formatVersion"] = config.formatVersion;
    obj["id"] = config.id;
    obj["name"] = config.name;
    obj["provider"] = config.provider;
    obj["model"] = config.model;
    obj["template"] = config.templateName;
    obj["url"] = config.url;
    obj["endpointMode"] = config.endpointMode;
    obj["customEndpoint"] = config.customEndpoint;

    QString sanitizedName = config.name;
    sanitizedName.replace(" ", "_");
    sanitizedName.replace(QRegularExpression("[^a-zA-Z0-9_-]"), "");

    QString fileName = QString("%1/%2_%3.json")
                           .arg(getConfigurationDirectory(config.type), sanitizedName, config.id);

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) {
        LOG_MESSAGE(QString("Failed to create configuration file: %1").arg(fileName));
        return false;
    }

    QJsonDocument doc(obj);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    loadConfigurations(config.type);
    return true;
}

bool ConfigurationManager::deleteConfiguration(const QString &id, ConfigurationType type)
{
    AIConfiguration config = getConfigurationById(id, type);
    if (config.isPredefined) {
        LOG_MESSAGE(QString("Cannot delete predefined configuration: %1").arg(id));
        return false;
    }

    QDir dir(getConfigurationDirectory(type));
    QStringList filters;
    filters << QString("*_%1.json").arg(id);
    QFileInfoList files = dir.entryInfoList(filters, QDir::Files);

    if (files.isEmpty()) {
        LOG_MESSAGE(QString("Configuration file not found for id: %1").arg(id));
        return false;
    }

    for (const QFileInfo &fileInfo : files) {
        QFile file(fileInfo.absoluteFilePath());
        if (!file.remove()) {
            LOG_MESSAGE(QString("Failed to delete configuration file: %1")
                            .arg(fileInfo.absoluteFilePath()));
            return false;
        }
    }

    loadConfigurations(type);
    return true;
}

QVector<AIConfiguration> ConfigurationManager::configurations(ConfigurationType type) const
{
    switch (type) {
    case ConfigurationType::CodeCompletion:
        return m_ccConfigurations;
    case ConfigurationType::Chat:
        return m_caConfigurations;
    case ConfigurationType::QuickRefactor:
        return m_qrConfigurations;
    }
    return {};
}

AIConfiguration ConfigurationManager::getConfigurationById(const QString &id,
                                                           ConfigurationType type) const
{
    const QVector<AIConfiguration> &configs = configurations(type);
    for (const AIConfiguration &config : configs) {
        if (config.id == id) {
            return config;
        }
    }
    return AIConfiguration();
}

} // namespace QodeAssist::Settings

