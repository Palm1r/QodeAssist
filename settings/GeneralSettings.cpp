// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "GeneralSettings.hpp"

#include <coreplugin/dialogs/ioptionspage.h>
#include <coreplugin/icore.h>
#include <utils/infolabel.h>
#include <utils/layoutbuilder.h>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QTimer>
#include <QVBoxLayout>

#include "../Version.hpp"
#include "AgentRosterWidget.hpp"
#include "AgentsSettingsPage.hpp"
#include "Logger.hpp"
#include "PipelinesConfig.hpp"
#include "SettingsConstants.hpp"
#include "SettingsTr.hpp"
#include "SettingsUtils.hpp"
#include "UpdateDialog.hpp"

#include <AgentFactory.hpp>

namespace QodeAssist::Settings {

GeneralSettings &generalSettings()
{
    static GeneralSettings settings;
    return settings;
}

namespace {

constexpr int kSaveDebounceMs = 300;

class AgentPipelinesWidget : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(AgentPipelinesWidget)
public:
    AgentPipelinesWidget(
        const QPointer<AgentFactory> &agentFactory,
        const QPointer<AgentsPageNavigator> &agentsNavigator,
        QWidget *parent = nullptr)
        : QWidget(parent)
        , m_agentFactory(agentFactory)
        , m_agentsNavigator(agentsNavigator)
    {
        m_titleLabel = new QLabel(Tr::tr(TrConstants::AGENT_PIPELINES), this);
        QFont tf = m_titleLabel->font();
        tf.setBold(true);
        tf.setPixelSize(13);
        m_titleLabel->setFont(tf);

        auto *headerRow = new QHBoxLayout;
        headerRow->setContentsMargins(0, 0, 0, 0);
        headerRow->setSpacing(8);
        headerRow->addWidget(m_titleLabel);
        headerRow->addStretch(1);

        auto *headerSep = new QFrame(this);
        headerSep->setFrameShape(QFrame::HLine);
        headerSep->setFrameShadow(QFrame::Sunken);

        m_loadWarning = new Utils::InfoLabel({}, Utils::InfoLabel::Warning, this);
        m_loadWarning->setElideMode(Qt::ElideNone);
        m_loadWarning->setWordWrap(true);
        m_loadWarning->setVisible(false);

        m_completionRoster = new AgentRosterWidget(this);
        m_completionRoster->setSlot(
            Tr::tr(TrConstants::CODE_COMPLETION),
            Tr::tr(TrConstants::SLOT_HINT_CODE_COMPLETION),
            {QStringLiteral("completion")});

        m_chatRoster = new AgentRosterWidget(this);
        m_chatRoster->setSlot(
            Tr::tr(TrConstants::CHAT_ASSISTANT),
            Tr::tr(TrConstants::SLOT_HINT_CHAT_ASSISTANT),
            {QStringLiteral("chat")});
        m_chatRoster->setOrderable(false);

        m_compressionRoster = new AgentRosterWidget(this);
        m_compressionRoster->setSlot(
            Tr::tr(TrConstants::CHAT_COMPRESSION),
            Tr::tr(TrConstants::SLOT_HINT_CHAT_COMPRESSION),
            {QStringLiteral("compression")});
        m_compressionRoster->setSingle(true);

        m_refactorRoster = new AgentRosterWidget(this);
        m_refactorRoster->setSlot(
            Tr::tr(TrConstants::QUICK_REFACTOR),
            Tr::tr(TrConstants::SLOT_HINT_QUICK_REFACTOR),
            {QStringLiteral("refactor")});
        m_refactorRoster->setSingle(true);

        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(12);
        root->addLayout(headerRow);
        root->addWidget(headerSep);
        root->addWidget(m_loadWarning);
        root->addWidget(m_completionRoster);
        root->addWidget(m_chatRoster);
        root->addWidget(m_compressionRoster);
        root->addWidget(m_refactorRoster);

        m_saveDebounce = new QTimer(this);
        m_saveDebounce->setSingleShot(true);
        m_saveDebounce->setInterval(kSaveDebounceMs);
        connect(m_saveDebounce, &QTimer::timeout, this, [this]() { persist(); });

        loadFromSettings();

        for (AgentRosterWidget *roster :
             {m_completionRoster, m_chatRoster, m_compressionRoster, m_refactorRoster}) {
            connect(roster, &AgentRosterWidget::editAgentRequested, this,
                    &AgentPipelinesWidget::onEditAgent);
            connect(roster, &AgentRosterWidget::rosterChanged, this,
                    [this](const QStringList &) { m_saveDebounce->start(); });
        }
    }

    ~AgentPipelinesWidget() override
    {
        if (m_saveDebounce && m_saveDebounce->isActive()) {
            m_saveDebounce->stop();
            persist(/*interactive*/ false);
        }
    }

