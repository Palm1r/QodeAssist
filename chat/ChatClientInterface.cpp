/* 
 * Copyright (C) 2024 Petr Mironychev
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

#include "ChatClientInterface.hpp"
#include "LLMProvidersManager.hpp"
#include "PromptTemplateManager.hpp"
#include "QodeAssistUtils.hpp"
#include "settings/ContextSettings.hpp"
#include "settings/GeneralSettings.hpp"
#include "settings/PresetPromptsSettings.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QUuid>

namespace QodeAssist::Chat {

ChatClientInterface::ChatClientInterface(QObject *parent)
    : QObject(parent)
    , m_requestHandler(new LLMRequestHandler(this))
{
    connect(m_requestHandler,
            &LLMRequestHandler::completionReceived,
            this,
            [this](const QString &completion, const QJsonObject &, bool isComplete) {
                handleLLMResponse(completion, isComplete);
            });

    connect(m_requestHandler,
            &LLMRequestHandler::requestFinished,
            this,
            [this](const QString &, bool success, const QString &errorString) {
                if (!success) {
                    emit errorOccurred(errorString);
                }
            });

    // QJsonObject systemMessage;
    // systemMessage["role"] = "system";
    // systemMessage["content"] = "You are a helpful C++ and QML programming assistant.";
    // m_chatHistory.append(systemMessage);
}

ChatClientInterface::~ChatClientInterface()
{
}

void ChatClientInterface::sendMessage(const QString &message)
{
    logMessage("Sending message: " + message);
    logMessage("chatProvider " + Settings::generalSettings().chatLlmProviders.stringValue());
    logMessage("chatTemplate " + Settings::generalSettings().chatPrompts.stringValue());

    auto chatTemplate = PromptTemplateManager::instance().getCurrentChatTemplate();
    auto chatProvider = LLMProvidersManager::instance().getCurrentChatProvider();

    ContextData context;
    context.prefix = message;
    context.suffix = "";
    if (Settings::contextSettings().useSpecificInstructions())
        context.instriuctions = Settings::contextSettings().specificInstractions();

    QJsonObject providerRequest;
    providerRequest["model"] = Settings::generalSettings().chatModelName();
    providerRequest["stream"] = true;

    providerRequest["messages"] = m_chatHistory;

    chatTemplate->prepareRequest(providerRequest, context);
    chatProvider->prepareRequest(providerRequest);

    m_chatHistory = providerRequest["messages"].toArray();

    LLMConfig config;
    config.requestType = RequestType::Chat;
    config.provider = chatProvider;
    config.promptTemplate = chatTemplate;
    config.url = QString("%1%2").arg(Settings::generalSettings().chatUrl(),
                                     Settings::generalSettings().chatEndPoint());
    config.providerRequest = providerRequest;

    QJsonObject request;
    request["id"] = QUuid::createUuid().toString();

    m_accumulatedResponse.clear();
    m_pendingMessage = message;
    m_requestHandler->sendLLMRequest(config, request);
}

void ChatClientInterface::clearMessages()
{
    m_chatHistory = {};
    m_accumulatedResponse.clear();
    m_pendingMessage.clear();
    logMessage("Chat history cleared");
}

void ChatClientInterface::handleLLMResponse(const QString &response, bool isComplete)
{
    m_accumulatedResponse += response;
    logMessage("Accumulated response: " + m_accumulatedResponse);

    if (isComplete) {
        logMessage("Message completed. Final response: " + m_accumulatedResponse);
        emit messageReceived(m_accumulatedResponse.trimmed());

        QJsonObject assistantMessage;
        assistantMessage["role"] = "assistant";
        assistantMessage["content"] = m_accumulatedResponse.trimmed();
        m_chatHistory.append(assistantMessage);

        m_pendingMessage.clear();
        m_accumulatedResponse.clear();

        trimChatHistory();
    }
}

void ChatClientInterface::trimChatHistory()
{
    int maxTokens = 4000;
    int totalTokens = 0;
    QJsonArray newHistory;

    if (!m_chatHistory.isEmpty()
        && m_chatHistory.first().toObject()["role"].toString() == "system") {
        newHistory.append(m_chatHistory.first());
    }

    for (int i = m_chatHistory.size() - 1; i >= 0; --i) {
        QJsonObject message = m_chatHistory[i].toObject();
        int messageTokens = message["content"].toString().length() / 4;

        if (totalTokens + messageTokens > maxTokens) {
            break;
        }

        newHistory.prepend(message);
        totalTokens += messageTokens;
    }

    m_chatHistory = newHistory;
}

} // namespace QodeAssist::Chat
