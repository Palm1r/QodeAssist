// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include <memory>

#include "QodeAssistConstants.hpp"
#include "QodeAssisttr.h"
#include "settings/PluginUpdater.hpp"
#include "settings/UpdateDialog.hpp"

#include <coreplugin/actionmanager/actioncontainer.h>
#include <coreplugin/actionmanager/actionmanager.h>
#include <coreplugin/actionmanager/command.h>
#include <coreplugin/coreconstants.h>
#include <coreplugin/dialogs/ioptionspage.h>
#include <coreplugin/editormanager/documentmodel.h>
#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/icontext.h>
#include <coreplugin/icore.h>
#include <coreplugin/messagemanager.h>
#include <coreplugin/modemanager.h>
#include <coreplugin/navigationwidget.h>
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
#include "QodeAssistClient.hpp"
#include "UpdateStatusWidget.hpp"
#include "Version.hpp"
#include "chat/ChatEditor.hpp"
#include "chat/ChatEditorFactory.hpp"
#include "chat/ChatOutputPane.h"
#include "chat/NavigationPanel.hpp"
#include "context/DocumentReaderQtCreator.hpp"
#include "logger/RequestPerformanceLogger.hpp"
#include "mcp/McpClientsManager.hpp"
#include "mcp/McpServerManager.hpp"
#include "sources/skills/SkillsManager.hpp"
#include "tools/ToolsRegistration.hpp"
#include "settings/AgentRole.hpp"
#include "settings/ChatAssistantSettings.hpp"
#include "settings/GeneralSettings.hpp"
#include "settings/ProjectSettingsPanel.hpp"
#include "settings/AgentsSettingsPage.hpp"
#include "settings/ProvidersSettingsPage.hpp"
#include "settings/QuickRefactorSettings.hpp"
#include "settings/SettingsConstants.hpp"

#include "ProviderInstanceFactory.hpp"
#include "ProviderLauncher.hpp"
#include "ProviderSecretsStore.hpp"

