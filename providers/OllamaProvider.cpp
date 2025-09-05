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

#include "OllamaProvider.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QtCore/qeventloop.h>

#include "llmcore/OllamaMessage.hpp"
#include "llmcore/ValidationUtils.hpp"
#include "logger/Logger.hpp"
#include "settings/ChatAssistantSettings.hpp"
#include "settings/CodeCompletionSettings.hpp"
#include "settings/ProviderSettings.hpp"

namespace QodeAssist::Providers {

OllamaProvider::OllamaProvider(QObject *parent)
    : LLMCore::Provider(parent)
    , m_toolsFactory(std::make_unique<Tools::ToolsFactory>())
    , m_toolsManager(std::make_unique<OllamaToolsManager>(this))
{
    m_toolsManager->setToolsFactory(m_toolsFactory.get());

    connect(
        m_toolsManager.get(),
        &OllamaToolsManager::requestReadyForContinuation,
        this,
        &OllamaProvider::onToolsRequestReadyForContinuation);
}

QString OllamaProvider::name() const
{
    return "Ollama";
}

QString OllamaProvider::url() const
{
    return "http://localhost:11434";
}

QString OllamaProvider::completionEndpoint() const
{
    return "/api/generate";
}

QString OllamaProvider::chatEndpoint() const
{
    return "/api/chat";
}

bool OllamaProvider::supportsModelListing() const
{
    return true;
}

bool OllamaProvider::supportsTools() const
{
    // Check if tools factory is available and we can detect model support
    return m_toolsManager->hasToolsSupport();
}

LLMCore::IToolsFactory *OllamaProvider::toolsFactory() const
{
    return m_toolsFactory.get();
}

void OllamaProvider::prepareRequest(
    QJsonObject &request,
    LLMCore::PromptTemplate *prompt,
    LLMCore::ContextData context,
    LLMCore::RequestType type)
{
    if (!prompt->isSupportProvider(providerID())) {
        LOG_MESSAGE(QString("Template %1 doesn't support %2 provider").arg(name(), prompt->name()));
    }

    prompt->prepareRequest(request, context);

    auto applySettings = [&request](const auto &settings) {
        QJsonObject options;
        options["num_predict"] = settings.maxTokens();
        options["temperature"] = settings.temperature();
        options["stop"] = request.take("stop");

        if (settings.useTopP())
            options["top_p"] = settings.topP();
        if (settings.useTopK())
            options["top_k"] = settings.topK();
        if (settings.useFrequencyPenalty())
            options["frequency_penalty"] = settings.frequencyPenalty();
        if (settings.usePresencePenalty())
            options["presence_penalty"] = settings.presencePenalty();

        request["options"] = options;
        request["keep_alive"] = settings.ollamaLivetime();
        request["stream"] = true;
    };

    if (type == LLMCore::RequestType::CodeCompletion) {
        applySettings(Settings::codeCompletionSettings());
    } else {
        applySettings(Settings::chatAssistantSettings());
    }

    // Add tools for chat requests if model supports them
    if (supportsTools() && type == LLMCore::RequestType::Chat) {
        QString modelName = request["model"].toString();

        if (isModelSupportTools(modelName)) {
            auto toolsDefinitions = m_toolsManager->getToolsDefinitions();
            if (!toolsDefinitions.isEmpty()) {
                request["tools"] = toolsDefinitions;
                LOG_MESSAGE(
                    QString("Added %1 tools to Ollama request").arg(toolsDefinitions.size()));
            }
        } else {
            LOG_MESSAGE(
                QString("Model %1 doesn't support function calling, skipping tools").arg(modelName));
        }
    }
}

void OllamaProvider::sendRequest(
    const QString &requestId, const QUrl &url, const QJsonObject &payload)
{
    m_accumulatedResponses[requestId].clear();
    m_requestUrls[requestId] = url;

    // Initialize tools manager ONLY if tools are present in payload
    if (payload.contains("tools") && !payload["tools"].toArray().isEmpty()) {
        m_toolsManager->initializeRequest(requestId, payload);
        LOG_MESSAGE(QString("Ollama: Initialized tools manager for request %1").arg(requestId));
    } else {
        LOG_MESSAGE(
            QString("Ollama: No tools in request %1, skipping tools manager").arg(requestId));
    }

    QNetworkRequest networkRequest(url);
    prepareNetworkRequest(networkRequest);

    LLMCore::HttpRequest
        request{.networkRequest = networkRequest, .requestId = requestId, .payload = payload};

    LOG_MESSAGE(QString("OllamaProvider: Sending request %1 to %2").arg(requestId, url.toString()));

    emit httpClient()->sendRequest(request);
}

void OllamaProvider::onDataReceived(const QString &requestId, const QByteArray &data)
{
    QString &accumulatedResponse = m_accumulatedResponses[requestId];

    if (data.isEmpty()) {
        return;
    }

    // Debug: log raw data
    LOG_MESSAGE(QString("Ollama raw data received: %1").arg(QString::fromUtf8(data)));

    QByteArrayList lines = data.split('\n');
    bool isDone = false;

    for (const QByteArray &line : lines) {
        if (line.trimmed().isEmpty()) {
            continue;
        }

        LOG_MESSAGE(QString("Processing Ollama line: %1").arg(QString::fromUtf8(line)));

        QJsonObject obj = parseOllamaLine(line);
        if (obj.isEmpty()) {
            LOG_MESSAGE("Failed to parse JSON from line");
            continue;
        }

        // Debug logging for Ollama responses
        LOG_MESSAGE(QString("Ollama response chunk: %1")
                        .arg(QJsonDocument(obj).toJson(QJsonDocument::Compact)));

        if (obj.contains("error") && !obj["error"].toString().isEmpty()) {
            LOG_MESSAGE("Error in Ollama response: " + obj["error"].toString());
            continue;
        }

        QString content;

        // Process through tools manager first (handles both text and tool calls)
        QString toolText = m_toolsManager->processEvent(requestId, obj);
        if (!toolText.isEmpty()) {
            content = toolText;
        } else {
            // Fallback to direct content extraction if tools manager didn't handle it
            if (obj.contains("response")) {
                // Generate API format
                content = obj["response"].toString();
            } else if (obj.contains("message")) {
                // Chat API format
                QJsonObject messageObj = obj["message"].toObject();
                content = messageObj["content"].toString();
            }
        }

        if (!content.isEmpty()) {
            accumulatedResponse += content;
            emit partialResponseReceived(requestId, content);
        }

        if (obj["done"].toBool()) {
            isDone = true;
        }
    }

    if (isDone) {
        // Check if there are active tools being processed
        if (!m_toolsManager->hasActiveTools(requestId)) {
            emit fullResponseReceived(requestId, accumulatedResponse);
            m_accumulatedResponses.remove(requestId);
        }
    }
}

void OllamaProvider::onRequestFinished(const QString &requestId, bool success, const QString &error)
{
    if (!success) {
        LOG_MESSAGE(QString("OllamaProvider request %1 failed: %2").arg(requestId, error));
        emit requestFailed(requestId, error);
    } else {
        LOG_MESSAGE(QString("Ollama request %1 completed successfully").arg(requestId));

        // ВАЖНО: всегда отправляем финальный ответ если есть контент
        if (m_accumulatedResponses.contains(requestId)) {
            const QString fullResponse = m_accumulatedResponses[requestId];
            if (!fullResponse.isEmpty()) {
                LOG_MESSAGE(QString("Sending final response: %1").arg(fullResponse));
                emit fullResponseReceived(requestId, fullResponse);
            } else {
                LOG_MESSAGE("No accumulated response content to send");
            }
        } else {
            LOG_MESSAGE("No accumulated response found for request");
        }
    }

    cleanupRequest(requestId);
}

void OllamaProvider::onToolsRequestReadyForContinuation(
    const QString &requestId, const QJsonObject &followUpRequest)
{
    LOG_MESSAGE(QString("Ollama: Sending continuation request for %1").arg(requestId));
    sendRequest(requestId, m_requestUrls[requestId], followUpRequest);
}

bool OllamaProvider::isModelSupportTools(const QString &modelName) const
{
    // List of known Ollama models that definitely support function calling
    static const QStringList definitelySupported
        = {"hermes3",
           "hermes-3",
           "qwen2.5",
           "qwen3",
           "mistral-nemo",
           "mixtral:8x7b",
           "mixtral:8x22b",
           "yi-coder",
           "granite3",
           "nemotron"};

    QString lowerModelName = modelName.toLower();

    // Check definite support first
    for (const QString &supportedModel : definitelySupported) {
        if (lowerModelName.contains(supportedModel)) {
            LOG_MESSAGE(QString("Model %1 confirmed to support function calling").arg(modelName));
            return true;
        }
    }

    // For testing, log when we're unsure about support
    LOG_MESSAGE(
        QString("Model %1 function calling support unknown - skipping tools").arg(modelName));
    return false;
}

QJsonObject OllamaProvider::parseOllamaLine(const QByteArray &line) const
{
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(line, &error);

    if (doc.isNull()) {
        if (error.error != QJsonParseError::NoError) {
            LOG_MESSAGE(QString("Ollama JSON parse error: %1").arg(error.errorString()));
        }
        return QJsonObject();
    }

    return doc.object();
}

void OllamaProvider::cleanupRequest(const QString &requestId)
{
    m_accumulatedResponses.remove(requestId);
    m_requestUrls.remove(requestId);
    m_toolsManager->cleanupRequest(requestId);
}

QList<QString> OllamaProvider::getInstalledModels(const QString &url)
{
    QList<QString> models;
    QNetworkAccessManager manager;
    QNetworkRequest request(QString("%1%2").arg(url, "/api/tags"));
    prepareNetworkRequest(request);
    QNetworkReply *reply = manager.get(request);

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() == QNetworkReply::NoError) {
        QByteArray responseData = reply->readAll();
        QJsonDocument jsonResponse = QJsonDocument::fromJson(responseData);
        QJsonObject jsonObject = jsonResponse.object();
        QJsonArray modelArray = jsonObject["models"].toArray();

        for (const QJsonValue &value : modelArray) {
            QJsonObject modelObject = value.toObject();
            QString modelName = modelObject["name"].toString();
            models.append(modelName);
        }
    } else {
        LOG_MESSAGE(QString("Error fetching Ollama models: %1").arg(reply->errorString()));
    }

