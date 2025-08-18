/* 
 * Copyright (C) 2024-2025 Petr Mironychev
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

#include "PromptTemplateManager.hpp"

#include <QMessageBox>

namespace QodeAssist::LLMCore {

PromptTemplateManager &PromptTemplateManager::instance()
{
    static PromptTemplateManager instance;
    return instance;
}

QStringList PromptTemplateManager::fimTemplatesNames() const
{
    return m_fimTemplates.keys();
}

QStringList PromptTemplateManager::chatTemplatesNames() const
{
    return m_chatTemplates.keys();
}

QStringList PromptTemplateManager::getFimTemplatesForProvider(ProviderID id)
{
    QStringList templateList;

    for (const auto tmpl : m_fimTemplates) {
        if (tmpl->isSupportProvider(id)) {
            templateList.append(tmpl->name());
        }
    }

    return templateList;
}

QStringList PromptTemplateManager::getChatTemplatesForProvider(ProviderID id)
{
    QStringList templateList;

    for (const auto tmpl : m_chatTemplates) {
        if (tmpl->isSupportProvider(id)) {
            templateList.append(tmpl->name());
        }
    }

    return templateList;
}

PromptTemplateManager::~PromptTemplateManager()
{
    qDeleteAll(m_fimTemplates);
}

PromptTemplate *PromptTemplateManager::getFimTemplateByName(const QString &templateName)
{
    if (!m_fimTemplates.contains(templateName)) {
        QMessageBox::warning(
            nullptr,
            QObject::tr("Template Not Found"),
            QObject::tr("Template '%1' was not found or has been updated. Please re-set new one.")
                .arg(templateName));
        return m_fimTemplates.first();
    }
    return m_fimTemplates[templateName];
}

PromptTemplate *PromptTemplateManager::getChatTemplateByName(const QString &templateName)
{
    if (!m_chatTemplates.contains(templateName)) {
        QMessageBox::warning(
            nullptr,
            QObject::tr("Template Not Found"),
            QObject::tr("Template '%1' was not found or has been updated. Please re-set new one.")
                .arg(templateName));
        return m_chatTemplates.first();
    }
    return m_chatTemplates[templateName];
}

} // namespace QodeAssist::LLMCore
