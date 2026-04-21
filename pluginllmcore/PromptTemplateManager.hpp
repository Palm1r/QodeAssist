// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QMap>
#include <QString>

#include "PromptTemplate.hpp"

namespace QodeAssist::PluginLLMCore {

class PromptTemplateManager
{
public:
    static PromptTemplateManager &instance();
    ~PromptTemplateManager();

    template<typename T>
    void registerTemplate()
    {
        static_assert(std::is_base_of<PromptTemplate, T>::value, "T must inherit from PromptTemplate");
        T *template_ptr = new T();
        QString name = template_ptr->name();
        m_fimTemplates[name] = template_ptr;
        if (template_ptr->type() == TemplateType::Chat) {
            m_chatTemplates[name] = template_ptr;
        }
    }

    PromptTemplate *getFimTemplateByName(const QString &templateName);
    PromptTemplate *getChatTemplateByName(const QString &templateName);

    QStringList fimTemplatesNames() const;
    QStringList chatTemplatesNames() const;

    QStringList getFimTemplatesForProvider(ProviderID id);
    QStringList getChatTemplatesForProvider(ProviderID id);

private:
    PromptTemplateManager() = default;
    PromptTemplateManager(const PromptTemplateManager &) = delete;
    PromptTemplateManager &operator=(const PromptTemplateManager &) = delete;

    QMap<QString, PromptTemplate *> m_fimTemplates;
    QMap<QString, PromptTemplate *> m_chatTemplates;
};

} // namespace QodeAssist::PluginLLMCore