    reply->deleteLater();
    return models;
}

QList<QString> OllamaProvider::validateRequest(const QJsonObject &request, LLMCore::TemplateType type)
{
    const auto fimReq = QJsonObject{
        {"keep_alive", {}},
        {"model", {}},
        {"stream", {}},
        {"prompt", {}},
        {"suffix", {}},
        {"system", {}},
        {"options",
         QJsonObject{
             {"temperature", {}},
             {"stop", {}},
             {"top_p", {}},
             {"top_k", {}},
             {"num_predict", {}},
             {"frequency_penalty", {}},
             {"presence_penalty", {}}}}};

    const auto messageReq = QJsonObject{
        {"keep_alive", {}},
        {"model", {}},
        {"stream", {}},
        {"messages", QJsonArray{{QJsonObject{{"role", {}}, {"content", {}}}}}},
        {"tools", QJsonArray{}},
        {"options",
         QJsonObject{
             {"temperature", {}},
             {"stop", {}},
             {"top_p", {}},
             {"top_k", {}},
             {"num_predict", {}},
             {"frequency_penalty", {}},
             {"presence_penalty", {}}}}};

    return LLMCore::ValidationUtils::validateRequestFields(
        request, type == LLMCore::TemplateType::FIM ? fimReq : messageReq);
}

QString OllamaProvider::apiKey() const
{
    return {};
}

void OllamaProvider::prepareNetworkRequest(QNetworkRequest &networkRequest) const
{
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    const auto key = Settings::providerSettings().ollamaBasicAuthApiKey();
    if (!key.isEmpty()) {
        networkRequest.setRawHeader("Authorization", "Basic " + key.toLatin1());
    }
}

LLMCore::ProviderID OllamaProvider::providerID() const
{
    return LLMCore::ProviderID::Ollama;
}

} // namespace QodeAssist::Providers
