// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QObject>

#include "templates/PromptTemplateManager.hpp"
#include "providers/ProvidersManager.hpp"
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
    Providers::ProvidersManager &m_providersManager;
    Templates::PromptTemplateManager &m_templateManger;

    void setupConnections();
};

} // namespace QodeAssist
