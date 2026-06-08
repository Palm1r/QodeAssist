// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "AgentPipelinesPage.hpp"

#include <coreplugin/dialogs/ioptionspage.h>
#include <coreplugin/icore.h>

#include <QColor>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>

#include "../../Version.hpp"
#include "AgentRosterWidget.hpp"
#include "AgentsSettingsPage.hpp"
#include "Logger.hpp"
#include "PipelinesConfig.hpp"
#include "SettingsConstants.hpp"
#include "SettingsTr.hpp"

#include <AgentFactory.hpp>

namespace QodeAssist::Settings {

AgentPipelinesPageNavigator::AgentPipelinesPageNavigator(QObject *parent)
    : QObject(parent)
{}

namespace {

constexpr int kSaveDebounceMs = 300;

struct SlotMeta
{
    const char *title;
    const char *hint;
};

const SlotMeta kSlotMeta[] = {
    {TrConstants::CODE_COMPLETION,  TrConstants::SLOT_HINT_CODE_COMPLETION},
    {TrConstants::CHAT_ASSISTANT,   TrConstants::SLOT_HINT_CHAT_ASSISTANT},
    {TrConstants::CHAT_COMPRESSION, TrConstants::SLOT_HINT_CHAT_COMPRESSION},
    {TrConstants::QUICK_REFACTOR,   TrConstants::SLOT_HINT_QUICK_REFACTOR},
};

class AgentPipelinesPageWidget : public Core::IOptionsPageWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(AgentPipelinesPageWidget)
public:
    AgentPipelinesPageWidget(
        const QPointer<AgentFactory> &agentFactory,
        const QPointer<AgentPipelinesPageNavigator> &navigator,
        const QPointer<AgentsPageNavigator> &agentsNavigator)
        : m_agentFactory(agentFactory)
        , m_navigator(navigator)
        , m_agentsNavigator(agentsNavigator)
    {
        m_titleLabel = new QLabel(Tr::tr(TrConstants::AGENT_PIPELINES), this);
        QFont tf = m_titleLabel->font();
        tf.setBold(true);
        tf.setPixelSize(13);
        m_titleLabel->setFont(tf);

        m_resetBtn = new QPushButton(Tr::tr(TrConstants::RESET_TO_DEFAULTS), this);

        auto *headerRow = new QHBoxLayout;
        headerRow->setContentsMargins(0, 0, 0, 0);
        headerRow->setSpacing(8);
        headerRow->addWidget(m_titleLabel);
        headerRow->addStretch(1);
        headerRow->addWidget(m_resetBtn);

        auto *headerSep = new QFrame(this);
        headerSep->setFrameShape(QFrame::HLine);
        headerSep->setFrameShadow(QFrame::Sunken);

        m_rosters[0] = new AgentRosterWidget(this);
        m_rosters[1] = new AgentRosterWidget(this);
        m_rosters[2] = new AgentRosterWidget(this);
        m_rosters[3] = new AgentRosterWidget(this);

        for (int i = 0; i < kRosterCount; ++i)
            m_rosters[i]->setSlot(Tr::tr(kSlotMeta[i].title), Tr::tr(kSlotMeta[i].hint), {});

        auto *content = new QWidget(this);
        auto *contentLay = new QVBoxLayout(content);
        contentLay->setContentsMargins(0, 0, 0, 0);
        contentLay->setSpacing(12);
        for (int i = 0; i < kRosterCount; ++i)
            contentLay->addWidget(m_rosters[i]);
        contentLay->addStretch(1);

        auto *scroll = new QScrollArea(this);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setWidget(content);

        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(8, 8, 8, 8);
        root->setSpacing(6);
        root->addLayout(headerRow);
        root->addWidget(headerSep);
        root->addWidget(scroll, 1);

        m_saveDebounce = new QTimer(this);
        m_saveDebounce->setSingleShot(true);
        m_saveDebounce->setInterval(kSaveDebounceMs);
        connect(m_saveDebounce, &QTimer::timeout, this, [this]() { persistRosters(); });

        loadFromSettings();

        connect(m_resetBtn, &QPushButton::clicked, this, &AgentPipelinesPageWidget::onReset);

        for (int i = 0; i < kRosterCount; ++i) {
            connect(m_rosters[i], &AgentRosterWidget::editAgentRequested, this,
                    &AgentPipelinesPageWidget::onEditAgent);
            connect(m_rosters[i], &AgentRosterWidget::rosterChanged, this,
                    [this](const QStringList &) { m_saveDebounce->start(); });
        }
    }

    ~AgentPipelinesPageWidget() override
    {
        if (m_saveDebounce && m_saveDebounce->isActive()) {
            m_saveDebounce->stop();
            persistRosters();
        }
    }

    void apply() final
    {
        if (m_saveDebounce && m_saveDebounce->isActive())
            m_saveDebounce->stop();
        persistRosters();
    }

private:
    static constexpr int kRosterCount = 4;

