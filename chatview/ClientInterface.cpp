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

#include "ClientInterface.hpp"
#include "ContextSettings.hpp"
#include "GeneralSettings.hpp"
#include "Logger.hpp"
#include "PromptTemplateManager.hpp"
#include "ProvidersManager.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QUuid>

namespace QodeAssist::Chat {

ClientInterface::ClientInterface(ChatModel *chatModel, QObject *parent)
    : QObject(parent)
    , m_requestHandler(new LLMCore::RequestHandler(this))
    , m_chatModel(chatModel)
{
    connect(m_requestHandler,
            &LLMCore::RequestHandler::completionReceived,
            this,
            [this](const QString &completion, const QJsonObject &request, bool isComplete) {
                handleLLMResponse(completion, request, isComplete);
            });

    connect(m_requestHandler,
            &LLMCore::RequestHandler::requestFinished,
            this,
            [this](const QString &, bool success, const QString &errorString) {
                if (!success) {
                    emit errorOccurred(errorString);
                }
            });
}

ClientInterface::~ClientInterface() = default;

void ClientInterface::sendMessage(const QString &message)
{
    LOG_MESSAGE("Sending message: " + message);
    LOG_MESSAGE("chatProvider " + Settings::generalSettings().chatLlmProviders.stringValue());
    LOG_MESSAGE("chatTemplate " + Settings::generalSettings().chatPrompts.stringValue());

    auto chatTemplate = LLMCore::PromptTemplateManager::instance().getCurrentChatTemplate();
    auto chatProvider = LLMCore::ProvidersManager::instance().getCurrentChatProvider();

    LLMCore::ContextData context;
    context.prefix = message;
    context.suffix = "";
    if (Settings::contextSettings().useChatSystemPrompt())
        context.systemPrompt = Settings::contextSettings().chatSystemPrompt();

    QJsonObject providerRequest;
    providerRequest["model"] = Settings::generalSettings().chatModelName();
    providerRequest["stream"] = true;
    providerRequest["messages"] = m_chatModel->prepareMessagesForRequest(context);

    chatTemplate->prepareRequest(providerRequest, context);
    chatProvider->prepareRequest(providerRequest, LLMCore::RequestType::Chat);

    LLMCore::LLMConfig config;
    config.requestType = LLMCore::RequestType::Chat;
    config.provider = chatProvider;
    config.promptTemplate = chatTemplate;
    config.url = QString("%1%2").arg(Settings::generalSettings().chatUrl(),
                                     Settings::generalSettings().chatEndPoint());
    config.providerRequest = providerRequest;
    config.multiLineCompletion = Settings::generalSettings().multiLineCompletion();

    QJsonObject request;
    request["id"] = QUuid::createUuid().toString();

    m_accumulatedResponse.clear();
    m_chatModel->addMessage(message, ChatModel::ChatRole::User, "");
    m_requestHandler->sendLLMRequest(config, request);
}

void ClientInterface::clearMessages()
{
    m_chatModel->clear();
    m_accumulatedResponse.clear();
    LOG_MESSAGE("Chat history cleared");
}

void ClientInterface::handleLLMResponse(const QString &response,
                                        const QJsonObject &request,
                                        bool isComplete)
{
    QString messageId = request["id"].toString();
    m_chatModel->addMessage(response.trimmed(), ChatModel::ChatRole::Assistant, messageId);

    if (isComplete) {
        LOG_MESSAGE("Message completed. Final response for message " + messageId + ": " + response);
    }
}

} // namespace QodeAssist::Chat
