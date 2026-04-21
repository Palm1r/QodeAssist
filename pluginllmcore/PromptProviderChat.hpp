// Copyright (C) 2025 Povilas Kanapickas <povilas@radix.lt>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "IPromptProvider.hpp"
#include "PromptTemplate.hpp"
#include "PromptTemplateManager.hpp"

namespace QodeAssist::PluginLLMCore {

class PromptProviderChat : public IPromptProvider
{
public:
    explicit PromptProviderChat(PromptTemplateManager &templateManager)
        : m_templateManager(templateManager)
    {}

    ~PromptProviderChat() = default;

    PromptTemplate *getTemplateByName(const QString &templateName) const override
    {
        return m_templateManager.getChatTemplateByName(templateName);
    }

    QStringList templatesNames() const override { return m_templateManager.chatTemplatesNames(); }

    QStringList getTemplatesForProvider(ProviderID id) const override
    {
        return m_templateManager.getChatTemplatesForProvider(id);
    }

private:
    PromptTemplateManager &m_templateManager;
};

} // namespace QodeAssist::PluginLLMCore
