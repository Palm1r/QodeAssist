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

#include "QodeAssistConstants.hpp"
#include "QodeAssisttr.h"
#include "settings/PluginUpdater.hpp"
#include "settings/UpdateDialog.hpp"

#include <coreplugin/actionmanager/actioncontainer.h>
#include <coreplugin/actionmanager/actionmanager.h>
#include <coreplugin/actionmanager/command.h>
#include <coreplugin/coreconstants.h>
#include <coreplugin/icontext.h>
#include <coreplugin/icore.h>
#include <coreplugin/messagemanager.h>
#include <coreplugin/modemanager.h>
#include <coreplugin/statusbarmanager.h>
#include <extensionsystem/iplugin.h>
#include <languageclient/languageclientmanager.h>

#include <texteditor/texteditor.h>
#include <utils/icon.h>
#include <QAction>
#include <QMainWindow>
#include <QMenu>
#include <QMessageBox>

#include "ConfigurationManager.hpp"
#include "QodeAssistClient.hpp"
#include "Version.hpp"
#include "chat/ChatOutputPane.h"
#include "chat/NavigationPanel.hpp"
#include "llmcore/PromptProviderFim.hpp"
#include "llmcore/ProvidersManager.hpp"
#include "settings/GeneralSettings.hpp"
#include "settings/ProjectSettingsPanel.hpp"
#include "settings/SettingsConstants.hpp"

#include "UpdateStatusWidget.hpp"
#include "providers/Providers.hpp"
#include "templates/Templates.hpp"

using namespace Utils;
using namespace Core;
using namespace ProjectExplorer;

namespace QodeAssist::Internal {

class QodeAssistPlugin final : public ExtensionSystem::IPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QtCreatorPlugin" FILE "QodeAssist.json")

public:
    QodeAssistPlugin()
        : m_updater(new PluginUpdater(this))
        , m_promptProvider(LLMCore::PromptTemplateManager::instance())
    {}

    ~QodeAssistPlugin() final
    {
        delete m_qodeAssistClient;
        delete m_chatOutputPane;
        delete m_navigationPanel;
    }

    void initialize() final
    {
#if QODEASSIST_QT_CREATOR_VERSION >= QT_VERSION_CHECK(15, 0, 83)
        Core::IOptionsPage::registerCategory(
            Constants::QODE_ASSIST_GENERAL_OPTIONS_CATEGORY,
            Constants::QODE_ASSIST_GENERAL_OPTIONS_DISPLAY_CATEGORY,
            ":/resources/images/qoderassist-icon.png");
#endif

        Providers::registerProviders();
        Templates::registerTemplates();

        Utils::Icon QCODEASSIST_ICON(
            {{":/resources/images/qoderassist-icon.png", Utils::Theme::IconsBaseColor}});

        ActionBuilder requestAction(this, Constants::QODE_ASSIST_REQUEST_SUGGESTION);
        requestAction.setToolTip(
            Tr::tr("Generate Qode Assist suggestion at the current cursor position."));
        requestAction.setText(Tr::tr("Request QodeAssist Suggestion"));
        requestAction.setIcon(QCODEASSIST_ICON.icon());
        const QKeySequence defaultShortcut = QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Q);
        requestAction.setDefaultKeySequence(defaultShortcut);
        requestAction.addOnTriggered(this, [this] {
            if (auto editor = TextEditor::TextEditorWidget::currentTextEditorWidget()) {
                if (m_qodeAssistClient && m_qodeAssistClient->reachable()) {
                    m_qodeAssistClient->requestCompletions(editor);
                } else
                    qWarning() << "The Qode Assist is not ready. Please check your connection and "
                                  "settings.";
            }
        });

        m_statusWidget = new UpdateStatusWidget;
        m_statusWidget->setDefaultAction(requestAction.contextAction());
        StatusBarManager::addStatusBarWidget(m_statusWidget, StatusBarManager::RightCorner);

        connect(m_statusWidget->updateButton(), &QPushButton::clicked, this, [this]() {
            UpdateDialog::checkForUpdatesAndShow(Core::ICore::mainWindow());
        });

        if (Settings::generalSettings().enableChat()) {
            m_chatOutputPane = new Chat::ChatOutputPane(this);
            m_navigationPanel = new Chat::NavigationPanel();
        }

        Settings::setupProjectPanel();
        ConfigurationManager::instance().init();

        if (Settings::generalSettings().enableCheckUpdate()) {
            QTimer::singleShot(3000, this, &QodeAssistPlugin::checkForUpdates);
        }
    }

    void extensionsInitialized() final {}

    void restartClient()
    {
        LanguageClient::LanguageClientManager::shutdownClient(m_qodeAssistClient);
        m_qodeAssistClient
            = new QodeAssistClient(LLMCore::ProvidersManager::instance(), &m_promptProvider);
    }

    bool delayedInitialize() final
    {
        restartClient();
        return true;
    }

    ShutdownFlag aboutToShutdown() final
    {
        if (!m_qodeAssistClient)
            return SynchronousShutdown;
        connect(m_qodeAssistClient, &QObject::destroyed, this, &IPlugin::asynchronousShutdownFinished);
        return AsynchronousShutdown;
    }

private:
    void checkForUpdates()
    {
        connect(
            m_updater,
            &PluginUpdater::updateCheckFinished,
            this,
            &QodeAssistPlugin::handleUpdateCheckResult,
            Qt::UniqueConnection);
        m_updater->checkForUpdates();
    }

    void handleUpdateCheckResult(const PluginUpdater::UpdateInfo &info)
    {
        if (!info.isUpdateAvailable
            || QVersionNumber::fromString(info.currentIdeVersion)
                   > QVersionNumber::fromString(info.targetIdeVersion))
            return;

        if (m_statusWidget)
            m_statusWidget->showUpdateAvailable(info.version);
    }

    QPointer<QodeAssistClient> m_qodeAssistClient;
    LLMCore::PromptProviderFim m_promptProvider;
    QPointer<Chat::ChatOutputPane> m_chatOutputPane;
    QPointer<Chat::NavigationPanel> m_navigationPanel;
    QPointer<PluginUpdater> m_updater;
    UpdateStatusWidget *m_statusWidget{nullptr};
};

} // namespace QodeAssist::Internal

#include <qodeassist.moc>
