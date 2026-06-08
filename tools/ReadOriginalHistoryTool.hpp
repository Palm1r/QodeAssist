// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <LLMQore/BaseTool.hpp>

#include <QMutex>
#include <QString>

namespace QodeAssist::Tools {

class ReadOriginalHistoryTool : public ::LLMQore::BaseTool
{
    Q_OBJECT

public:
    explicit ReadOriginalHistoryTool(QObject *parent = nullptr);

    QString id() const override;
    QString displayName() const override;
    QString description() const override;
    QJsonObject parametersSchema() const override;

    QFuture<LLMQore::ToolResult> executeAsync(const QJsonObject &input = QJsonObject()) override;

    void setCurrentSessionId(const QString &sessionId);

private:
    mutable QMutex m_mutex;
    QString m_currentSessionId;
};

} // namespace QodeAssist::Tools
