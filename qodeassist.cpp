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

#include <QAction>
#include <QMainWindow>
#include <QMenu>
#include <QMessageBox>
#include <texteditor/texteditor.h>
#include <utils/icon.h>

#include "ConfigurationManager.hpp"
#include "QodeAssistClient.hpp"
#include "chat/ChatOutputPane.h"
#include "chat/NavigationPanel.hpp"
#include "settings/ProjectSettingsPanel.hpp"

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
    {
    }

    ~QodeAssistPlugin() final
    {
        delete m_qodeAssistClient;
        delete m_chatOutputPane;
        delete m_navigationPanel;
    }

    void initialize() final
    {
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

        auto toggleButton = new QToolButton;
        toggleButton->setDefaultAction(requestAction.contextAction());
        StatusBarManager::addStatusBarWidget(toggleButton, StatusBarManager::RightCorner);

        m_chatOutputPane = new Chat::ChatOutputPane(this);
        m_navigationPanel = new Chat::NavigationPanel();

        Settings::setupProjectPanel();

        ConfigurationManager::instance().init();
    }

    void extensionsInitialized() final
    {
    }

    void restartClient()
    {
        LanguageClient::LanguageClientManager::shutdownClient(m_qodeAssistClient);

        m_qodeAssistClient = new QodeAssistClient();
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
        connect(m_qodeAssistClient,
                &QObject::destroyed,
                this,
                &IPlugin::asynchronousShutdownFinished);
        return AsynchronousShutdown;
    }

private:
    QPointer<QodeAssistClient> m_qodeAssistClient;
    QPointer<Chat::ChatOutputPane> m_chatOutputPane;
    QPointer<Chat::NavigationPanel> m_navigationPanel;
};

} // namespace QodeAssist::Internal

#include <qodeassist.moc>
