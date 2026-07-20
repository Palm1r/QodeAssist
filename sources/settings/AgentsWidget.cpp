// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "AgentsWidget.hpp"

#include <QDesktopServices>
#include <QDir>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTextBrowser>
#include <QUrl>
#include <QVBoxLayout>

#include <coreplugin/dialogs/ioptionspage.h>
#include <projectexplorer/project.h>
#include <projectexplorer/projectmanager.h>

#include "AgentsSettings.hpp"
#include "SettingsConstants.hpp"
#include "SettingsTr.hpp"
#include "acp/AgentCatalogStore.hpp"
#include "acp/AgentTester.hpp"

namespace QodeAssist::Settings {

namespace {

constexpr int agentIdRole = Qt::UserRole;

QString distributionSummary(const Acp::AgentDefinition &agent)
{
    const QString kind = Acp::agentDistributionName(agent.distribution.kind);
    switch (agent.distribution.kind) {
    case Acp::AgentDistributionKind::Npx:
    case Acp::AgentDistributionKind::Uvx:
        return QStringLiteral("%1 (%2)").arg(kind, agent.distribution.package);
    case Acp::AgentDistributionKind::Command:
        return QStringLiteral("%1 (%2)").arg(kind, agent.distribution.command);
    case Acp::AgentDistributionKind::Binary:
    case Acp::AgentDistributionKind::Unknown:
        return kind;
    }
    return kind;
}

QString linkRow(const QString &label, const QString &url)
{
    if (url.isEmpty())
        return {};
    return QStringLiteral("<b>%1</b> <a href=\"%2\">%2</a><br>")
        .arg(label.toHtmlEscaped(), url.toHtmlEscaped());
}

QString unavailableGuidance(const Acp::AgentDefinition &agent)
{
    if (agent.distribution.kind == Acp::AgentDistributionKind::Binary) {
        QString text = Tr::tr(
            "This agent ships as a platform binary. QodeAssist does not download binaries yet: "
            "install it manually, then add a JSON file to the agents folder with a "
            "\"command\" distribution pointing at the executable.");

        if (const Acp::AgentBinaryTarget *target = Acp::binaryTargetForCurrentPlatform(agent)) {
            text += QStringLiteral("<br><br>")
                    + Tr::tr("Download for %1:").arg(target->platform.toHtmlEscaped())
                    + QStringLiteral(" <a href=\"%1\">%1</a>").arg(target->archive.toHtmlEscaped());
        }
        return text;
    }

    return Tr::tr("This agent has no distribution QodeAssist can launch.");
}

} // namespace

AgentsWidget::AgentsWidget(Acp::AgentCatalogStore *store)
    : m_store(store)
    , m_tester(new Acp::AgentTester(this))
{
    setupUI();

    connect(m_store, &Acp::AgentCatalogStore::catalogChanged, this, &AgentsWidget::populateList);
    connect(
        m_store,
        &Acp::AgentCatalogStore::refreshFinished,
        this,
        [this](bool ok, const QString &error) {
            showStatus(
                ok ? Tr::tr("Agent list refreshed from the registry.")
                   : Tr::tr("Registry refresh failed: %1").arg(error));
            updateButtons();
        });

    connect(m_tester, &Acp::AgentTester::finished, this, [this](bool ok, const QString &report) {
        showStatus(ok ? Tr::tr("Agent responded.") : Tr::tr("Agent test failed."));
        m_details->append(
            QStringLiteral("<hr><b>%1</b><pre>%2</pre>")
                .arg(ok ? Tr::tr("Test result") : Tr::tr("Test failed"), report.toHtmlEscaped()));
        updateButtons();
    });

    populateList();
}

void AgentsWidget::apply()
{
    auto &settings = agentsSettings();
    settings.agentExtraPaths.setValue(m_extraPathsEdit->text());
    settings.agentForwardedVariables.setValue(m_forwardedVariablesEdit->text());
    settings.writeSettings();
}

QLineEdit *AgentsWidget::addSettingRow(QVBoxLayout *layout, Utils::StringAspect &aspect)
{
    auto *row = new QHBoxLayout();
    auto *label = new QLabel(aspect.labelText(), this);
    label->setToolTip(aspect.toolTip());

    auto *edit = new QLineEdit(aspect.volatileValue(), this);
    edit->setToolTip(aspect.toolTip());
    connect(edit, &QLineEdit::textChanged, &aspect, [&aspect](const QString &text) {
        aspect.setVolatileValue(text);
    });

    row->addWidget(label);
    row->addWidget(edit, 1);
    layout->addLayout(row);

    return edit;
}

void AgentsWidget::setupUI()
{
    auto *mainLayout = new QVBoxLayout(this);

    auto *headerLayout = new QHBoxLayout();
    auto *info = new QLabel(
        Tr::tr(
            "ACP agents from your own JSON files, the agent registry, and the bundled "
            "snapshot. Definitions with the same id override each other in that order."),
        this);
    info->setWordWrap(true);
    headerLayout->addWidget(info, 1);

    auto *openFolderButton = new QPushButton(Tr::tr("Open Agents Folder..."), this);
    connect(openFolderButton, &QPushButton::clicked, this, &AgentsWidget::onOpenAgentsFolder);
    headerLayout->addWidget(openFolderButton);
    mainLayout->addLayout(headerLayout);

    m_extraPathsEdit = addSettingRow(mainLayout, agentsSettings().agentExtraPaths);
    m_forwardedVariablesEdit = addSettingRow(mainLayout, agentsSettings().agentForwardedVariables);

    auto *contentLayout = new QHBoxLayout();

    m_agentsList = new QListWidget(this);
    m_agentsList->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(m_agentsList, &QListWidget::itemSelectionChanged, this, &AgentsWidget::showSelectedAgent);
    contentLayout->addWidget(m_agentsList, 1);

    auto *buttonsLayout = new QVBoxLayout();

    m_testButton = new QPushButton(Tr::tr("Test"), this);
    m_testButton->setToolTip(
        Tr::tr("Start the agent, run the ACP handshake and report its capabilities."));
    connect(m_testButton, &QPushButton::clicked, this, &AgentsWidget::onTest);
    buttonsLayout->addWidget(m_testButton);

    m_reloadButton = new QPushButton(Tr::tr("Reload"), this);
    m_reloadButton->setToolTip(Tr::tr("Re-read the agent JSON files and the cached registry."));
    connect(m_reloadButton, &QPushButton::clicked, this, [this]() {
        m_store->reload();
        showStatus(Tr::tr("Agent list reloaded from disk."));
    });
    buttonsLayout->addWidget(m_reloadButton);

    m_refreshButton = new QPushButton(Tr::tr("Refresh from Registry"), this);
    m_refreshButton->setToolTip(Tr::tr("Download the agent registry and update the local cache."));
    connect(m_refreshButton, &QPushButton::clicked, this, &AgentsWidget::onRefresh);
    buttonsLayout->addWidget(m_refreshButton);

    buttonsLayout->addStretch();
    contentLayout->addLayout(buttonsLayout);
    mainLayout->addLayout(contentLayout, 1);

    m_details = new QTextBrowser(this);
    m_details->setOpenExternalLinks(true);
    m_details->setMinimumHeight(160);
    mainLayout->addWidget(m_details, 1);

    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    mainLayout->addWidget(m_status);
}

void AgentsWidget::populateList()
{
    const QString selectedId = m_agentsList->currentItem()
                                   ? m_agentsList->currentItem()->data(agentIdRole).toString()
                                   : QString();

    m_agentsList->clear();

    const QList<Acp::AgentDefinition> agents = m_store->catalog().agents();
    for (const Acp::AgentDefinition &agent : agents) {
        const QString label = agent.version.isEmpty()
                                  ? agent.name
                                  : QStringLiteral("%1 %2").arg(agent.name, agent.version);
        auto *item = new QListWidgetItem(label, m_agentsList);
        item->setData(agentIdRole, agent.id);

        if (agent.isLaunchable()) {
            item->setToolTip(
                QStringLiteral("%1 — %2")
                    .arg(distributionSummary(agent), Acp::agentSourceName(agent.source)));
        } else {
            item->setForeground(palette().brush(QPalette::Disabled, QPalette::Text));
            item->setToolTip(unavailableGuidance(agent));
        }

        if (agent.id == selectedId)
            m_agentsList->setCurrentItem(item);
    }

    showStatus(
        m_store->hasCachedRegistry()
            ? QString()
            : Tr::tr(
                  "Showing the bundled agent snapshot — press Refresh from Registry for the "
                  "published list."));
    showSelectedAgent();
}

void AgentsWidget::showStatus(const QString &message)
{
    const QStringList warnings = m_store->warnings();
    if (warnings.isEmpty()) {
        m_status->setText(message);
        return;
    }

    const QString ignored = Tr::tr("Ignored entries: %1").arg(warnings.join(QStringLiteral("; ")));
    m_status->setText(message.isEmpty() ? ignored : QStringLiteral("%1 %2").arg(message, ignored));
}

void AgentsWidget::showSelectedAgent()
{
    updateButtons();

    const auto agent = selectedAgent();
    if (!agent) {
        m_details->clear();
        return;
    }

    QString html = QStringLiteral("<h3>%1</h3>").arg(agent->name.toHtmlEscaped());
    if (!agent->description.isEmpty())
        html += QStringLiteral("<p>%1</p>").arg(agent->description.toHtmlEscaped());

    html += QStringLiteral("<p>");
    html += QStringLiteral("<b>%1</b> %2<br>").arg(Tr::tr("Id:"), agent->id.toHtmlEscaped());
    html += QStringLiteral("<b>%1</b> %2<br>")
                .arg(Tr::tr("Distribution:"), distributionSummary(*agent).toHtmlEscaped());
    html += QStringLiteral("<b>%1</b> %2<br>")
                .arg(Tr::tr("Defined by:"), agent->origin.toHtmlEscaped());
    if (!agent->license.isEmpty())
        html += QStringLiteral("<b>%1</b> %2<br>")
                    .arg(Tr::tr("License:"), agent->license.toHtmlEscaped());
    if (!agent->authors.isEmpty())
        html += QStringLiteral("<b>%1</b> %2<br>")
                    .arg(
                        Tr::tr("Authors:"),
                        agent->authors.join(QStringLiteral(", ")).toHtmlEscaped());
    html += linkRow(Tr::tr("Repository:"), agent->repository);
    html += linkRow(Tr::tr("Website:"), agent->website);
    html += QStringLiteral("</p>");

    if (!agent->isLaunchable())
        html += QStringLiteral("<p>%1</p>").arg(unavailableGuidance(*agent));

    m_details->setHtml(html);
}

void AgentsWidget::updateButtons()
{
    const auto agent = selectedAgent();
    const bool testing = m_tester->isRunning();
    m_testButton->setText(testing ? Tr::tr("Cancel Test") : Tr::tr("Test"));
    m_testButton->setEnabled(testing || (agent && agent->isLaunchable()));
    m_refreshButton->setEnabled(!m_store->isRefreshing());
    m_refreshButton->setText(
        m_store->isRefreshing() ? Tr::tr("Refreshing...") : Tr::tr("Refresh from Registry"));
}

void AgentsWidget::onTest()
{
    if (m_tester->isRunning()) {
        m_tester->cancel();
        return;
    }

    const auto agent = selectedAgent();
    if (!agent)
        return;

    showStatus(Tr::tr("Starting %1...").arg(agent->name));
    m_tester->start(*agent, testWorkingDirectory());
    updateButtons();
}

void AgentsWidget::onRefresh()
{
    showStatus(Tr::tr("Downloading the agent registry..."));
    m_store->refreshFromRegistry();
    updateButtons();
}

void AgentsWidget::onOpenAgentsFolder()
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(Acp::AgentCatalogStore::userAgentsDirectory()));
}

std::optional<Acp::AgentDefinition> AgentsWidget::selectedAgent() const
{
    const QListWidgetItem *item = m_agentsList->currentItem();
    if (!item)
        return std::nullopt;
    return m_store->catalog().agent(item->data(agentIdRole).toString());
}

QString AgentsWidget::testWorkingDirectory()
{
    if (const ProjectExplorer::Project *project = ProjectExplorer::ProjectManager::startupProject())
        return project->projectDirectory().path();
    return QDir::homePath();
}

AgentsSettingsPage::AgentsSettingsPage(Acp::AgentCatalogStore *store)
{
    setId(Constants::QODE_ASSIST_AGENTS_SETTINGS_PAGE_ID);
    setDisplayName(Tr::tr("Agents"));
    setCategory(Constants::QODE_ASSIST_GENERAL_OPTIONS_CATEGORY);
    setWidgetCreator([store]() { return new AgentsWidget(store); });
}

} // namespace QodeAssist::Settings
