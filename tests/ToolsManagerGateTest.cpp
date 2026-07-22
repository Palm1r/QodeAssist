// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "ToolsManagerGateTest.hpp"

#include <QJsonObject>
#include <QSignalSpy>
#include <QTest>

#include <LLMQore/BaseTool.hpp>
#include <LLMQore/ToolsManager.hpp>

#include "tools/BuildProjectTool.hpp"
#include "tools/CreateNewFileTool.hpp"
#include "tools/EditFileTool.hpp"
#include "tools/EditorStateTools.hpp"
#include "tools/ExecuteTerminalCommandTool.hpp"
#include "tools/GetIssuesListTool.hpp"
#include "tools/ReadFileTool.hpp"

namespace QodeAssist {

namespace {

class FakeGatedTool : public ::LLMQore::BaseTool
{
public:
    FakeGatedTool(QString toolId, ::LLMQore::ToolSafety safety, QObject *parent = nullptr)
        : ::LLMQore::BaseTool(parent)
        , m_id(std::move(toolId))
        , m_safety(safety)
    {}

    QString id() const override { return m_id; }
    QString displayName() const override { return m_id; }
    QString description() const override { return m_id; }
    QJsonObject parametersSchema() const override { return {}; }
    ::LLMQore::ToolSafety safety() const override { return m_safety; }

    QFuture<LLMQore::ToolResult> executeAsync(const QJsonObject &) override
    {
        ++executions;
        QPromise<LLMQore::ToolResult> promise;
        promise.start();
        promise.addResult(LLMQore::ToolResult::text(QStringLiteral("ran ") + m_id));
        promise.finish();
        return promise.future();
    }

    int executions = 0;

private:
    QString m_id;
    ::LLMQore::ToolSafety m_safety;
};

} // namespace

void ToolsManagerGateTest::testToolSafetyDefaultsToMutating()
{
    FakeGatedTool undeclared("undeclared", ::LLMQore::ToolSafety::Mutating);
    QCOMPARE(undeclared.safety(), ::LLMQore::ToolSafety::Mutating);

    QCOMPARE(Tools::ReadFileTool().safety(), ::LLMQore::ToolSafety::ReadOnly);
    QCOMPARE(Tools::GetIssuesListTool().safety(), ::LLMQore::ToolSafety::ReadOnly);
    QCOMPARE(Tools::ListOpenEditorsTool().safety(), ::LLMQore::ToolSafety::ReadOnly);

    QCOMPARE(Tools::EditFileTool().safety(), ::LLMQore::ToolSafety::Mutating);
    QCOMPARE(Tools::CreateNewFileTool().safety(), ::LLMQore::ToolSafety::Mutating);
    QCOMPARE(Tools::ExecuteTerminalCommandTool().safety(), ::LLMQore::ToolSafety::Mutating);
    QCOMPARE(Tools::BuildProjectTool().safety(), ::LLMQore::ToolSafety::Mutating);
}

void ToolsManagerGateTest::testToolsManagerWithoutAGateRunsImmediately()
{
    ::LLMQore::ToolsManager manager(::LLMQore::ToolSchemaFormat::OpenAI);
    auto *tool = new FakeGatedTool("edit_thing", ::LLMQore::ToolSafety::Mutating, &manager);
    manager.addTool(tool);

    manager.executeToolCall("req-1", "call-1", "edit_thing", QJsonObject{});

    QTRY_COMPARE(tool->executions, 1);
}

void ToolsManagerGateTest::testToolsManagerGateCanDeclineAToolCall()
{
    ::LLMQore::ToolsManager manager(::LLMQore::ToolSchemaFormat::OpenAI);
    auto *tool = new FakeGatedTool("edit_thing", ::LLMQore::ToolSafety::Mutating, &manager);
    manager.addTool(tool);

    QStringList gated;
    manager.setExecutionGate(
        [&gated](
            const QString &, const QString &, const QString &toolName, const QJsonObject &) {
            gated.append(toolName);
            QPromise<bool> promise;
            promise.start();
            promise.addResult(false);
            promise.finish();
            return promise.future();
        });

    QString resultText;
    QObject::connect(
        &manager,
        &::LLMQore::ToolsManager::toolExecutionResult,
        &manager,
        [&resultText](const QString &, const QString &, const QString &, const QString &result) {
            resultText = result;
        });

    manager.executeToolCall("req-1", "call-1", "edit_thing", QJsonObject{});

    QTRY_VERIFY(!resultText.isEmpty());

    QCOMPARE(gated, QStringList{"edit_thing"});
    QCOMPARE(tool->executions, 0);
    QVERIFY2(resultText.contains("declined"), qPrintable(resultText));
}

void ToolsManagerGateTest::testToolsManagerGateCanAllowAToolCall()
{
    ::LLMQore::ToolsManager manager(::LLMQore::ToolSchemaFormat::OpenAI);
    auto *tool = new FakeGatedTool("edit_thing", ::LLMQore::ToolSafety::Mutating, &manager);
    manager.addTool(tool);

    manager.setExecutionGate(
        [](const QString &, const QString &, const QString &, const QJsonObject &) {
            QPromise<bool> promise;
            promise.start();
            promise.addResult(true);
            promise.finish();
            return promise.future();
        });

    manager.executeToolCall("req-1", "call-1", "edit_thing", QJsonObject{});

    QTRY_COMPARE(tool->executions, 1);
}

void ToolsManagerGateTest::testToolsManagerGateResumesTheQueueAfterADenial()
{
    ::LLMQore::ToolsManager manager(::LLMQore::ToolSchemaFormat::OpenAI);
    auto *denied = new FakeGatedTool("edit_thing", ::LLMQore::ToolSafety::Mutating, &manager);
    auto *allowed = new FakeGatedTool("read_thing", ::LLMQore::ToolSafety::ReadOnly, &manager);
    manager.addTool(denied);
    manager.addTool(allowed);

    manager.setExecutionGate(
        [](const QString &, const QString &, const QString &toolName, const QJsonObject &) {
            QPromise<bool> promise;
            promise.start();
            promise.addResult(toolName != QLatin1String("edit_thing"));
            promise.finish();
            return promise.future();
        });

    bool finished = false;
    QObject::connect(
        &manager,
        &::LLMQore::ToolsManager::toolExecutionComplete,
        &manager,
        [&finished](const QString &, const QHash<QString, LLMQore::ToolResult> &) {
            finished = true;
        });

    manager.executeToolCall("req-1", "call-1", "edit_thing", QJsonObject{});
    manager.executeToolCall("req-1", "call-2", "read_thing", QJsonObject{});

    QTRY_VERIFY(finished);
    QCOMPARE(denied->executions, 0);
    QCOMPARE(allowed->executions, 1);
}

} // namespace QodeAssist
