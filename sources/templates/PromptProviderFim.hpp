// Copyright (C) 2025 Povilas Kanapickas <povilas@radix.lt>
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include "templates/IPromptProvider.hpp"
#include "templates/PromptTemplateManager.hpp"

namespace QodeAssist::Templates {

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

    QStringList getTemplatesForProvider(Providers::ProviderID id) const override
    {
        return m_templateManager.getFimTemplatesForProvider(id);
    }

private:
    PromptTemplateManager &m_templateManager;
};

} // namespace QodeAssist::Templates
