// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "PromptTemplateManager.hpp"

#include <QMessageBox>

namespace QodeAssist::PluginLLMCore {

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

} // namespace QodeAssist::PluginLLMCore