#include <AgentFactory.hpp>
#include <GenericProvider.hpp>
#include <SessionManager.hpp>
#include "widgets/CustomInstructionsManager.hpp"
#include "widgets/QuickRefactorDialog.hpp"
#include <ChatView/ChatView.hpp>
#include <ChatView/ChatFileManager.hpp>
#include <ChatView/ChatRootView.hpp>
#include <ChatView/ChatWidget.hpp>
#include <ChatView/SessionFileRegistry.hpp>
#include <coreplugin/editormanager/editormanager.h>
#include <QQmlContext>
#include <QUuid>
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
        delete m_chatEditorFactory;
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
                    m_qodeAssistClient->requestCompletions(editor);
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

        m_engine = new QQmlEngine{this};
        m_sessionFileRegistry = new Chat::SessionFileRegistry{this};
        m_skillsManager = new Skills::SkillsManager{this};

        if (Settings::chatAssistantSettings().enableChatInBottomToolBar()) {
            m_chatOutputPane = new Chat::ChatOutputPane{
                m_engine, m_sessionFileRegistry, m_skillsManager};
        }
        if (Settings::chatAssistantSettings().enableChatInNavigationPanel()) {
            m_navigationPanel = new Chat::NavigationPanel{
                m_engine, m_sessionFileRegistry, m_skillsManager};
        }
        m_chatEditorFactory = new Chat::ChatEditorFactory{
            m_engine, m_sessionFileRegistry, m_skillsManager};

        Settings::setupProjectPanel();

        Providers::registerBuiltinProviders();
        m_providerInstanceFactory = new Providers::ProviderInstanceFactory(this);
        m_providerSecretsStore = new Providers::ProviderSecretsStore(this);
        m_providerLauncher = new Providers::ProviderLauncher(this);
        m_providersPageNavigator = new Settings::ProvidersPageNavigator(this);
        m_providersOptionsPage = Settings::createProvidersSettingsPage(
            m_providerInstanceFactory,
            m_providerSecretsStore,
            m_providerLauncher,
            m_providersPageNavigator);

        // Ensure the default agent roles exist on disk before agents load, so a
        // chat agent's `role = "<id>"` resolves to a system prompt even on a fresh
        // install where the Roles settings page was never opened.
        Settings::AgentRolesManager::ensureDefaultRoles();

        m_agentFactory = new AgentFactory(m_providerInstanceFactory, m_providerSecretsStore, this);
        m_sessionManager = new SessionManager(m_agentFactory, this);
        {
            auto &contributors = m_sessionManager->toolContributors();
            contributors.add([](::LLMQore::ToolsManager *tools) {
                Tools::registerQodeAssistTools(tools);
            });
            contributors.add([skills = m_skillsManager](::LLMQore::ToolsManager *tools) {
                if (skills)
                    Tools::registerSkillTool(tools, skills);
            });
            contributors.add([](::LLMQore::ToolsManager *tools) {
                Mcp::McpClientsManager::instance().registerToolsOn(tools);
            });
        }
        m_engine->rootContext()->setContextProperty("agentFactory", m_agentFactory);
        m_engine->rootContext()->setContextProperty("sessionManager", m_sessionManager);
        m_agentsPageNavigator = new Settings::AgentsPageNavigator(this);
        m_agentsOptionsPage = Settings::createAgentsSettingsPage(
            m_agentFactory, m_agentsPageNavigator);

        Settings::generalSettings().setAgentPipelinesContext(
            m_agentFactory, m_agentsPageNavigator);

        m_mcpServerManager = new Mcp::McpServerManager(this);
        m_mcpServerManager->init();

        Mcp::McpClientsManager::instance().init();

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

        ActionBuilder showChatViewAction(this, Constants::QODE_ASSIST_SHOW_CHAT_ACTION);
        const QKeySequence showChatViewShortcut = QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_W);
        showChatViewAction.setDefaultKeySequence(showChatViewShortcut);
        showChatViewAction.setToolTip(Tr::tr("Open QodeAssist Chat as an editor tab"));
        showChatViewAction.setText(Tr::tr("Show QodeAssist Chat"));
        showChatViewAction.setIcon(QCODEASSIST_CHAT_ICON.icon());
        showChatViewAction.addOnTriggered(this, [this] { openChatInEditor(); });
        m_statusWidget->setChatButtonAction(showChatViewAction.contextAction());

        m_chatButtonMenu = new QMenu(m_statusWidget);
        connect(
            m_chatButtonMenu,
            &QMenu::aboutToShow,
            this,
            &QodeAssistPlugin::rebuildChatButtonMenu);
        m_statusWidget->setChatButtonMenu(m_chatButtonMenu);

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

        ActionBuilder openChatWindowAction(this, Constants::QODE_ASSIST_OPEN_CHAT_WINDOW_ACTION);
        openChatWindowAction.setText(Tr::tr("Open QodeAssist Chat in Separate Window"));
        openChatWindowAction.setToolTip(Tr::tr("Open the QodeAssist chat in a separate window"));
        openChatWindowAction.setIcon(QCODEASSIST_CHAT_ICON.icon());
        openChatWindowAction.addOnTriggered(this, [this] { openChatInWindow(); });

        ActionBuilder newChatAction(this, Constants::QODE_ASSIST_NEW_CHAT_ACTION);
        newChatAction.setText(Tr::tr("New QodeAssist Chat"));
        newChatAction.setToolTip(Tr::tr("Open a fresh chat in a new editor tab"));
        newChatAction.setIcon(QCODEASSIST_CHAT_ICON.icon());
        newChatAction.addOnTriggered(this, [this] { openNewChatInEditor(); });

        ActionBuilder sendMessageAction(this, Constants::QODE_ASSIST_CHAT_SEND_MESSAGE);
        sendMessageAction.setContext(Core::Context(Constants::QODE_ASSIST_CHAT_CONTEXT));
        sendMessageAction.setText(Tr::tr("Send QodeAssist Chat Message"));
        sendMessageAction.setToolTip(Tr::tr("Send the current message to the LLM"));
        sendMessageAction.setDefaultKeySequence(QKeySequence(Qt::Key_Return));
        sendMessageAction.addOnTriggered(this, [] {
            if (auto chatWidget = Chat::ChatWidget::focusedInstance())
                chatWidget->sendMessage();
        });

        ActionBuilder clearSessionAction(this, Constants::QODE_ASSIST_CHAT_CLEAR_SESSION);
        clearSessionAction.setContext(Core::Context(Constants::QODE_ASSIST_CHAT_CONTEXT));
        clearSessionAction.setText(Tr::tr("Clear QodeAssist Chat Session"));
        clearSessionAction.setToolTip(Tr::tr("Clear the current chat session"));
        clearSessionAction.setDefaultKeySequence(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_L));
        clearSessionAction.addOnTriggered(this, [] {
            if (auto chatWidget = Chat::ChatWidget::focusedInstance())
                chatWidget->clearSession();
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
            *m_agentFactory,
            *m_sessionManager,
            m_documentReader,
            m_performanceLogger));
        m_qodeAssistClient->setSessionManager(m_sessionManager);
        m_qodeAssistClient->setAgentFactory(m_agentFactory);
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
    void openChatInEditor()
    {
        if (auto existing = findExistingChatEditor()) {
            Core::EditorManager::activateEditor(existing);
            existing->consumePendingChatFile();
            return;
        }

        QString title = Tr::tr("QodeAssist Chat");
        Core::IEditor *editor = Core::EditorManager::openEditorWithContents(
            Constants::QODE_ASSIST_CHAT_EDITOR_ID, &title, {}, QUuid::createUuid().toString());
        if (auto chatEditor = qobject_cast<Chat::ChatEditor *>(editor))
            chatEditor->consumePendingChatFile();
    }

    void openNewChatInEditor()
    {
        QString title = Tr::tr("QodeAssist Chat");
        Core::IEditor *editor = Core::EditorManager::openEditorWithContents(
            Constants::QODE_ASSIST_CHAT_EDITOR_ID, &title, {}, QUuid::createUuid().toString());
        // For the "New Chat" button pending is empty (no-op). For relocate-to-editor it
        // carries the handed-off chat file and gets loaded into the freshly opened tab.
        if (auto chatEditor = qobject_cast<Chat::ChatEditor *>(editor))
            chatEditor->consumePendingChatFile();
    }

    Chat::ChatEditor *findExistingChatEditor() const
    {
        const auto entries = Core::DocumentModel::entries();
        for (auto *entry : entries) {
            if (!entry || !entry->document)
                continue;
            if (entry->document->id() != Constants::QODE_ASSIST_CHAT_EDITOR_ID)
                continue;
            const auto editors = Core::DocumentModel::editorsForDocument(entry->document);
            for (auto *editor : editors) {
                if (auto chatEditor = qobject_cast<Chat::ChatEditor *>(editor))
                    return chatEditor;
            }
        }
        return nullptr;
    }

    void openChatInWindow()
    {
        if (!m_chatView)
            m_chatView.reset(new Chat::ChatView{m_engine, m_sessionFileRegistry, m_skillsManager});

        if (!m_chatView->isVisible())
            m_chatView->show();

        m_chatView->raise();
        m_chatView->requestActivate();

        if (auto rootView = qobject_cast<Chat::ChatRootView *>(m_chatView->rootObject()))
            rootView->consumePendingChatFile();
    }

    void setChatInBottomPaneEnabled(bool enabled)
    {
        if (enabled && !m_chatOutputPane)
            m_chatOutputPane = new Chat::ChatOutputPane{
                m_engine, m_sessionFileRegistry, m_skillsManager};
        else if (!enabled && m_chatOutputPane)
            delete m_chatOutputPane;

        Settings::chatAssistantSettings().enableChatInBottomToolBar.setValue(enabled);
        Settings::chatAssistantSettings().writeSettings();
    }

    void setChatInSidebarEnabled(bool enabled)
    {
        if (enabled && !m_navigationPanel)
            m_navigationPanel = new Chat::NavigationPanel{
                m_engine, m_sessionFileRegistry, m_skillsManager};
        else if (!enabled && m_navigationPanel)
            delete m_navigationPanel;

        Settings::chatAssistantSettings().enableChatInNavigationPanel.setValue(enabled);
        Settings::chatAssistantSettings().writeSettings();
    }

    void rebuildChatButtonMenu()
    {
        if (!m_chatButtonMenu)
            return;

        m_chatButtonMenu->clear();

        QAction *paneAction = m_chatButtonMenu->addAction(Tr::tr("Chat in Bottom Panel"));
        paneAction->setCheckable(true);
        paneAction->setChecked(m_chatOutputPane != nullptr);
        connect(paneAction, &QAction::toggled, this, [this](bool on) {
            setChatInBottomPaneEnabled(on);
        });

        QAction *sidebarAction = m_chatButtonMenu->addAction(Tr::tr("Chat in Sidebar"));
        sidebarAction->setCheckable(true);
        sidebarAction->setChecked(m_navigationPanel != nullptr);
        connect(sidebarAction, &QAction::toggled, this, [this](bool on) {
            setChatInSidebarEnabled(on);
        });

        m_chatButtonMenu->addSeparator();

        if (m_chatView && m_chatView->isVisible()) {
            QAction *editorAction = m_chatButtonMenu->addAction(Tr::tr("Open Chat in Editor"));
            connect(editorAction, &QAction::triggered, this, [this] {
                if (m_chatView)
                    m_chatView->close();
                openChatInEditor();
            });
        } else {
            QAction *windowAction
                = m_chatButtonMenu->addAction(Tr::tr("Open Chat in Separate Window"));
            connect(windowAction, &QAction::triggered, this, [this] { openChatInWindow(); });
        }
    }

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
    Context::DocumentReaderQtCreator m_documentReader;
    RequestPerformanceLogger m_performanceLogger;
    QPointer<Chat::ChatOutputPane> m_chatOutputPane;
    QPointer<Chat::NavigationPanel> m_navigationPanel;
    QPointer<Chat::SessionFileRegistry> m_sessionFileRegistry;
    Chat::ChatEditorFactory *m_chatEditorFactory{nullptr};
    QPointer<QMenu> m_chatButtonMenu;
    QPointer<PluginUpdater> m_updater;
    UpdateStatusWidget *m_statusWidget{nullptr};
    QString m_lastRefactorInstructions;
    std::unique_ptr<Chat::ChatView> m_chatView;
    QPointer<Mcp::McpServerManager> m_mcpServerManager;
    QPointer<QQmlEngine> m_engine;
    QPointer<Skills::SkillsManager> m_skillsManager;
    QPointer<Providers::ProviderInstanceFactory> m_providerInstanceFactory;
    QPointer<Providers::ProviderSecretsStore> m_providerSecretsStore;
    QPointer<Providers::ProviderLauncher> m_providerLauncher;
    QPointer<Settings::ProvidersPageNavigator> m_providersPageNavigator;
    std::unique_ptr<Core::IOptionsPage> m_providersOptionsPage;
    QPointer<AgentFactory> m_agentFactory;
    QPointer<SessionManager> m_sessionManager;
    QPointer<Settings::AgentsPageNavigator> m_agentsPageNavigator;
    std::unique_ptr<Core::IOptionsPage> m_agentsOptionsPage;
};

} // namespace QodeAssist::Internal

#include <qodeassist.moc>