    void resetToDefaults()
    {
        QString err;
        if (!PipelinesConfig::save(PipelineRosters::defaults(), &err))
            LOG_MESSAGE(QStringLiteral("[Pipelines] failed to reset rosters: %1").arg(err));

        m_saveErrorShown = false;
        loadFromSettings();
    }

private:
    void persist(bool interactive = true)
    {
        PipelineRosters rosters;
        rosters.codeCompletion = m_completionRoster->roster();
        rosters.chatAssistant = m_chatRoster->roster();
        rosters.chatCompression = m_compressionRoster->roster().value(0);
        rosters.quickRefactor = m_refactorRoster->roster().value(0);
        QString err;
        if (!PipelinesConfig::save(rosters, &err)) {
            LOG_MESSAGE(QStringLiteral("[Pipelines] save failed (%1): %2")
                            .arg(PipelinesConfig::filePath(), err));
            if (interactive && !m_saveErrorShown) {
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

    void onEditAgent(const QString &name)
    {
        if (m_agentsNavigator)
            m_agentsNavigator->requestSelectAgent(name);

        showSettings(Constants::QODE_ASSIST_AGENTS_SETTINGS_PAGE_ID);
    }

    void loadFromSettings()
    {
        const PipelinesLoadResult lr = PipelinesConfig::load();
        const bool broken = lr.status == PipelinesLoadStatus::ParseError
                            || lr.status == PipelinesLoadStatus::SchemaError;
        if (broken) {
            m_loadWarning->setText(
                tr("pipelines.toml has issues — using defaults for affected entries:\n%1\n"
                   "Changes you make here will overwrite the file.")
                    .arg(lr.message));
        }
        m_loadWarning->setVisible(broken);

        AgentFactory *factory = m_agentFactory.data();
        const auto asList = [](const QString &name) {
            return name.isEmpty() ? QStringList{} : QStringList{name};
        };
        m_completionRoster->setRoster(lr.rosters.codeCompletion, factory);
        m_chatRoster->setRoster(lr.rosters.chatAssistant, factory);
        m_compressionRoster->setRoster(asList(lr.rosters.chatCompression), factory);
        m_refactorRoster->setRoster(asList(lr.rosters.quickRefactor), factory);
    }

    QPointer<AgentFactory> m_agentFactory;
    QPointer<AgentsPageNavigator> m_agentsNavigator;

    QLabel *m_titleLabel = nullptr;
    Utils::InfoLabel *m_loadWarning = nullptr;

    AgentRosterWidget *m_completionRoster = nullptr;
    AgentRosterWidget *m_chatRoster = nullptr;
    AgentRosterWidget *m_compressionRoster = nullptr;
    AgentRosterWidget *m_refactorRoster = nullptr;

    QTimer *m_saveDebounce = nullptr;
    bool m_saveErrorShown = false;
};

} // namespace

GeneralSettings::GeneralSettings()
{
    setAutoApply(false);

    setDisplayName(TrConstants::GENERAL);

    enableQodeAssist.setSettingsKey(Constants::ENABLE_QODE_ASSIST);
    enableQodeAssist.setLabelText(TrConstants::ENABLE_QODE_ASSIST);
    enableQodeAssist.setDefaultValue(true);

    enableLogging.setSettingsKey(Constants::ENABLE_LOGGING);
    enableLogging.setLabelText(TrConstants::ENABLE_LOG);
    enableLogging.setToolTip(TrConstants::ENABLE_LOG_TOOLTIP);
    enableLogging.setDefaultValue(false);

    enableCheckUpdate.setSettingsKey(Constants::ENABLE_CHECK_UPDATE);
    enableCheckUpdate.setLabelText(TrConstants::ENABLE_CHECK_UPDATE_ON_START);
    enableCheckUpdate.setDefaultValue(true);

    requestTimeout.setSettingsKey(Constants::REQUEST_TIMEOUT);
    requestTimeout.setLabelText(Tr::tr("Request timeout (seconds):"));
    requestTimeout.setToolTip(Tr::tr(
        "Maximum time to wait for the model to send data before a request is aborted. "
        "Applies to all requests — chat, code completion, quick refactor and chat compression. "
        "The timer resets every time data is received, so this effectively limits the "
        "time-to-first-token and any stall between tokens. Increase it for slow or local "
        "models that need a long time to start responding. Set to 0 to disable the timeout."));
    requestTimeout.setRange(0, 3600);
    requestTimeout.setDefaultValue(120);

    resetToDefaults.m_buttonText = TrConstants::RESET_TO_DEFAULTS;
    checkUpdate.m_buttonText = TrConstants::CHECK_UPDATE;

    readSettings();

    Logger::instance().setLoggingEnabled(enableLogging());

    setupConnections();

    setLayouter([this]() {
        using namespace Layouting;

        auto networkGroup = Group{
            title(Tr::tr("Network")),
            Column{Row{requestTimeout, Stretch{1}}}};

        auto *supportLabel = new QLabel(Tr::tr("Support the development of QodeAssist:"));

        auto *supportLinks = new QLabel(
            "<a href='https://ko-fi.com/qodeassist' style='color: #0066cc;'>Support on Ko-fi ☕</a>"
            " &nbsp;|&nbsp; "
            "<a href='https://github.com/Palm1r/"
            "QodeAssist?tab=readme-ov-file#support-the-development-of-qodeassist' "
            "style='color: #0066cc;'>Support page on GitHub</a>"
            " &nbsp;|&nbsp; "
            "<a href='https://www.paypal.com/paypalme/palm1r' style='color: #0066cc;'>Support via "
            "PayPal 💳</a>");
        supportLinks->setOpenExternalLinks(true);
        supportLinks->setTextFormat(Qt::RichText);

        auto *pipelines = new AgentPipelinesWidget(m_agentFactory, m_agentsNavigator);
        m_resetPipelines = [p = QPointer(pipelines)] {
            if (p)
                p->resetToDefaults();
        };

        return Column{
            Row{supportLabel, supportLinks, Stretch{1}, checkUpdate, resetToDefaults},
            Space{8},
            Row{enableQodeAssist, Stretch{1}},
            Row{enableLogging, Stretch{1}},
            Row{enableCheckUpdate, Stretch{1}},
            Space{8},
            networkGroup,
            Space{12},
            pipelines,
            Stretch{1}};
    });
}

void GeneralSettings::setAgentPipelinesContext(
    AgentFactory *agentFactory, AgentsPageNavigator *agentsNavigator)
{
    m_agentFactory = agentFactory;
    m_agentsNavigator = agentsNavigator;
}

void GeneralSettings::setupConnections()
{
    connect(&enableLogging, &Utils::BoolAspect::volatileValueChanged, this, [this]() {
        Logger::instance().setLoggingEnabled(enableLogging.volatileValue());
    });
    connect(&resetToDefaults, &ButtonAspect::clicked, this, &GeneralSettings::resetPageToDefaults);
    connect(&checkUpdate, &ButtonAspect::clicked, this, [this]() {
        QodeAssist::UpdateDialog::checkForUpdatesAndShow(Core::ICore::dialogParent());
    });
}

void GeneralSettings::resetPageToDefaults()
{
    const QMessageBox::StandardButton reply = QMessageBox::question(
        Core::ICore::dialogParent(),
        TrConstants::RESET_SETTINGS,
        TrConstants::CONFIRMATION,
        QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes)
        return;

    resetAspect(enableQodeAssist);
    resetAspect(enableLogging);
    resetAspect(requestTimeout);
    resetAspect(enableCheckUpdate);
    writeSettings();

    if (m_resetPipelines)
        m_resetPipelines();
}

class GeneralSettingsPage : public Core::IOptionsPage
{
public:
    GeneralSettingsPage()
    {
        setId(Constants::QODE_ASSIST_GENERAL_SETTINGS_PAGE_ID);
        setDisplayName(TrConstants::GENERAL);
        setCategory(Constants::QODE_ASSIST_GENERAL_OPTIONS_CATEGORY);
#if QODEASSIST_QT_CREATOR_VERSION < QT_VERSION_CHECK(15, 0, 83)
        setDisplayCategory(Constants::QODE_ASSIST_GENERAL_OPTIONS_DISPLAY_CATEGORY);
        setCategoryIconPath(":/resources/images/qoderassist-icon.png");
#endif
        setSettingsProvider([] { return &generalSettings(); });
    }
};

const GeneralSettingsPage generalSettingsPage;

/*!
    \sa {Core::ICore::showOptionsDialog()}, {Core::ICore::showSettings()}
    \note This function was added to fix Qt Creator API broken changes in v19.0.0-beta2 version
*/
void showSettings(const Utils::Id page)
{
#if QODEASSIST_QT_CREATOR_VERSION >= QT_VERSION_CHECK(18, 0, 83)
    Core::ICore::showSettings(page);
#else
    Core::ICore::showOptionsDialog(page);
#endif
}

/*!
    \sa {Core::ICore::showOptionsDialog()}, {Core::ICore::showSettings()}
    \note This function was added to fix Qt Creator API broken changes in v19.0.0-beta2 version
 */
void showSettings(const Utils::Id page, Utils::Id item)
{
#if QODEASSIST_QT_CREATOR_VERSION >= QT_VERSION_CHECK(18, 0, 83)
    Core::ICore::showSettings(page, item);
#else
    Core::ICore::showOptionsDialog(page, item);
#endif
}

} // namespace QodeAssist::Settings

#include "GeneralSettings.moc"
