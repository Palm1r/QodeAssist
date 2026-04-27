// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <context/IgnoreManager.hpp>
#include <LLMQore/BaseTool.hpp>
#include <QFuture>
#include <QJsonObject>
#include <QObject>

namespace QodeAssist::Tools {

class FindFileTool : public ::LLMQore::BaseTool
{
    Q_OBJECT

public:
    explicit FindFileTool(QObject *parent = nullptr);

    QString id() const override;
    QString displayName() const override;
    QString description() const override;
    QJsonObject parametersSchema() const override;
    QFuture<LLMQore::ToolResult> executeAsync(const QJsonObject &input) override;

private:
    Context::IgnoreManager *m_ignoreManager;
};

} // namespace QodeAssist::Tools
