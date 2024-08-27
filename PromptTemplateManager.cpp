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

namespace QodeAssist {

PromptTemplateManager &PromptTemplateManager::instance()
{
    static PromptTemplateManager instance;
    return instance;
}

void PromptTemplateManager::setCurrentTemplate(const QString &name)
{
    if (m_templates.contains(name)) {
        m_currentTemplateName = name;
    }
}

const Templates::PromptTemplate *PromptTemplateManager::getCurrentTemplate() const
{
    auto it = m_templates.find(m_currentTemplateName);
    return it != m_templates.end() ? it.value() : nullptr;
}

QStringList PromptTemplateManager::getTemplateNames() const
{
    return m_templates.keys();
}

PromptTemplateManager::~PromptTemplateManager()
{
    qDeleteAll(m_templates);
}

} // namespace QodeAssist
