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

#include "ClaudeRequestMessage.hpp"
#include "ClaudeToolHandler.hpp"
#include "llmcore/Provider.hpp"
#include "tools/ToolsFactory.hpp"
#include <QFuture>
#include <QHash>

namespace QodeAssist::Providers {

class ClaudeProvider : public LLMCore::Provider
{
    Q_OBJECT
public:
    explicit ClaudeProvider(QObject *parent = nullptr);

    QString name() const override;
    QString url() const override;
    QString completionEndpoint() const override;
    QString chatEndpoint() const override;
    bool supportsModelListing() const override;
    void prepareRequest(
        QJsonObject &request,
        LLMCore::PromptTemplate *prompt,
        LLMCore::ContextData context,
        LLMCore::RequestType type) override;
    QList<QString> getInstalledModels(const QString &url) override;
    QList<QString> validateRequest(const QJsonObject &request, LLMCore::TemplateType type) override;
    QString apiKey() const override;
    void prepareNetworkRequest(QNetworkRequest &networkRequest) const override;
    LLMCore::ProviderID providerID() const override;

    void sendRequest(const QString &requestId, const QUrl &url, const QJsonObject &payload) override;

    bool supportsTools() const override;
    LLMCore::IToolsFactory *toolsFactory() const override;

public slots:
    void onDataReceived(const QString &requestId, const QByteArray &data) override;
    void onRequestFinished(const QString &requestId, bool success, const QString &error) override;

private slots:
    void onToolCompleted(const QString &requestId, const QString &toolId, const QString &result);
    void onToolFailed(const QString &requestId, const QString &toolId, const QString &error);

    // ClaudeProvider.hpp - обновляем только нужные части
private:
    struct RequestState
    {
        QJsonObject originalRequest;
        QJsonArray originalMessages;
        QString assistantText;
        QList<QPair<QString, QJsonObject>> toolCalls; // toolId -> {name, input}
        QHash<QString, QString> toolResults;          // toolId -> result

        // Временные поля для парсинга
        QString currentToolId;
        QString currentToolName;
        QString accumulatedInput;

        RequestState() = default;
        RequestState(const QJsonObject &request)
            : originalRequest(request)
            , originalMessages(request["messages"].toArray())
        {}

        bool allToolsCompleted() const { return toolCalls.size() == toolResults.size(); }
    };

    std::unique_ptr<Tools::ToolsFactory> m_toolsFactory;
    std::unique_ptr<ClaudeToolHandler> m_toolHandler;
    QHash<LLMCore::RequestID, RequestState> m_requestStates;
    QHash<QString, QPromise<QString>> m_toolPromises; // Добавили это поле

    QJsonObject parseEventLine(const QString &line);
    void handleContentBlockStart(const QString &requestId, const QJsonObject &event);
    QString handleContentBlockDelta(const QString &requestId, const QJsonObject &event);
    void handleContentBlockStop(const QString &requestId, const QJsonObject &event);
    bool handleMessageDelta(const QString &requestId, const QJsonObject &event);
    void updateResponseAndCheckCompletion(
        const QString &requestId, const QString &tempResponse, bool messageComplete);
    void cleanupRequest(const QString &requestId);
    void executeTool(
        const QString &requestId,
        const QString &toolId,
        const QString &toolName,
        const QJsonObject &input);
};

} // namespace QodeAssist::Providers
