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


#include <QFuture>
#include <QNetworkRequest>
#include <QObject>
#include <QString>

#include "ContextData.hpp"
#include "DataBuffers.hpp"
#include "RequestType.hpp"

#include <llmcore/prompt/PromptTemplate.hpp>

class QNetworkReply;
class QJsonObject;

namespace QodeAssist::LLMCore {

enum ProviderCapability {
    NoCapabilities = 0,
    ToolsCapability = 1 << 0,
    ThinkingCapability = 1 << 1,
    ImageCapability = 1 << 2
};
Q_DECLARE_FLAGS(ProviderCapabilities, ProviderCapability)
Q_DECLARE_OPERATORS_FOR_FLAGS(ProviderCapabilities)

class BaseClient;
class ToolsManager;

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
    virtual QFuture<QList<QString>> getInstalledModels(const QString &url) = 0;
    virtual QList<QString> validateRequest(const QJsonObject &request, TemplateType type) = 0;
    virtual QString apiKey() const = 0;
    virtual ProviderID providerID() const = 0;

    virtual void sendRequest(const RequestID &requestId, const QUrl &url, const QJsonObject &payload)
        = 0;

    virtual ProviderCapabilities capabilities() const { return NoCapabilities; }
    bool hasCapability(ProviderCapability cap) const { return capabilities().testFlag(cap); }

    virtual void cancelRequest(const RequestID &requestId) = 0;

    virtual ToolsManager *toolsManager() const { return nullptr; }

protected:
    void connectClientSignals(BaseClient *client);

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
};

} // namespace QodeAssist::LLMCore
