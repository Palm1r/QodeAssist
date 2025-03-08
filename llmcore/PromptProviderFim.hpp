/*
 * Copyright (C) 2025 Povilas Kanapickas <povilas@radix.lt>
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

#include "IPromptProvider.hpp"
#include "PromptTemplateManager.hpp"

namespace QodeAssist::LLMCore {

class PromptProviderFim : public IPromptProvider
{
public:
    explicit PromptProviderFim(PromptTemplateManager &templateManager)
        : m_templateManager(templateManager)
    {}

    ~PromptProviderFim() = default;

    PromptTemplate *getTemplateByName(const QString &templateName) const override
    {
        return m_templateManager.getFimTemplateByName(templateName);
    }

    QStringList templatesNames() const override { return m_templateManager.fimTemplatesNames(); }

    QStringList getTemplatesForProvider(ProviderID id) const override
    {
        return m_templateManager.getFimTemplatesForProvider(id);
    }

private:
    PromptTemplateManager &m_templateManager;
};

} // namespace QodeAssist::LLMCore
