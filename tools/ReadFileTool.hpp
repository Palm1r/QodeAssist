// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <LLMQore/BaseTool.hpp>
#include <QFuture>
#include <QJsonObject>
#include <QObject>

namespace QodeAssist::Tools {

class ReadFileTool : public ::LLMQore::BaseTool
{
    Q_OBJECT

public:
    explicit ReadFileTool(QObject *parent = nullptr);

    QString id() const override;
    QString displayName() const override;
    QString description() const override;
    QJsonObject parametersSchema() const override;
    QFuture<LLMQore::ToolResult> executeAsync(const QJsonObject &input) override;
};

} // namespace QodeAssist::Tools
