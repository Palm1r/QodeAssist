#pragma once

#include <projectexplorer/project.h>
#include <projectexplorer/projectpanelfactory.h>
#include <projectexplorer/projectsettingswidget.h>
#include <utils/layoutbuilder.h>

#include "QodeAssistConstants.hpp"
#include "QodeAssisttr.h"
#include "settings/GeneralSettings.hpp"

namespace QodeAssist {

class QodeAssistProjectSettingsWidget final : public ProjectExplorer::ProjectSettingsWidget
{
    Q_OBJECT

public:
    explicit QodeAssistProjectSettingsWidget(ProjectExplorer::Project *project)
        : m_settings(new Settings::QodeAssistProjectSettings(project))
    {
        m_settings->setParent(this);

        setGlobalSettingsId(Constants::QODE_ASSIST_PROJECT_SETTINGS_OPTIONS_ID);
        setUseGlobalSettingsCheckBoxVisible(true);

        setupConnections();
        initializeLayout();

        setUseGlobalSettings(m_settings->useGlobalSettings());
        setEnabled(!m_settings->useGlobalSettings());
    }

private:
    void setupConnections()
    {
        connect(this,
                &ProjectSettingsWidget::useGlobalSettingsChanged,
                m_settings,
                &Settings::QodeAssistProjectSettings::setUseGlobalSettings);

        connect(this,
                &ProjectSettingsWidget::useGlobalSettingsChanged,
                this,
                [this](bool useGlobal) { setEnabled(!useGlobal); });
    }

    void initializeLayout()
    {
        using namespace Layouting;
        Column{m_settings->enableQodeAssist, Row{m_settings->embeddingStoragePath}}.attachTo(this);
    }

    Settings::QodeAssistProjectSettings *m_settings;
};

class QodeAssistProjectPanel final : public ProjectExplorer::ProjectPanelFactory
{
public:
    QodeAssistProjectPanel()
    {
        setPriority(1000);
        setDisplayName(Tr::tr("QodeAsssist"));
        setCreateWidgetFunction([](ProjectExplorer::Project *project) {
            return new QodeAssistProjectSettingsWidget(project);
        });
    }
};

inline void initQodeAssistProjectSettings()
{
    static QodeAssistProjectPanel panel;
}

} // namespace QodeAssist
