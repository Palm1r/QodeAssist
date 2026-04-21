// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <LLMQore/BaseTool.hpp>

#include <context/IgnoreManager.hpp>

namespace QodeAssist::Tools {

class ListProjectFilesTool : public ::LLMQore::BaseTool
{
    Q_OBJECT
public:
    explicit ListProjectFilesTool(QObject *parent = nullptr);

    QString id() const override;
    QString displayName() const override;
    QString description() const override;
    QJsonObject parametersSchema() const override;

    QFuture<LLMQore::ToolResult> executeAsync(const QJsonObject &input = QJsonObject()) override;

private:
    QString formatFileList(const QStringList &files) const;
    Context::IgnoreManager *m_ignoreManager;
};

} // namespace QodeAssist::Tools
