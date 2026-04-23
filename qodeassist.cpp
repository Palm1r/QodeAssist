// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

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
#include <QTranslator>

#include <QInputDialog>
#include "ConfigurationManager.hpp"
#include "QodeAssistClient.hpp"
#include "UpdateStatusWidget.hpp"
#include "Version.hpp"
#include "chat/ChatOutputPane.h"
#include "chat/NavigationPanel.hpp"
#include "context/DocumentReaderQtCreator.hpp"
#include "pluginllmcore/PromptProviderFim.hpp"
#include "pluginllmcore/ProvidersManager.hpp"
#include "logger/RequestPerformanceLogger.hpp"
#include "mcp/McpServerManager.hpp"
#include "providers/Providers.hpp"
#include "settings/ChatAssistantSettings.hpp"
#include "settings/GeneralSettings.hpp"
#include "settings/ProjectSettingsPanel.hpp"
#include "settings/QuickRefactorSettings.hpp"
#include "settings/SettingsConstants.hpp"
#include "templates/Templates.hpp"
#include "widgets/CustomInstructionsManager.hpp"
#include "widgets/QuickRefactorDialog.hpp"
#include <ChatView/ChatView.hpp>
#include <ChatView/ChatFileManager.hpp>
#include <coreplugin/actionmanager/actioncontainer.h>
#include <coreplugin/actionmanager/actionmanager.h>
#include <texteditor/textdocument.h>
#include <texteditor/texteditor.h>
#include <texteditor/texteditorconstants.h>

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
        , m_promptProvider(PluginLLMCore::PromptTemplateManager::instance())
    {}

    ~QodeAssistPlugin() final
    {
        Chat::ChatFileManager::cleanupGlobalIntermediateStorage();
        
        delete m_qodeAssistClient;
        if (m_chatOutputPane) {
            delete m_chatOutputPane;
        }
        if (m_navigationPanel) {
            delete m_navigationPanel;
        }
    }

    void loadTranslations()
    {
        const QString langId = Core::ICore::userInterfaceLanguage();

        QTranslator *translator = new QTranslator(qApp);
        QString resourcePath = QString(":/translations/QodeAssist_%1.qm").arg(langId);

        bool success = translator->load(resourcePath);
        if (success) {
            qApp->installTranslator(translator);
            qDebug() << "Loaded translation from resources:" << resourcePath;
        } else {
            delete translator;
            qDebug() << "No translation found for language:" << langId;
        }
    }

    void initialize() final
    {
#if QODEASSIST_QT_CREATOR_VERSION >= QT_VERSION_CHECK(15, 0, 83)
        Core::IOptionsPage::registerCategory(
            Constants::QODE_ASSIST_GENERAL_OPTIONS_CATEGORY,
            Constants::QODE_ASSIST_GENERAL_OPTIONS_DISPLAY_CATEGORY,
            ":/resources/images/qoderassist-icon.png");
#endif
        QQuickWindow::setSceneGraphBackend(
            Settings::chatAssistantSettings().chatRenderer.stringValue());

        loadTranslations();

        Providers::registerProviders();
        Templates::registerTemplates();
        
        CustomInstructionsManager::instance().loadInstructions();

        Utils::Icon QCODEASSIST_ICON(
            {{":/resources/images/qoderassist-icon.png", Utils::Theme::IconsBaseColor}});
        Utils::Icon QCODEASSIST_CHAT_ICON(
            {{":/resources/images/qode-assist-chat-icon.png", Utils::Theme::IconsBaseColor}});

        ActionBuilder requestAction(this, Constants::QODE_ASSIST_REQUEST_SUGGESTION);
        requestAction.setToolTip(
            Tr::tr("Generate QodeAssist suggestion at the current cursor position."));
        requestAction.setText(Tr::tr("Request QodeAssist Suggestion"));
        requestAction.setIcon(QCODEASSIST_ICON.icon());
        const QKeySequence defaultShortcut = QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Q);
        requestAction.setDefaultKeySequence(defaultShortcut);
        requestAction.addOnTriggered(this, [this] {
            if (auto editor = TextEditor::TextEditorWidget::currentTextEditorWidget()) {
                if (m_qodeAssistClient && m_qodeAssistClient->reachable()) {
                    if (m_qodeAssistClient->isHintVisible()) {
                        m_qodeAssistClient->hideHintAndRequestCompletion(editor);
                    } else {
                        m_qodeAssistClient->requestCompletions(editor);
                    }
                } else
                    qWarning() << "The QodeAssist is not ready. Please check your connection and "
                                  "settings.";
            }
        });

        m_statusWidget = new UpdateStatusWidget;
        m_statusWidget->setDefaultAction(requestAction.contextAction());
        StatusBarManager::addStatusBarWidget(m_statusWidget, StatusBarManager::RightCorner);

        connect(m_statusWidget->updateButton(), &QPushButton::clicked, this, [this]() {
            UpdateDialog::checkForUpdatesAndShow(Core::ICore::mainWindow());
        });

        if (Settings::chatAssistantSettings().enableChatInBottomToolBar()) {
            m_chatOutputPane = new Chat::ChatOutputPane(this);
        }
        if (Settings::chatAssistantSettings().enableChatInNavigationPanel()) {
            m_navigationPanel = new Chat::NavigationPanel();
        }

        Settings::setupProjectPanel();
        ConfigurationManager::instance().init();

        m_mcpServerManager = new Mcp::McpServerManager(this);
        m_mcpServerManager->init();

        if (Settings::generalSettings().enableCheckUpdate()) {
            QTimer::singleShot(3000, this, &QodeAssistPlugin::checkForUpdates);
        }

        ActionBuilder quickRefactorAction(this, "QodeAssist.QuickRefactor");
        const QKeySequence quickRefactorShortcut = QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_R);
        quickRefactorAction.setDefaultKeySequence(quickRefactorShortcut);
        quickRefactorAction.setToolTip(Tr::tr("Refactor code using QodeAssist"));
        quickRefactorAction.setText(Tr::tr("Quick Refactor with QodeAssist"));
        quickRefactorAction.setIcon(QCODEASSIST_ICON.icon());
        quickRefactorAction.addOnTriggered(this, [this] {
            if (auto editor = TextEditor::TextEditorWidget::currentTextEditorWidget()) {
                if (m_qodeAssistClient && m_qodeAssistClient->reachable()) {
                    QuickRefactorDialog
                        dialog(Core::ICore::dialogParent(), m_lastRefactorInstructions);

                    if (dialog.exec() == QDialog::Accepted) {
                        QString instructions = dialog.instructions();
                        if (!instructions.isEmpty()) {
                            m_lastRefactorInstructions = instructions;
                            m_qodeAssistClient->requestQuickRefactor(editor, instructions);
                        }
                    }
                } else {
                    qWarning() << "The QodeAssist is not ready. Please check your connection and "
                                  "settings.";
                }
            }
        });

        ActionBuilder showChatViewAction(this, "QodeAssist.ShowChatView");
        const QKeySequence showChatViewShortcut = QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_W);
        showChatViewAction.setDefaultKeySequence(showChatViewShortcut);
        showChatViewAction.setToolTip(Tr::tr("Show QodeAssist Chat"));
        showChatViewAction.setText(Tr::tr("Show QodeAssist Chat"));
        showChatViewAction.setIcon(QCODEASSIST_CHAT_ICON.icon());
        showChatViewAction.addOnTriggered(this, [this] {
            if (!m_chatView) {
                m_chatView.reset(new Chat::ChatView());
            }

            if (!m_chatView->isVisible()) {
                m_chatView->show();
            }

            m_chatView->raise();
            m_chatView->requestActivate();
        });
        m_statusWidget->setChatButtonAction(showChatViewAction.contextAction());

        ActionBuilder closeChatViewAction(this, "QodeAssist.CloseChatView");
        const QKeySequence closeChatViewShortcut = QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_S);
        closeChatViewAction.setDefaultKeySequence(closeChatViewShortcut);
        closeChatViewAction.setToolTip(Tr::tr("Close QodeAssist Chat"));
        closeChatViewAction.setText(Tr::tr("Close QodeAssist Chat"));
        closeChatViewAction.setIcon(QCODEASSIST_CHAT_ICON.icon());
        closeChatViewAction.addOnTriggered(this, [this] {
            if (m_chatView && m_chatView->isActive() && m_chatView->isVisible()) {
                m_chatView->close();
            }
        });

        Core::ActionContainer *editorContextMenu = Core::ActionManager::actionContainer(
            TextEditor::Constants::M_STANDARDCONTEXTMENU);
        if (editorContextMenu) {
            editorContextMenu->addSeparator(Core::Context(TextEditor::Constants::C_TEXTEDITOR));
            editorContextMenu
                ->addAction(quickRefactorAction.command(), Core::Constants::G_DEFAULT_THREE);
            editorContextMenu->addAction(requestAction.command(), Core::Constants::G_DEFAULT_THREE);
            editorContextMenu->addAction(showChatViewAction.command(),
                                         Core::Constants::G_DEFAULT_THREE);
        }

        Chat::ChatFileManager::cleanupGlobalIntermediateStorage();
    }

    void extensionsInitialized() final {}

    void restartClient()
    {
        LanguageClient::LanguageClientManager::shutdownClient(m_qodeAssistClient);
        m_qodeAssistClient = new QodeAssistClient(new LLMClientInterface(
            Settings::generalSettings(),
            Settings::codeCompletionSettings(),
            PluginLLMCore::ProvidersManager::instance(),
            &m_promptProvider,
            m_documentReader,
            m_performanceLogger));
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
        if (!info.isUpdateAvailable)
            return;

        if (m_statusWidget)
            m_statusWidget->showUpdateAvailable(info.version);
    }

    QPointer<QodeAssistClient> m_qodeAssistClient;
    PluginLLMCore::PromptProviderFim m_promptProvider;
    Context::DocumentReaderQtCreator m_documentReader;
    RequestPerformanceLogger m_performanceLogger;
    QPointer<Chat::ChatOutputPane> m_chatOutputPane;
    QPointer<Chat::NavigationPanel> m_navigationPanel;
    QPointer<PluginUpdater> m_updater;
    UpdateStatusWidget *m_statusWidget{nullptr};
    QString m_lastRefactorInstructions;
    QScopedPointer<Chat::ChatView> m_chatView;
    QPointer<Mcp::McpServerManager> m_mcpServerManager;
};

} // namespace QodeAssist::Internal

#include <qodeassist.moc>
