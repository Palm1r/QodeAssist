// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <LLMQore/BaseTool.hpp>

namespace QodeAssist::Tools {

class CreateNewFileTool : public ::LLMQore::BaseTool
{
    Q_OBJECT
public:
    explicit CreateNewFileTool(QObject *parent = nullptr);

    QString id() const override;
    QString displayName() const override;
    QString description() const override;
    QJsonObject parametersSchema() const override;

    QFuture<LLMQore::ToolResult> executeAsync(const QJsonObject &input = QJsonObject()) override;
};

} // namespace QodeAssist::Tools

