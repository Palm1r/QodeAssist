#include "MistralAIProvider.hpp"

#include "settings/ChatAssistantSettings.hpp"
#include "settings/CodeCompletionSettings.hpp"
#include "settings/ProviderSettings.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QtCore/qeventloop.h>

#include "llmcore/OpenAIMessage.hpp"
#include "llmcore/ValidationUtils.hpp"
#include "logger/Logger.hpp"

namespace QodeAssist::Providers {

QString MistralAIProvider::name() const
{
    return "Mistral AI";
}

QString MistralAIProvider::url() const
{
    return "https://api.mistral.ai";
}

QString MistralAIProvider::completionEndpoint() const
{
    return "/v1/fim/completions";
}

QString MistralAIProvider::chatEndpoint() const
{
    return "/v1/chat/completions";
}

bool MistralAIProvider::supportsModelListing() const
{
    return true;
}

QList<QString> MistralAIProvider::getInstalledModels(const QString &url)
{
    QList<QString> models;
    QNetworkAccessManager manager;
    QNetworkRequest request(QString("%1/v1/models").arg(url));

    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if (!apiKey().isEmpty()) {
        request.setRawHeader("Authorization", QString("Bearer %1").arg(apiKey()).toUtf8());
    }

    QNetworkReply *reply = manager.get(request);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() == QNetworkReply::NoError) {
        QByteArray responseData = reply->readAll();
        QJsonDocument jsonResponse = QJsonDocument::fromJson(responseData);
        QJsonObject jsonObject = jsonResponse.object();

        if (jsonObject.contains("data") && jsonObject["object"].toString() == "list") {
            QJsonArray modelArray = jsonObject["data"].toArray();
            for (const QJsonValue &value : modelArray) {
                QJsonObject modelObject = value.toObject();
                if (modelObject.contains("id")) {
                    QString modelId = modelObject["id"].toString();
                    models.append(modelId);
                }
            }
        }
    } else {
        LOG_MESSAGE(QString("Error fetching Mistral AI models: %1").arg(reply->errorString()));
    }

    reply->deleteLater();
    return models;
}

QList<QString> MistralAIProvider::validateRequest(
    const QJsonObject &request, LLMCore::TemplateType type)
{
    const auto fimReq = QJsonObject{
        {"model", {}},
        {"max_tokens", {}},
        {"stream", {}},
        {"temperature", {}},
        {"prompt", {}},
        {"suffix", {}}};

    const auto templateReq = QJsonObject{
        {"model", {}},
        {"messages", QJsonArray{{QJsonObject{{"role", {}}, {"content", {}}}}}},
        {"temperature", {}},
        {"max_tokens", {}},
        {"top_p", {}},
        {"frequency_penalty", {}},
        {"presence_penalty", {}},
        {"stop", QJsonArray{}},
        {"stream", {}}};

    return LLMCore::ValidationUtils::validateRequestFields(
        request, type == LLMCore::TemplateType::FIM ? fimReq : templateReq);
}

QString MistralAIProvider::apiKey() const
{
    return Settings::providerSettings().mistralAiApiKey();
}

void MistralAIProvider::prepareNetworkRequest(QNetworkRequest &networkRequest) const
{
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    if (!apiKey().isEmpty()) {
        networkRequest.setRawHeader("Authorization", QString("Bearer %1").arg(apiKey()).toUtf8());
    }
}

LLMCore::ProviderID MistralAIProvider::providerID() const
{
    return LLMCore::ProviderID::MistralAI;
}

void MistralAIProvider::sendRequest(
    const LLMCore::RequestID &requestId, const QUrl &url, const QJsonObject &payload)
{
    m_dataBuffers[requestId].clear();
    m_requestUrls[requestId] = url;

    QNetworkRequest networkRequest(url);
    prepareNetworkRequest(networkRequest);

    LLMCore::HttpRequest
        request{.networkRequest = networkRequest, .requestId = requestId, .payload = payload};

    LOG_MESSAGE(
        QString("MistralAIProvider: Sending request %1 to %2").arg(requestId, url.toString()));

    emit httpClient()->sendRequest(request);
}

void MistralAIProvider::onDataReceived(
    const QodeAssist::LLMCore::RequestID &requestId, const QByteArray &data)
{
    LLMCore::DataBuffers &buffers = m_dataBuffers[requestId];
    QStringList lines = buffers.rawStreamBuffer.processData(data);

    if (data.isEmpty()) {
        return;
    }

    bool isDone = false;
    QString tempResponse;

    for (const QString &line : lines) {
        if (line.trimmed().isEmpty()) {
            continue;
        }

        if (line == "data: [DONE]") {
            isDone = true;
            continue;
        }

        QJsonObject responseObj = parseEventLine(line);
        if (responseObj.isEmpty())
            continue;

        auto message = LLMCore::OpenAIMessage::fromJson(responseObj);
        if (message.hasError()) {
            LOG_MESSAGE("Error in MistralAI response: " + message.error);
            continue;
        }

        QString content = message.getContent();
        if (!content.isEmpty()) {
            tempResponse += content;
        }

        if (message.isDone()) {
            isDone = true;
        }
    }

    if (!tempResponse.isEmpty()) {
        buffers.responseContent += tempResponse;
        emit partialResponseReceived(requestId, tempResponse);
    }

    if (isDone) {
        emit fullResponseReceived(requestId, buffers.responseContent);
        m_dataBuffers.remove(requestId);
    }
}

void MistralAIProvider::onRequestFinished(
    const QodeAssist::LLMCore::RequestID &requestId, bool success, const QString &error)
{
    if (!success) {
        LOG_MESSAGE(QString("MistralAIProvider request %1 failed: %2").arg(requestId, error));
        emit requestFailed(requestId, error);
    } else {
        if (m_dataBuffers.contains(requestId)) {
            const LLMCore::DataBuffers &buffers = m_dataBuffers[requestId];
            if (!buffers.responseContent.isEmpty()) {
                emit fullResponseReceived(requestId, buffers.responseContent);
            }
        }
    }

    m_dataBuffers.remove(requestId);
    m_requestUrls.remove(requestId);
}

void MistralAIProvider::prepareRequest(
    QJsonObject &request,
    LLMCore::PromptTemplate *prompt,
    LLMCore::ContextData context,
    LLMCore::RequestType type)
{
    if (!prompt->isSupportProvider(providerID())) {
        LOG_MESSAGE(QString("Template %1 doesn't support %2 provider").arg(name(), prompt->name()));
    }

    prompt->prepareRequest(request, context);

    if (type == LLMCore::RequestType::Chat) {
        auto &settings = Settings::chatAssistantSettings();

        request["max_tokens"] = settings.maxTokens();
        request["temperature"] = settings.temperature();

        if (settings.useTopP())
            request["top_p"] = settings.topP();

        // request["random_seed"] = "";

        if (settings.useFrequencyPenalty())
            request["frequency_penalty"] = settings.frequencyPenalty();
        if (settings.usePresencePenalty())
            request["presence_penalty"] = settings.presencePenalty();

    } else {
        auto &settings = Settings::codeCompletionSettings();

        request["max_tokens"] = settings.maxTokens();
        request["temperature"] = settings.temperature();

        if (settings.useTopP())
            request["top_p"] = settings.topP();

        // request["random_seed"] = "";
    }
}

} // namespace QodeAssist::Providers
