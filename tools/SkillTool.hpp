// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <LLMQore/BaseTool.hpp>
#include <QFuture>
#include <QJsonObject>
#include <QObject>
#include <QPointer>

namespace QodeAssist::Skills {
class SkillsManager;
}

namespace QodeAssist::Tools {

class SkillTool : public ::LLMQore::BaseTool
{
    Q_OBJECT

public:
    explicit SkillTool(Skills::SkillsManager *skillsManager, QObject *parent = nullptr);

    QString id() const override;
    QString displayName() const override;
    QString description() const override;
    QJsonObject parametersSchema() const override;
    QFuture<LLMQore::ToolResult> executeAsync(const QJsonObject &input) override;

private:
    QPointer<Skills::SkillsManager> m_skillsManager;
};

} // namespace QodeAssist::Tools
