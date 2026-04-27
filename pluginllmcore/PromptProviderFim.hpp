// Copyright (C) 2025 Povilas Kanapickas <povilas@radix.lt>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "IPromptProvider.hpp"
#include "PromptTemplateManager.hpp"

namespace QodeAssist::PluginLLMCore {

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

} // namespace QodeAssist::PluginLLMCore
