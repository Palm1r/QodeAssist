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

#include "templates/PromptTemplate.hpp"

namespace QodeAssist {

class PromptTemplateManager
{
public:
    static PromptTemplateManager &instance();

    template<typename T>
    void registerTemplate()
    {
        static_assert(std::is_base_of<Templates::PromptTemplate, T>::value,
                      "T must inherit from PromptTemplate");
        T *template_ptr = new T();
        QString name = template_ptr->name();
        m_templates[name] = template_ptr;
    }

    void setCurrentTemplate(const QString &name);
    const Templates::PromptTemplate *getCurrentTemplate() const;
    QStringList getTemplateNames() const;

    ~PromptTemplateManager();

private:
    PromptTemplateManager() = default;
    PromptTemplateManager(const PromptTemplateManager &) = delete;
    PromptTemplateManager &operator=(const PromptTemplateManager &) = delete;

    QMap<QString, Templates::PromptTemplate *> m_templates;
    QString m_currentTemplateName;
};

} // namespace QodeAssist
