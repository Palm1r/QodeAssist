/*
 * Copyright (C) 2025 Povilas Kanapickas
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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>

#include "LLMClientInterface.hpp"
#include "MockDocumentReader.hpp"
#include "MockRequestHandler.hpp"
#include "llmcore/IPromptProvider.hpp"
#include "llmcore/IProviderRegistry.hpp"
#include "logger/EmptyRequestPerformanceLogger.hpp"
#include "settings/CodeCompletionSettings.hpp"
#include "settings/GeneralSettings.hpp"
#include "templates/Templates.hpp"
#include <coreplugin/editormanager/documentmodel.h>

using namespace testing;

namespace QodeAssist {

class MockPromptProvider : public LLMCore::IPromptProvider
{
public:
    MOCK_METHOD(LLMCore::PromptTemplate *, getTemplateByName, (const QString &), (const override));
    MOCK_METHOD(QStringList, templatesNames, (), (const override));
    MOCK_METHOD(QStringList, getTemplatesForProvider, (LLMCore::ProviderID id), (const override));
};

class MockProviderRegistry : public LLMCore::IProviderRegistry
{
public:
    MOCK_METHOD(LLMCore::Provider *, getProviderByName, (const QString &), (override));
    MOCK_METHOD(QStringList, providersNames, (), (const override));
};

class MockProvider : public LLMCore::Provider
{
public:
    QString name() const override { return "mock_provider"; }
    QString url() const override { return "https://mock_url"; }
    QString completionEndpoint() const override { return "/v1/completions"; }
    QString chatEndpoint() const override { return "/v1/chat/completions"; }
    bool supportsModelListing() const override { return false; }

    void prepareRequest(
        QJsonObject &request,
        LLMCore::PromptTemplate *promptTemplate,
        LLMCore::ContextData context,
        LLMCore::RequestType requestType,
        bool isToolsEnabled,
        bool isThinkingEnabled) override
    {
        promptTemplate->prepareRequest(request, context);
    }

    bool handleResponse(QNetworkReply *reply, QString &accumulatedResponse) override
    {
        return true;
    }

    QList<QString> getInstalledModels(const QString &url) override { return {}; }

    QStringList validateRequest(
        const QJsonObject &request, LLMCore::TemplateType templateType) override
    {
        return {};
    }

    QString apiKey() const override { return "mock_api_key"; }
    void prepareNetworkRequest(QNetworkRequest &request) const override {}
    LLMCore::ProviderID providerID() const override { return LLMCore::ProviderID::OpenAI; }
};

class LLMClientInterfaceTest : public Test
{
protected:
    void SetUp() override
    {
        Core::DocumentModel::init();

        m_provider = std::make_unique<MockProvider>();
        m_fimTemplate = std::make_unique<Templates::CodeLlamaQMLFim>();
        m_chatTemplate = std::make_unique<Templates::Claude>();

        ON_CALL(m_providerRegistry, getProviderByName(_)).WillByDefault(Return(m_provider.get()));
        ON_CALL(m_promptProvider, getTemplateByName(_)).WillByDefault(Return(m_fimTemplate.get()));

        EXPECT_CALL(m_providerRegistry, getProviderByName(_)).Times(testing::AnyNumber());
        EXPECT_CALL(m_promptProvider, getTemplateByName(_)).Times(testing::AnyNumber());

        m_generalSettings.ccProvider.setValue("mock_provider");
        m_generalSettings.ccModel.setValue("mock_model");
        m_generalSettings.ccTemplate.setValue("mock_template");
        m_generalSettings.ccUrl.setValue("http://localhost:8000");

        m_completeSettings.systemPromptForNonFimModels.setValue("system prompt non fim");
        m_completeSettings.systemPrompt.setValue("system prompt");
        m_completeSettings.userMessageTemplateForCC.setValue(
            "user message template prefix:\n${prefix}\nsuffix:\n${suffix}\n");

        m_client = std::make_unique<LLMClientInterface>(
            m_generalSettings,
            m_completeSettings,
            m_providerRegistry,
            &m_promptProvider,
            m_documentReader,
            m_performanceLogger);
    }

    void TearDown() override { Core::DocumentModel::destroy(); }

    QJsonObject createInitializeRequest()
    {
        QJsonObject request;
        request["jsonrpc"] = "2.0";
        request["id"] = "init-1";
        request["method"] = "initialize";
        return request;
    }

    QString buildTestFilePath() { return QString(CMAKE_CURRENT_SOURCE_DIR) + "/test_file.py"; }

    QJsonObject createCompletionRequest()
    {
        QJsonObject position;
        position["line"] = 2;
        position["character"] = 5;

        QJsonObject doc;
        // change next line to link to test_file.py in current directory of the cmake project
        doc["uri"] = "file://" + buildTestFilePath();
        doc["position"] = position;

        QJsonObject params;
        params["doc"] = doc;

        QJsonObject request;
        request["jsonrpc"] = "2.0";
        request["id"] = "completion-1";
        request["method"] = "getCompletionsCycling";
        request["params"] = params;

        return request;
    }

    QJsonObject createCancelRequest(const QString &idToCancel)
    {
        QJsonObject params;
        params["id"] = idToCancel;

        QJsonObject request;
        request["jsonrpc"] = "2.0";
        request["id"] = "cancel-1";
        request["method"] = "$/cancelRequest";
        request["params"] = params;

        return request;
    }

    Settings::GeneralSettings m_generalSettings;
    Settings::CodeCompletionSettings m_completeSettings;
    MockProviderRegistry m_providerRegistry;
    MockPromptProvider m_promptProvider;
    MockDocumentReader m_documentReader;
    EmptyRequestPerformanceLogger m_performanceLogger;
    std::unique_ptr<LLMClientInterface> m_client;
    std::unique_ptr<MockProvider> m_provider;
    std::unique_ptr<LLMCore::PromptTemplate> m_fimTemplate;
    std::unique_ptr<LLMCore::PromptTemplate> m_chatTemplate;
};

TEST_F(LLMClientInterfaceTest, initialize)
{
    QSignalSpy spy(m_client.get(), &LanguageClient::BaseClientInterface::messageReceived);

    QJsonObject request = createInitializeRequest();
    m_client->sendData(QJsonDocument(request).toJson());

    ASSERT_EQ(spy.count(), 1);
    auto message = spy.takeFirst().at(0).value<LanguageServerProtocol::JsonRpcMessage>();
    QJsonObject response = message.toJsonObject();

    EXPECT_EQ(response["id"].toString(), "init-1");
    EXPECT_TRUE(response.contains("result"));
    EXPECT_TRUE(response["result"].toObject().contains("capabilities"));
    EXPECT_TRUE(response["result"].toObject().contains("serverInfo"));
}

TEST_F(LLMClientInterfaceTest, ServerDeviceTemplate)
{
    EXPECT_EQ(m_client->serverDeviceTemplate().toFSPathString(), "QodeAssist");
}

} // namespace QodeAssist