    void persistRosters()
    {
        PipelineRosters rosters;
        rosters.codeCompletion = m_rosters[0]->roster();
        rosters.chatAssistant = m_rosters[1]->roster();
        rosters.chatCompression = m_rosters[2]->roster();
        rosters.quickRefactor = m_rosters[3]->roster();
        QString err;
        if (!PipelinesConfig::save(rosters, &err)) {
            LOG_MESSAGE(QStringLiteral("[Pipelines] save failed (%1): %2")
                            .arg(PipelinesConfig::filePath(), err));
            if (!m_saveErrorShown) {
                m_saveErrorShown = true;
                QMessageBox::warning(
                    Core::ICore::dialogParent(),
                    Tr::tr(TrConstants::AGENT_PIPELINES),
                    tr("Failed to save pipelines.toml:\n%1\n\n"
                       "Further save failures will only be logged.")
                        .arg(err));
            }
        } else {
            m_saveErrorShown = false;
        }
    }

    void onReset()
    {
        const auto reply = QMessageBox::question(
            Core::ICore::dialogParent(),
            Tr::tr(TrConstants::RESET_SETTINGS),
            Tr::tr(TrConstants::CONFIRMATION),
            QMessageBox::Yes | QMessageBox::No);

        if (reply != QMessageBox::Yes)
            return;

        QString err;
        if (!PipelinesConfig::save(PipelineRosters::defaults(), &err))
            LOG_MESSAGE(QStringLiteral("[Pipelines] failed to reset rosters: %1").arg(err));

        m_saveErrorShown = false;
        loadFromSettings();
    }

    void onEditAgent(const QString &name)
    {
        if (m_agentsNavigator)
            m_agentsNavigator->requestSelectAgent(name);
        if (m_navigator)
            emit m_navigator->editAgentRequested(name);

#if QODEASSIST_QT_CREATOR_VERSION >= QT_VERSION_CHECK(18, 0, 83)
        Core::ICore::showSettings(Constants::QODE_ASSIST_AGENTS_SETTINGS_PAGE_ID);
#else
        Core::ICore::showOptionsDialog(Constants::QODE_ASSIST_AGENTS_SETTINGS_PAGE_ID);
#endif
    }

    void loadFromSettings()
    {
        const PipelinesLoadResult lr = PipelinesConfig::load();
        if (lr.status == PipelinesLoadStatus::ParseError
            || lr.status == PipelinesLoadStatus::SchemaError) {
            QMessageBox::warning(
                Core::ICore::dialogParent(),
                Tr::tr(TrConstants::AGENT_PIPELINES),
                tr("pipelines.toml has issues — using defaults for affected entries:\n%1\n\n"
                   "Click OK to continue. Changes you make here will overwrite the file.")
                    .arg(lr.message));
        }

        AgentFactory *factory = m_agentFactory.data();
        m_rosters[0]->setRoster(lr.rosters.codeCompletion, factory);
        m_rosters[1]->setRoster(lr.rosters.chatAssistant, factory);
        m_rosters[2]->setRoster(lr.rosters.chatCompression, factory);
        m_rosters[3]->setRoster(lr.rosters.quickRefactor, factory);
    }

    QPointer<AgentFactory> m_agentFactory;
    QPointer<AgentPipelinesPageNavigator> m_navigator;
    QPointer<AgentsPageNavigator> m_agentsNavigator;

    QLabel *m_titleLabel = nullptr;
    QPushButton *m_resetBtn = nullptr;

    AgentRosterWidget *m_rosters[kRosterCount] = {};

    QTimer *m_saveDebounce = nullptr;
    bool m_saveErrorShown = false;
};

class AgentPipelinesOptionsPage final : public Core::IOptionsPage
{
public:
    AgentPipelinesOptionsPage(
        AgentFactory *agentFactory,
        AgentPipelinesPageNavigator *navigator,
        AgentsPageNavigator *agentsNavigator)
    {
        setId(Constants::QODE_ASSIST_AGENT_PIPELINES_PAGE_ID);
        setDisplayName(Tr::tr(TrConstants::AGENT_PIPELINES));
        setCategory(Constants::QODE_ASSIST_GENERAL_OPTIONS_CATEGORY);
        const QPointer<AgentFactory> factoryPtr(agentFactory);
        const QPointer<AgentPipelinesPageNavigator> navPtr(navigator);
        const QPointer<AgentsPageNavigator> agentsNavPtr(agentsNavigator);
        setWidgetCreator([factoryPtr, navPtr, agentsNavPtr] {
            return new AgentPipelinesPageWidget(factoryPtr, navPtr, agentsNavPtr);
        });
    }
};

} // namespace

std::unique_ptr<Core::IOptionsPage> createAgentPipelinesSettingsPage(
    AgentFactory *agentFactory,
    AgentPipelinesPageNavigator *navigator,
    AgentsPageNavigator *agentsNavigator)
{
    return std::make_unique<AgentPipelinesOptionsPage>(
        agentFactory, navigator, agentsNavigator);
}

} // namespace QodeAssist::Settings

#include "AgentPipelinesPage.moc"
