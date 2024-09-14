/* 
 * Copyright (C) 2024 Petr Mironychev
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

#include "QodeAssistUtils.hpp"

namespace QodeAssist {

PromptTemplateManager &PromptTemplateManager::instance()
{
    static PromptTemplateManager instance;
    return instance;
}

void PromptTemplateManager::setCurrentFimTemplate(const QString &name)
{
    logMessage("Setting current FIM provider to: " + name);
    if (!m_fimTemplates.contains(name) || m_fimTemplates[name] == nullptr) {
        logMessage("Error to set current FIM template" + name);
        return;
    }

    m_currentFimTemplate = m_fimTemplates[name];
}

Templates::PromptTemplate *PromptTemplateManager::getCurrentFimTemplate()
{
    if (m_currentFimTemplate == nullptr) {
        logMessage("Current fim provider is null");
        return nullptr;
    }

    return m_currentFimTemplate;
}

void PromptTemplateManager::setCurrentChatTemplate(const QString &name)
{
    logMessage("Setting current chat provider to:  " + name);
    if (!m_chatTemplates.contains(name) || m_chatTemplates[name] == nullptr) {
        logMessage("Error to set current chat template" + name);
        return;
    }

    m_currentChatTemplate = m_chatTemplates[name];
}

Templates::PromptTemplate *PromptTemplateManager::getCurrentChatTemplate()
{
    if (m_currentChatTemplate == nullptr)
        logMessage("Current chat provider is null");

    return m_currentChatTemplate;
}

QStringList PromptTemplateManager::fimTemplatesNames() const
{
    return m_fimTemplates.keys();
}

QStringList PromptTemplateManager::chatTemplatesNames() const
{
    return m_chatTemplates.keys();
}

PromptTemplateManager::~PromptTemplateManager()
{
    qDeleteAll(m_fimTemplates);
    qDeleteAll(m_chatTemplates);
}

} // namespace QodeAssist
