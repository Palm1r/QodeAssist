/*
 * Copyright (C) 2025 Petr Mironychev
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

#include "SimpleLLMTask.hpp"
#include "llmcore/PromptTemplateManager.hpp"
#include "llmcore/ProvidersManager.hpp"
#include "llmcore/RequestConfig.hpp"
#include "settings/ChatAssistantSettings.hpp"
#include "settings/GeneralSettings.hpp"
#include <logger/Logger.hpp>

#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QUuid>

namespace QodeAssist {

SimpleLLMTask::SimpleLLMTask(QObject *parent)
    : BaseTask(parent)
    , m_requestHandler(new LLMCore::RequestHandler(this))
    , m_promise(nullptr)
{
    addParameter("systemPrompt", QString("Hello, how are you?"));

    addInputPort("prompt");
    addOutputPort("response");
    addOutputPort("success");

    connect(
        m_requestHandler,
        &LLMCore::RequestHandler::completionReceived,
        this,
        &SimpleLLMTask::onLLMResponseReceived);
    connect(
        m_requestHandler,
        &LLMCore::RequestHandler::requestFinished,
        this,
        &SimpleLLMTask::onLLMRequestFinished);
}

SimpleLLMTask::~SimpleLLMTask()
{
    if (m_promise) {
        if (!m_promise->future().isFinished()) {
            m_promise->addResult(TaskState::Failed);
            m_promise->finish();
        }
        delete m_promise;
    }
}

TaskState SimpleLLMTask::execute()
{
    LOG_MESSAGE("SimpleLLMTask: Starting execution");

    m_accumulatedResponse.clear();

    QVariant inputPrompt = getInputValue("prompt");
    QString actualPrompt;

    if (inputPrompt.isValid() && !inputPrompt.toString().isEmpty()) {
        actualPrompt = inputPrompt.toString();
    } else {
        actualPrompt = getParameter("prompt").toString();
    }

    if (actualPrompt.isEmpty()) {
        actualPrompt = "Say hello!";
    }

    LOG_MESSAGE(QString("SimpleLLMTask: Using prompt: %1").arg(actualPrompt));

    if (m_promise) {
        delete m_promise;
    }
    m_promise = new QPromise<TaskState>();
    m_promise->start();

    if (!sendLLMRequest(actualPrompt)) {
        setOutputValue("success", false);
        setOutputValue("response", "Failed to send request");
        m_promise->addResult(TaskState::Failed);
        m_promise->finish();
    }

    QFuture<TaskState> future = m_promise->future();
    future.waitForFinished();

    TaskState result = future.result();

    delete m_promise;
    m_promise = nullptr;

    return result;
}

bool SimpleLLMTask::sendLLMRequest(const QString &actualPrompt)
{
    auto &settings = Settings::generalSettings();
    auto &chatSettings = Settings::chatAssistantSettings();

    const QString providerName = settings.caProvider();
    auto provider = LLMCore::ProvidersManager::instance().getProviderByName(providerName);
    if (!provider) {
        LOG_MESSAGE(QString("SimpleLLMTask: No provider: %1").arg(providerName));
        return false;
    }

    const QString templateName = settings.caTemplate();
    auto promptTemplate = LLMCore::PromptTemplateManager::instance().getChatTemplateByName(
        templateName);
    if (!promptTemplate) {
        LOG_MESSAGE(QString("SimpleLLMTask: No template: %1").arg(templateName));
        return false;
    }

    LLMCore::LLMConfig config;
    config.requestType = LLMCore::RequestType::Chat;
    config.provider = provider;
    config.promptTemplate = promptTemplate;
    config.url = QString("%1%2").arg(settings.caUrl(), provider->chatEndpoint());
    config.providerRequest = {{"model", settings.caModel()}, {"stream", chatSettings.stream()}};
    config.apiKey = provider->apiKey();

    LLMCore::ContextData contextData;
    contextData.systemPrompt = getParameter("systemPrompt").toString();

    QVector<LLMCore::Message> messages;
    messages.append({"user", actualPrompt});
    contextData.history = messages;

    provider->prepareRequest(
        config.providerRequest, promptTemplate, contextData, LLMCore::RequestType::Chat);

    m_currentRequestId = QUuid::createUuid().toString();
    QJsonObject request{{"id", m_currentRequestId}};

    m_requestHandler->sendLLMRequest(config, request);

    LOG_MESSAGE(QString("SimpleLLMTask: Request sent with ID: %1").arg(m_currentRequestId));
    return true;
}

void SimpleLLMTask::onLLMResponseReceived(
    const QString &response, const QJsonObject &request, bool isComplete)
{
    QString requestId = request["id"].toString();

    if (requestId != m_currentRequestId) {
        return;
    }

    m_accumulatedResponse = response;

    LOG_MESSAGE(QString("SimpleLLMTask: Got response chunk, total: %1, complete: %2")
                    .arg(m_accumulatedResponse.length())
                    .arg(isComplete));

    if (isComplete) {
        setOutputValue("success", true);
        setOutputValue("response", m_accumulatedResponse);

        LOG_MESSAGE(QString("SimpleLLMTask: Success! Response length: %1 %2")
                        .arg(m_accumulatedResponse.length())
                        .arg(m_accumulatedResponse));

        if (m_promise && !m_promise->future().isFinished()) {
            m_promise->addResult(TaskState::Success);
            m_promise->finish();
        }
    }
}

void SimpleLLMTask::onLLMRequestFinished(
    const QString &requestId, bool success, const QString &errorString)
{
    if (requestId != m_currentRequestId) {
        return;
    }

    if (!success) {
        LOG_MESSAGE(QString("SimpleLLMTask: Request failed: %1").arg(errorString));
        setOutputValue("success", false);
        setOutputValue("response", QString("Error: %1").arg(errorString));

        if (m_promise && !m_promise->future().isFinished()) {
            m_promise->addResult(TaskState::Failed);
            m_promise->finish();
        }
    }
}

} // namespace QodeAssist
