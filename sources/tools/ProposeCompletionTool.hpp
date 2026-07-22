// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <LLMQore/BaseTool.hpp>

namespace QodeAssist::Tools {

class ProposeCompletionTool : public ::LLMQore::BaseTool
{
    Q_OBJECT

public:
    explicit ProposeCompletionTool(QObject *parent = nullptr);

    static QString toolId();

    QString id() const override;
    QString displayName() const override;
    QString description() const override;
    QJsonObject parametersSchema() const override;
    ::LLMQore::ToolSafety safety() const override;

    QFuture<::LLMQore::ToolResult> executeAsync(const QJsonObject &input = QJsonObject()) override;

signals:
    void completionProposed(const QString &filePath, int line, int column, const QString &text);
};

} // namespace QodeAssist::Tools
