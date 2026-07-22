// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "tools/ProposeCompletionTool.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <logger/Logger.hpp>

namespace QodeAssist::Tools {

ProposeCompletionTool::ProposeCompletionTool(QObject *parent)
    : ::LLMQore::BaseTool(parent)
{
}

QString ProposeCompletionTool::toolId()
{
    return QStringLiteral("propose_completion");
}

QString ProposeCompletionTool::id() const
{
    return toolId();
}

QString ProposeCompletionTool::displayName() const
{
    return tr("Propose Completion");
}

QString ProposeCompletionTool::description() const
{
    return tr(
        "Propose the code completion text for the current cursor position. Pass the actual "
        "code to insert in the `text` argument — it must be the real, non-empty continuation, "
        "never an empty string. The text is shown to the user as an inline ghost-text "
        "suggestion they accept with Tab. Call this exactly once, after you have worked out "
        "the completion; it inserts nothing by itself.");
}

QJsonObject ProposeCompletionTool::parametersSchema() const
{
    return QJsonObject{
        {"type", "object"},
        {"properties",
         QJsonObject{
             {"file",
              QJsonObject{
                  {"type", "string"},
                  {"description", "Absolute path of the file the completion is for."}}},
             {"line",
              QJsonObject{
                  {"type", "integer"},
                  {"description", "Zero-based line of the cursor position."}}},
             {"column",
              QJsonObject{
                  {"type", "integer"},
                  {"description", "Zero-based column of the cursor position."}}},
             {"text",
              QJsonObject{
                  {"type", "string"},
                  {"description",
                   "The actual code to insert at the cursor position, verbatim. Must be the "
                   "real, non-empty continuation — never an empty string."}}}}},
        {"required", QJsonArray{"text"}}};
}

::LLMQore::ToolSafety ProposeCompletionTool::safety() const
{
    return ::LLMQore::ToolSafety::ReadOnly;
}

QFuture<::LLMQore::ToolResult> ProposeCompletionTool::executeAsync(const QJsonObject &input)
{
    LOG_MESSAGE(
        QString("propose_completion invoked with input: %1")
            .arg(QString::fromUtf8(QJsonDocument(input).toJson(QJsonDocument::Compact))));

    const QString filePath = input.value("file").toString();
    const QString text = input.value("text").toString();

    if (text.isEmpty()) {
        LOG_MESSAGE(QStringLiteral("propose_completion rejected: text is empty"));
        return QtFuture::makeReadyValueFuture(
            ::LLMQore::ToolResult::error(QStringLiteral(
                "The 'text' argument was empty. Work out the actual code to insert at the cursor "
                "and call propose_completion again with that non-empty completion text.")));
    }

    const int line = input.value("line").toInt(-1);
    const int column = input.value("column").toInt(-1);

    emit completionProposed(filePath, line, column, text);

    return QtFuture::makeReadyValueFuture(
        ::LLMQore::ToolResult::text(
            QStringLiteral("Completion proposal shown to the user as a suggestion.")));
}

} // namespace QodeAssist::Tools
