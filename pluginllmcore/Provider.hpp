// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QFlags>
#include <QFuture>
#include <QObject>
#include <QString>
#include <utils/environment.h>

#include "ContextData.hpp"
#include "PromptTemplate.hpp"
#include "RequestType.hpp"

namespace LLMQore {
class BaseClient;
class ToolsManager;
}

class QJsonObject;

namespace QodeAssist::PluginLLMCore {

enum class ProviderCapability {
    Tools        = 0x1,
    Thinking     = 0x2,
    Image        = 0x4,
    ModelListing = 0x8,
};
Q_DECLARE_FLAGS(ProviderCapabilities, ProviderCapability)
Q_DECLARE_OPERATORS_FOR_FLAGS(ProviderCapabilities)

class Provider : public QObject
{
    Q_OBJECT
public:
    explicit Provider(QObject *parent = nullptr);

    virtual ~Provider() = default;

    virtual QString name() const = 0;
    virtual QString url() const = 0;
    virtual void prepareRequest(
        QJsonObject &request,
        PluginLLMCore::PromptTemplate *prompt,
        PluginLLMCore::ContextData context,
        PluginLLMCore::RequestType type,
        bool isToolsEnabled,
        bool isThinkingEnabled)
        = 0;
    virtual QFuture<QList<QString>> getInstalledModels(const QString &url) = 0;
    virtual ProviderID providerID() const = 0;
    virtual ProviderCapabilities capabilities() const { return {}; }

    virtual ::LLMQore::BaseClient *client() const = 0;
    virtual QString apiKey() const = 0;

    virtual RequestID sendRequest(
        const QUrl &url, const QJsonObject &payload, const QString &endpoint);
    void cancelRequest(const RequestID &requestId);
    ::LLMQore::ToolsManager *toolsManager() const;
};

} // namespace QodeAssist::PluginLLMCore
