/*
 * Copyright (C) 2025 Petr Mironychev
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

#include <QObject>

#include "llmcore/PromptTemplateManager.hpp"
#include "llmcore/ProvidersManager.hpp"
#include "settings/GeneralSettings.hpp"

namespace QodeAssist {

class ConfigurationManager : public QObject
{
    Q_OBJECT

public:
    static ConfigurationManager &instance();

    void init();

    void updateTemplateDescription(const Utils::StringAspect &templateAspect);
    void updateAllTemplateDescriptions();
    void checkTemplate(const Utils::StringAspect &templateAspect);
    void checkAllTemplate();

public slots:
    void selectProvider();
    void selectModel();
    void selectTemplate();
    void selectUrl();

private:
    explicit ConfigurationManager(QObject *parent = nullptr);
    ~ConfigurationManager() = default;
    ConfigurationManager(const ConfigurationManager &) = delete;
    ConfigurationManager &operator=(const ConfigurationManager &) = delete;

    Settings::GeneralSettings &m_generalSettings;
    LLMCore::ProvidersManager &m_providersManager;
    LLMCore::PromptTemplateManager &m_templateManger;

    void setupConnections();
};

} // namespace QodeAssist
