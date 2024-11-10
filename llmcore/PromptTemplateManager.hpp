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

#pragma once

#include <QMap>
#include <QString>

#include "PromptTemplate.hpp"

namespace QodeAssist::LLMCore {

class PromptTemplateManager
{
public:
    static PromptTemplateManager &instance();
    ~PromptTemplateManager();

    template<typename T>
    void registerTemplate()
    {
        static_assert(std::is_base_of<PromptTemplate, T>::value,
                      "T must inherit from PromptTemplate");
        T *template_ptr = new T();
        QString name = template_ptr->name();
        if (template_ptr->type() == TemplateType::Fim) {
            m_fimTemplates[name] = template_ptr;
        } else if (template_ptr->type() == TemplateType::Chat) {
            m_chatTemplates[name] = template_ptr;
        }
    }

    PromptTemplate *getFimTemplateByName(const QString &templateName);
    PromptTemplate *getChatTemplateByName(const QString &templateName);

    QStringList fimTemplatesNames() const;
    QStringList chatTemplatesNames() const;

private:
    PromptTemplateManager() = default;
    PromptTemplateManager(const PromptTemplateManager &) = delete;
    PromptTemplateManager &operator=(const PromptTemplateManager &) = delete;

    QMap<QString, PromptTemplate *> m_fimTemplates;
    QMap<QString, PromptTemplate *> m_chatTemplates;
};

} // namespace QodeAssist::LLMCore
