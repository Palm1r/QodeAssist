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

#include <utils/environment.h>
#include <QNetworkRequest>
#include <QObject>
#include <QString>

#include "ContextData.hpp"
#include "DataBuffers.hpp"
#include "HttpClient.hpp"
#include "IToolsManager.hpp"
#include "PromptTemplate.hpp"
#include "RequestType.hpp"

class QNetworkReply;
class QJsonObject;

namespace QodeAssist::LLMCore {

class Provider : public QObject
{
    Q_OBJECT
public:
    explicit Provider(QObject *parent = nullptr);

    virtual ~Provider() = default;

    virtual QString name() const = 0;
    virtual QString url() const = 0;
    virtual QString completionEndpoint() const = 0;
    virtual QString chatEndpoint() const = 0;
    virtual bool supportsModelListing() const = 0;
    virtual void prepareRequest(
        QJsonObject &request,
        LLMCore::PromptTemplate *prompt,
        LLMCore::ContextData context,
        LLMCore::RequestType type,
        bool isToolsEnabled,
        bool isThinkingEnabled)
        = 0;
    virtual QList<QString> getInstalledModels(const QString &url) = 0;
    virtual QList<QString> validateRequest(const QJsonObject &request, TemplateType type) = 0;
    virtual QString apiKey() const = 0;
    virtual void prepareNetworkRequest(QNetworkRequest &networkRequest) const = 0;
    virtual ProviderID providerID() const = 0;

    virtual void sendRequest(const RequestID &requestId, const QUrl &url, const QJsonObject &payload)
        = 0;

    virtual bool supportsTools() const { return false; };
    virtual bool supportThinking() const { return false; };
    virtual bool supportImage() const { return false; };

    virtual void cancelRequest(const RequestID &requestId);

    virtual IToolsManager *toolsManager() const { return nullptr; }

    HttpClient *httpClient() const;

public slots:
    virtual void onDataReceived(
        const QodeAssist::LLMCore::RequestID &requestId, const QByteArray &data)
        = 0;
    virtual void onRequestFinished(
        const QodeAssist::LLMCore::RequestID &requestId, bool success, const QString &error)
        = 0;

signals:
    void partialResponseReceived(
        const QodeAssist::LLMCore::RequestID &requestId, const QString &partialText);
    void fullResponseReceived(
        const QodeAssist::LLMCore::RequestID &requestId, const QString &fullText);
    void requestFailed(const QodeAssist::LLMCore::RequestID &requestId, const QString &error);
    void toolExecutionStarted(
        const QString &requestId, const QString &toolId, const QString &toolName);
    void toolExecutionCompleted(
        const QString &requestId,
        const QString &toolId,
        const QString &toolName,
        const QString &result);
    void continuationStarted(const QodeAssist::LLMCore::RequestID &requestId);
    void thinkingBlockReceived(
        const QString &requestId, const QString &thinking, const QString &signature);
    void redactedThinkingBlockReceived(const QString &requestId, const QString &signature);

protected:
    QJsonObject parseEventLine(const QString &line);

    QHash<RequestID, DataBuffers> m_dataBuffers;
    QHash<RequestID, QUrl> m_requestUrls;

private:
    HttpClient *m_httpClient;
};

} // namespace QodeAssist::LLMCore
