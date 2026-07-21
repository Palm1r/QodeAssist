// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QJsonObject>
#include <QNetworkRequest>

#include <LLMQore/BaseClient.hpp>
#include <LLMQore/BaseTool.hpp>
#include <LLMQore/ToolResult.hpp>

#include "providers/Provider.hpp"
#include "templates/IPromptProvider.hpp"
#include "templates/PromptTemplate.hpp"

namespace QodeAssist {

class FakeChatClient : public ::LLMQore::BaseClient
{
    Q_OBJECT

public:
    using BaseClient::BaseClient;

    ::LLMQore::RequestID sendMessage(
        const QJsonObject &payload,
        const QString &endpoint,
        ::LLMQore::RequestMode mode) override
    {
        Q_UNUSED(endpoint)
        Q_UNUSED(mode)
        lastPayload = payload;
        return QStringLiteral("fake-req-%1").arg(++requestCounter);
    }

    ::LLMQore::RequestID ask(const QString &prompt, ::LLMQore::RequestMode mode) override
    {
        Q_UNUSED(prompt)
        Q_UNUSED(mode)
        return {};
    }

    QFuture<QList<QString>> listModels(const QString &endpoint) override
    {
        Q_UNUSED(endpoint)
        return QtFuture::makeReadyValueFuture(QList<QString>{});
    }

    QJsonObject lastPayload;
    int requestCounter = 0;

protected:
    ::LLMQore::ToolSchemaFormat toolSchemaFormat() const override
    {
        return ::LLMQore::ToolSchemaFormat::OpenAI;
    }

    void processData(const ::LLMQore::RequestID &, const QByteArray &) override {}
    void processBufferedResponse(const ::LLMQore::RequestID &, const QByteArray &) override {}

    QNetworkRequest prepareNetworkRequest(const QUrl &url) const override
    {
        return QNetworkRequest(url);
    }

    ::LLMQore::BaseMessage *messageForRequest(const ::LLMQore::RequestID &) const override
    {
        return nullptr;
    }

    void cleanupDerivedData(const ::LLMQore::RequestID &) override {}

    QJsonObject buildContinuationPayload(
        const QJsonObject &,
        ::LLMQore::BaseMessage *,
        const QHash<QString, ::LLMQore::ToolResult> &) override
    {
        return {};
    }
};

class FakeMutatingTool : public ::LLMQore::BaseTool
{
    Q_OBJECT

public:
    using BaseTool::BaseTool;

    QString id() const override { return QStringLiteral("fake_write"); }
    QString displayName() const override { return QStringLiteral("Fake Write"); }
    QString description() const override { return QStringLiteral("Writes something"); }
    QJsonObject parametersSchema() const override { return {}; }

    QFuture<::LLMQore::ToolResult> executeAsync(const QJsonObject &input) override
    {
        Q_UNUSED(input)
        ++executions;
        return QtFuture::makeReadyValueFuture(::LLMQore::ToolResult::text(QStringLiteral("done")));
    }

    int executions = 0;
};

class FakeChatTemplate : public Templates::PromptTemplate
{
public:
    Templates::TemplateType type() const override { return Templates::TemplateType::Chat; }
    QString name() const override { return QStringLiteral("FakeChatTemplate"); }
    QStringList stopWords() const override { return {}; }
    void prepareRequest(QJsonObject &, const LLMCore::ContextData &) const override {}
    QString description() const override { return {}; }
    bool isSupportProvider(Providers::ProviderID) const override { return true; }
    bool supportsToolHistory() const override { return true; }
};

class FakeChatPromptProvider : public Templates::IPromptProvider
{
public:
    Templates::PromptTemplate *getTemplateByName(const QString &) const override
    {
        return &m_template;
    }

    QStringList templatesNames() const override { return {m_template.name()}; }

    QStringList getTemplatesForProvider(Providers::ProviderID) const override
    {
        return {m_template.name()};
    }

private:
    mutable FakeChatTemplate m_template;
};

class FakeLlmProvider : public Providers::Provider
{
    Q_OBJECT

public:
    explicit FakeLlmProvider(QObject *parent = nullptr)
        : Providers::Provider(parent)
        , m_client(new FakeChatClient(this))
    {}

    QString name() const override { return QStringLiteral("FakeLLM"); }
    QString url() const override { return {}; }

    void prepareRequest(
        QJsonObject &,
        Templates::PromptTemplate *,
        LLMCore::ContextData context,
        LLMCore::RequestType,
        bool,
        bool) override
    {
        lastContext = context;
    }

    LLMCore::ContextData lastContext;

    QFuture<QList<QString>> getInstalledModels(const QString &) override
    {
        return QtFuture::makeReadyValueFuture(QList<QString>{});
    }

    Providers::ProviderID providerID() const override { return Providers::ProviderID::Any; }

    Providers::ProviderCapabilities capabilities() const override
    {
        return Providers::ProviderCapability::Tools;
    }

    ::LLMQore::BaseClient *client() const override { return m_client; }
    QString apiKey() const override { return {}; }

    FakeChatClient *fakeClient() const { return m_client; }

private:
    FakeChatClient *m_client = nullptr;
};

} // namespace QodeAssist
