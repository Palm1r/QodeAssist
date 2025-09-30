/* 
 * Copyright (C) 2025 Petr Mironychev
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

#pragma once

#include <llmcore/BaseTool.hpp>

#include <context/IgnoreManager.hpp>

namespace QodeAssist::Tools {

class ListProjectFilesTool : public LLMCore::BaseTool
{
    Q_OBJECT
public:
    explicit ListProjectFilesTool(QObject *parent = nullptr);

    QString name() const override;
    QString description() const override;
    QJsonObject getDefinition(LLMCore::ToolSchemaFormat format) const override;
    QFuture<QString> executeAsync(const QJsonObject &input = QJsonObject()) override;

private:
    QString formatFileList(const QStringList &files) const;
    Context::IgnoreManager *m_ignoreManager;
};

} // namespace QodeAssist::Tools
