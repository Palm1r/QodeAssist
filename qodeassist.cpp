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

#include "LLMProvidersManager.hpp"
#include "PromptTemplateManager.hpp"
#include "QodeAssistClient.hpp"
#include "chat/ChatOutputPane.h"
#include "chat/NavigationPanel.hpp"
#include "providers/LMStudioProvider.hpp"
#include "providers/OllamaProvider.hpp"
#include "providers/OpenAICompatProvider.hpp"

#include "settings/GeneralSettings.hpp"
#include "templates/CodeLlamaFimTemplate.hpp"
#include "templates/CodeLlamaInstruct.hpp"
#include "templates/CustomTemplate.hpp"
#include "templates/DeepSeekCoderChatTemplate.hpp"
#include "templates/DeepSeekCoderV2.hpp"
#include "templates/StarCoder2Template.hpp"

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

    }

    void initialize() final
    {
        auto &providerManager = LLMProvidersManager::instance();
        providerManager.registerProvider<Providers::OllamaProvider>();
        providerManager.registerProvider<Providers::LMStudioProvider>();
        providerManager.registerProvider<Providers::OpenAICompatProvider>();

        auto &templateManager = PromptTemplateManager::instance();
        templateManager.registerTemplate<Templates::CodeLlamaFimTemplate>();
        templateManager.registerTemplate<Templates::StarCoder2Template>();
        templateManager.registerTemplate<Templates::DeepSeekCoderV2Template>();
        templateManager.registerTemplate<Templates::CustomTemplate>();
        templateManager.registerTemplate<Templates::DeepSeekCoderChatTemplate>();
        templateManager.registerTemplate<Templates::CodeLlamaInstructTemplate>();

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
        m_navigationPanel.reset(new Chat::NavigationPanel());
    }

    void extensionsInitialized() final
    {
    }

    void restartClient()
    {
        LanguageClient::LanguageClientManager::shutdownClient(m_qodeAssistClient.get());

        m_qodeAssistClient.reset(new QodeAssistClient());
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
        connect(m_qodeAssistClient.get(),
                &QObject::destroyed,
                this,
                &IPlugin::asynchronousShutdownFinished);
        return AsynchronousShutdown;
    }

private:
    QScopedPointer<QodeAssistClient> m_qodeAssistClient;
    QPointer<Chat::ChatOutputPane> m_chatOutputPane;
    QScopedPointer<Chat::NavigationPanel> m_navigationPanel;
};

} // namespace QodeAssist::Internal

#include <qodeassist.moc>
