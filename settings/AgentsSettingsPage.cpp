// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "AgentsSettingsPage.hpp"

#include "AgentDetailPane.hpp"
#include "AgentDuplicator.hpp"
#include "AgentListPane.hpp"
#include "SettingsConstants.hpp"
#include "SettingsTheme.hpp"

#include <coreplugin/dialogs/ioptionspage.h>
#include <coreplugin/editormanager/editormanager.h>

#include <utils/filepath.h>
#include <utils/theme/theme.h>

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFont>
#include <QFontMetrics>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPalette>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <Agent.hpp>
#include <AgentFactory.hpp>

namespace QodeAssist::Settings {

AgentsPageNavigator::AgentsPageNavigator(QObject *parent)
    : QObject(parent)
{}

void AgentsPageNavigator::requestSelectAgent(const QString &name)
{
    m_pending = name;
    emit selectAgentRequested(name);
}

QString AgentsPageNavigator::takePendingSelection()
{
    QString p = m_pending;
    m_pending.clear();
    return p;
}

namespace {

class AgentsWidget : public Core::IOptionsPageWidget
{
    Q_OBJECT
public:
    explicit AgentsWidget(AgentFactory *agentFactory, AgentsPageNavigator *navigator)
        : m_agentFactory(agentFactory)
        , m_navigator(navigator)
    {
        Q_ASSERT(m_agentFactory);

        m_titleLabel = new QLabel(tr("Agents"), this);
        QFont tf = m_titleLabel->font();
        tf.setBold(true);
        tf.setPixelSize(13);
        m_titleLabel->setFont(tf);

        m_reload = new QPushButton(tr("Reload from disk"), this);
        m_openUserDir = new QPushButton(tr("Open agents folder"), this);

        m_userPathLabel = new QLabel(this);
        m_userPathLabel->setFont(monospaceFont(11));
        QPalette mutedPal = m_userPathLabel->palette();
        mutedPal.setColor(QPalette::WindowText, Utils::creatorColor(Utils::Theme::PanelTextColorMid));
        m_userPathLabel->setPalette(mutedPal);
        m_userPathLabel->setMaximumWidth(260);

        auto *headerRow = new QHBoxLayout;
        headerRow->setContentsMargins(0, 0, 0, 0);
        headerRow->setSpacing(8);
        headerRow->addWidget(m_titleLabel);
        headerRow->addStretch(1);
        headerRow->addWidget(m_reload);
        headerRow->addWidget(m_userPathLabel);
        headerRow->addWidget(m_openUserDir);

        auto *headerSep = new QFrame(this);
        headerSep->setFrameShape(QFrame::HLine);
        headerSep->setFrameShadow(QFrame::Sunken);

        m_listPane = new AgentListPane(m_agentFactory, this);

        m_detail = new AgentDetailPane(this);
        m_detail->setInstanceFactory(m_agentFactory->instanceFactory());
        m_detail->setAgentFactory(m_agentFactory);
        m_detailScroll = new QScrollArea(this);
        m_detailScroll->setWidgetResizable(true);
        m_detailScroll->setFrameShape(QFrame::StyledPanel);
        m_detailScroll->setWidget(m_detail);

        auto *splitter = new QSplitter(Qt::Horizontal, this);
        splitter->addWidget(m_listPane);
        splitter->addWidget(m_detailScroll);
        splitter->setStretchFactor(0, 0);
        splitter->setStretchFactor(1, 1);
        splitter->setSizes({320, 700});

        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(8, 8, 8, 8);
        root->setSpacing(6);
        root->addLayout(headerRow);
        root->addWidget(headerSep);
        root->addWidget(splitter, 1);

        connect(m_reload, &QPushButton::clicked, this, &AgentsWidget::reloadFromDisk);
        connect(m_openUserDir, &QPushButton::clicked, this, [] {
            const QString dir = QodeAssist::AgentFactory::userAgentsDir();
            QDir().mkpath(dir);
            QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
        });

        connect(m_listPane, &AgentListPane::currentAgentChanged, this, [this](const QString &name) {
            if (const AgentConfig *cfg = m_agentFactory->configByName(name))
                m_detail->setAgent(*cfg);
            else
                m_detail->clear();
        });

        connect(
            m_detail,
            &AgentDetailPane::openInEditorRequested,
            this,
            &AgentsWidget::openAgentInEditor);
        connect(m_detail, &AgentDetailPane::customizeRequested, this, &AgentsWidget::customizeAgent);
        connect(m_detail, &AgentDetailPane::deleteRequested, this, &AgentsWidget::deleteAgent);

        if (m_navigator) {
            connect(
                m_navigator,
                &AgentsPageNavigator::selectAgentRequested,
                m_listPane,
                &AgentListPane::selectByName);
        }

        reloadFromDisk();

        if (m_navigator) {
            QTimer::singleShot(0, this, [this] {
                if (!m_navigator)
                    return;
                const QString pending = m_navigator->takePendingSelection();
                if (!pending.isEmpty())
                    m_listPane->selectByName(pending);
            });
        }
    }

