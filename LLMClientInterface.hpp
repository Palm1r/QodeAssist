// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <LLMQore/BaseClient.hpp>
#include <languageclient/languageclientinterface.h>
#include <texteditor/texteditor.h>

#include <QPointer>

#include <context/ContextManager.hpp>
#include <context/IDocumentReader.hpp>
#include <context/ProgrammingLanguage.hpp>
#include <logger/IRequestPerformanceLogger.hpp>
#include <settings/CodeCompletionSettings.hpp>
#include <settings/GeneralSettings.hpp>

class QNetworkReply;
class QNetworkAccessManager;

namespace QodeAssist {

class AgentFactory;
class Session;
class SessionManager;

namespace Templates {
struct ContextData;
}

class LLMClientInterface : public LanguageClient::BaseClientInterface
{
    Q_OBJECT

public:
    LLMClientInterface(
        const Settings::GeneralSettings &generalSettings,
        const Settings::CodeCompletionSettings &completeSettings,
        AgentFactory &agentFactory,
        SessionManager &sessionManager,
        Context::IDocumentReader &documentReader,
        IRequestPerformanceLogger &performanceLogger);
    ~LLMClientInterface() override;

    Utils::FilePath serverDeviceTemplate() const override;

    void sendCompletionToClient(
        const QString &completion, const QJsonObject &request, bool isComplete);

    void handleCompletion(const QJsonObject &request);

    // exposed for tests
    void sendData(const QByteArray &data) override;

    Context::ContextManager *contextManager() const;

protected:
    void startImpl() override;

private:
    void handleInitialize(const QJsonObject &request);
    void handleShutdown(const QJsonObject &request);
    void handleTextDocumentDidOpen(const QJsonObject &request);
    void handleInitialized(const QJsonObject &request);
    void handleExit(const QJsonObject &request);
    void handleCancelRequest();
    void sendErrorResponse(const QJsonObject &request, const QString &errorMessage);

    void onCompletionFinished(const QString &requestId);
    void onCompletionFailed(const QString &requestId, const QString &error);
    void finishRequest(const QString &requestId);
    QString requestIdForSession(Session *session) const;

    struct RequestContext
    {
        QJsonObject originalRequest;
        QPointer<Session> session;
    };

    Templates::ContextData prepareContext(
        const QJsonObject &request, const Context::DocumentInfo &documentInfo);

    QString pickCompletionAgent(const QString &filePath) const;

    const Settings::CodeCompletionSettings &m_completeSettings;
    const Settings::GeneralSettings &m_generalSettings;
    AgentFactory &m_agentFactory;
    SessionManager &m_sessionManager;
    Context::IDocumentReader &m_documentReader;
    IRequestPerformanceLogger &m_performanceLogger;
    QElapsedTimer m_completionTimer;
    Context::ContextManager *m_contextManager;
    QHash<QString, RequestContext> m_activeRequests;
};

} // namespace QodeAssist
