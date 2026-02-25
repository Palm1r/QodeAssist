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

#include <llmcore/Provider.hpp>

namespace QodeAssist::LLMCore {
class OpenAIClient;
}

namespace QodeAssist::Providers {

class LlamaCppProvider : public LLMCore::Provider
{
    Q_OBJECT
public:
    explicit LlamaCppProvider(QObject *parent = nullptr);

    QString name() const override;
    QString url() const override;
    QString completionEndpoint() const override;
    QString chatEndpoint() const override;
    bool supportsModelListing() const override;
    void prepareRequest(
        QJsonObject &request,
        LLMCore::PromptTemplate *prompt,
        LLMCore::ContextData context,
        LLMCore::RequestType type,
        bool isToolsEnabled,
        bool isThinkingEnabled) override;
    QFuture<QList<QString>> getInstalledModels(const QString &url) override;
    QList<QString> validateRequest(const QJsonObject &request, LLMCore::TemplateType type) override;
    QString apiKey() const override;
    LLMCore::ProviderID providerID() const override;

    void sendRequest(
        const LLMCore::RequestID &requestId, const QUrl &url, const QJsonObject &payload) override;

    LLMCore::ProviderCapabilities capabilities() const override;
    void cancelRequest(const LLMCore::RequestID &requestId) override;

    LLMCore::ToolsManager *toolsManager() const override;

private:
    LLMCore::OpenAIClient *m_client;
};

} // namespace QodeAssist::Providers
