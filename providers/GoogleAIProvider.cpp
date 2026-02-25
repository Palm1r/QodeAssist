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

#include "GoogleAIProvider.hpp"

#include "llmcore/GoogleAIClient.hpp"
#include "tools/ToolsFactory.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtCore/qurlquery.h>

#include "llmcore/ValidationUtils.hpp"
#include "logger/Logger.hpp"
#include "settings/ChatAssistantSettings.hpp"
#include "settings/CodeCompletionSettings.hpp"
#include "settings/QuickRefactorSettings.hpp"
#include "settings/GeneralSettings.hpp"
#include "settings/ProviderSettings.hpp"

namespace QodeAssist::Providers {

GoogleAIProvider::GoogleAIProvider(QObject *parent)
    : LLMCore::Provider(parent)
    , m_client(new LLMCore::GoogleAIClient([this]() { return apiKey(); }, this))
{
    Tools::registerAppTools(m_client->tools());
    Tools::configureToolSettings(m_client->tools());
    connectClientSignals(m_client);
}

QString GoogleAIProvider::name() const
{
    return "Google AI";
}

QString GoogleAIProvider::url() const
{
    return "https://generativelanguage.googleapis.com/v1beta";
}

QString GoogleAIProvider::completionEndpoint() const
{
    return {};
}

QString GoogleAIProvider::chatEndpoint() const
{
    return {};
}

bool GoogleAIProvider::supportsModelListing() const
{
    return true;
}

void GoogleAIProvider::prepareRequest(
    QJsonObject &request,
    LLMCore::PromptTemplate *prompt,
    LLMCore::ContextData context,
    LLMCore::RequestType type,
    bool isToolsEnabled,
    bool isThinkingEnabled)
{
    if (!prompt->isSupportProvider(providerID())) {
        LOG_MESSAGE(QString("Template %1 doesn't support %2 provider").arg(name(), prompt->name()));
    }

    prompt->prepareRequest(request, context);

    auto applyModelParams = [&request](const auto &settings) {
        QJsonObject generationConfig;
        generationConfig["maxOutputTokens"] = settings.maxTokens();
        generationConfig["temperature"] = settings.temperature();

        if (settings.useTopP())
            generationConfig["topP"] = settings.topP();
        if (settings.useTopK())
            generationConfig["topK"] = settings.topK();

        request["generationConfig"] = generationConfig;
    };

    auto applyThinkingMode = [&request](const auto &settings) {
        QJsonObject generationConfig;
        generationConfig["maxOutputTokens"] = settings.thinkingMaxTokens();

        if (settings.useTopP())
            generationConfig["topP"] = settings.topP();
        if (settings.useTopK())
            generationConfig["topK"] = settings.topK();

        generationConfig["temperature"] = 1.0;

        QJsonObject thinkingConfig;
        thinkingConfig["includeThoughts"] = true;
        int budgetTokens = settings.thinkingBudgetTokens();
        if (budgetTokens != -1) {
            thinkingConfig["thinkingBudget"] = budgetTokens;
        }

        generationConfig["thinkingConfig"] = thinkingConfig;
        request["generationConfig"] = generationConfig;
    };

    if (type == LLMCore::RequestType::CodeCompletion) {
        applyModelParams(Settings::codeCompletionSettings());
    } else if (type == LLMCore::RequestType::QuickRefactoring) {
        const auto &qrSettings = Settings::quickRefactorSettings();

        if (isThinkingEnabled) {
            applyThinkingMode(qrSettings);
        } else {
            applyModelParams(qrSettings);
        }
    } else {
        const auto &chatSettings = Settings::chatAssistantSettings();

        if (isThinkingEnabled) {
            applyThinkingMode(chatSettings);
        } else {
            applyModelParams(chatSettings);
        }
    }

    if (isToolsEnabled) {
        LLMCore::RunToolsFilter filter = LLMCore::RunToolsFilter::ALL;
        if (type == LLMCore::RequestType::QuickRefactoring) {
            filter = LLMCore::RunToolsFilter::OnlyRead;
        }

        auto toolsDefinitions = m_client->tools()->getToolsDefinitions(
            LLMCore::ToolSchemaFormat::Google, filter);
        if (!toolsDefinitions.isEmpty()) {
            request["tools"] = toolsDefinitions;
            LOG_MESSAGE(QString("Added %1 tools to Google AI request").arg(toolsDefinitions.size()));
        }
    }
}

QFuture<QList<QString>> GoogleAIProvider::getInstalledModels(const QString &url)
{

    return m_client->listModels(url);
}

QList<QString> GoogleAIProvider::validateRequest(
    const QJsonObject &request, LLMCore::TemplateType type)
{
    QJsonObject templateReq;

    templateReq = QJsonObject{
        {"contents", QJsonArray{}},
        {"system_instruction", QJsonArray{}},
        {"generationConfig",
         QJsonObject{
             {"temperature", {}},
             {"maxOutputTokens", {}},
             {"topP", {}},
             {"topK", {}},
             {"thinkingConfig",
              QJsonObject{{"thinkingBudget", {}}, {"includeThoughts", {}}}}}},
        {"safetySettings", QJsonArray{}},
        {"tools", QJsonArray{}}};

    return LLMCore::ValidationUtils::validateRequestFields(request, templateReq);
}

QString GoogleAIProvider::apiKey() const
{
    return Settings::providerSettings().googleAiApiKey();
}

LLMCore::ProviderID GoogleAIProvider::providerID() const
{
    return LLMCore::ProviderID::GoogleAI;
}

void GoogleAIProvider::sendRequest(
    const LLMCore::RequestID &requestId, const QUrl &url, const QJsonObject &payload)
{

    m_client->sendMessage(requestId, url, payload);
}

LLMCore::ProviderCapabilities GoogleAIProvider::capabilities() const
{
    return LLMCore::ToolsCapability | LLMCore::ThinkingCapability | LLMCore::ImageCapability;
}

void GoogleAIProvider::cancelRequest(const LLMCore::RequestID &requestId)
{
    m_client->cancelRequest(requestId);
}

LLMCore::ToolsManager *GoogleAIProvider::toolsManager() const
{
    return m_client->tools();
}

} // namespace QodeAssist::Providers
