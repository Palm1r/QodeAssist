// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QFuture>
#include <QObject>
#include <QString>
#include <QStringList>
#include <utils/environment.h>

#include "ContextData.hpp"
#include "ProviderID.hpp"
#include "LLMQore/BaseClient.hpp"

namespace LLMQore {
class BaseClient;
class ToolsManager;
}

namespace QodeAssist::Templates {
class PromptTemplate;
}

class QJsonObject;

namespace QodeAssist::Providers {

using Templates::ContextData;
using Templates::PromptTemplate;
using LLMQore::RequestID;

class Provider : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(Provider)
public:
    explicit Provider(QObject *parent = nullptr);

    virtual ~Provider() = default;

    virtual QString name() const = 0;

    virtual QString url() const { return m_url; }
    virtual QString apiKey() const { return m_apiKey; }
    void setUrl(const QString &url) { m_url = url; }
    void setApiKey(const QString &apiKey) { m_apiKey = apiKey; }

    [[nodiscard]] virtual bool prepareRequest(
        QJsonObject &request,
        PromptTemplate *prompt,
        const ContextData &context,
        bool isToolsEnabled,
        QString *errorOut = nullptr);
    virtual QFuture<QList<QString>> getInstalledModels(const QString &url) = 0;
    virtual ProviderID providerID() const = 0;

    virtual ::LLMQore::BaseClient *client() const = 0;

    virtual RequestID sendRequest(
        const QUrl &url, const QJsonObject &payload, const QString &endpoint);
    void cancelRequest(const RequestID &requestId);
    ::LLMQore::ToolsManager *toolsManager() const;

    void setPromptCaching(
        bool enabled, bool extendedTtl, const QStringList &breakpoints = {});

private:
    QString m_url;
    QString m_apiKey;
    bool m_promptCachingEnabled = false;
    bool m_promptCachingExtendedTtl = false;
    QStringList m_promptCacheBreakpoints;
};

} // namespace QodeAssist::Providers