    void apply() final {}

private:
    void reloadFromDisk()
    {
        m_agentFactory->reload();
        updateUserPathLabel();
        m_listPane->refresh();
    }

    void updateUserPathLabel()
    {
        const QString dir = QodeAssist::AgentFactory::userAgentsDir();
        m_userPathLabel->setText(
            QFontMetrics(m_userPathLabel->font()).elidedText(dir, Qt::ElideLeft, 256));
        m_userPathLabel->setToolTip(dir);
    }

    void openAgentInEditor(const AgentConfig &agent)
    {
        const QString name = agent.name;
        const QString sourcePath = agent.sourcePath;
        const bool isUser = agent.isUserSource();

        if (!isUser) {
            QMessageBox::information(
                this,
                tr("Open agent"),
                tr("'%1' is bundled with the plugin and read-only.\n"
                   "Use Duplicate to create an editable user copy.")
                    .arg(name));
            return;
        }
        if (sourcePath.isEmpty() || sourcePath.startsWith(QLatin1String(":/"))) {
            QMessageBox::warning(
                this, tr("Open agent"), tr("Agent '%1' has no editable source file.").arg(name));
            return;
        }
        if (!Core::EditorManager::openEditor(Utils::FilePath::fromString(sourcePath))) {
            QMessageBox::warning(this, tr("Open agent"), tr("Could not open %1.").arg(sourcePath));
        }
    }

    void customizeAgent(const AgentConfig &parent)
    {
        const AgentDuplicateResult res = duplicateAgentInUserDir(parent, *m_agentFactory);
        if (!res.ok) {
            QMessageBox::warning(this, tr("Duplicate"), res.error);
            return;
        }
        const QString newName = res.newName;
        reloadFromDisk();
        m_listPane->selectByName(newName);
    }

    void deleteAgent(const AgentConfig &agent)
    {
        if (!agent.isUserSource())
            return;
        const QString name = agent.name;
        const QString sourcePath = agent.sourcePath;

        if (QMessageBox::question(
                this,
                tr("Delete Agent"),
                tr("Delete agent '%1'?\n\nThis will remove the file:\n%2").arg(name, sourcePath),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No)
            != QMessageBox::Yes)
            return;
        if (!QFile::remove(sourcePath)) {
            QMessageBox::warning(
                this,
                tr("Delete Agent"),
                tr("Could not delete the agent file:\n%1").arg(sourcePath));
            return;
        }
        m_agentFactory->clearAgentModelOverride(name);
        m_agentFactory->clearAgentProviderOverride(name);
        reloadFromDisk();
    }

    AgentFactory *m_agentFactory;
    QPointer<AgentsPageNavigator> m_navigator;

    QLabel *m_titleLabel = nullptr;
    QPushButton *m_reload = nullptr;
    QPushButton *m_openUserDir = nullptr;
    QLabel *m_userPathLabel = nullptr;

    AgentListPane *m_listPane = nullptr;
    QScrollArea *m_detailScroll = nullptr;
    AgentDetailPane *m_detail = nullptr;
};

class AgentsSettingsPage : public Core::IOptionsPage
{
public:
    AgentsSettingsPage(AgentFactory *agentFactory, AgentsPageNavigator *navigator)
    {
        setId(Constants::QODE_ASSIST_AGENTS_SETTINGS_PAGE_ID);
        setDisplayName(QObject::tr("Agents"));
        setCategory(Constants::QODE_ASSIST_GENERAL_OPTIONS_CATEGORY);
        setWidgetCreator(
            [agentFactory, navigator]() { return new AgentsWidget(agentFactory, navigator); });
    }
};

} // namespace

std::unique_ptr<Core::IOptionsPage> createAgentsSettingsPage(
    AgentFactory *agentFactory, AgentsPageNavigator *navigator)
{
    return std::make_unique<AgentsSettingsPage>(agentFactory, navigator);
}

} // namespace QodeAssist::Settings

#include "AgentsSettingsPage.moc"
