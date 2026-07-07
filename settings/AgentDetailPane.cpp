// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "AgentDetailPane.hpp"

#include "ModelSelector.hpp"
#include "SectionBox.hpp"
#include "SettingsTheme.hpp"
#include "SettingsUiBuilders.hpp"

#include <utils/theme/theme.h>

#include <AgentFactory.hpp>
#include <ProviderInstance.hpp>
#include <ProviderInstanceFactory.hpp>

#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QEvent>
#include <QFile>
#include <QFont>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScopedValueRollback>
#include <QSignalBlocker>
#include <QToolButton>
#include <QVBoxLayout>

namespace QodeAssist::Settings {

namespace {

constexpr qint64 kRawTomlMaxBytes = 256 * 1024;

enum class FileReadStatus { Ok, Empty, Truncated, OpenFailed };

struct FileReadResult
{
    FileReadStatus status = FileReadStatus::OpenFailed;
    QString content;
    QString error;
};

FileReadResult readFileTextCapped(const QString &path, qint64 maxBytes)
{
    FileReadResult result;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        result.status = FileReadStatus::OpenFailed;
        result.error = f.errorString();
        return result;
    }
    const qint64 size = f.size();
    const QByteArray bytes = f.read(maxBytes);
    result.content = QString::fromUtf8(bytes);
    if (size == 0)
        result.status = FileReadStatus::Empty;
    else if (size > maxBytes)
        result.status = FileReadStatus::Truncated;
    else
        result.status = FileReadStatus::Ok;
    return result;
}

} // namespace

AgentDetailPane::AgentDetailPane(QWidget *parent)
    : QWidget(parent)
{
    m_name = new QLabel(this);
    QFont nf = m_name->font();
    nf.setBold(true);
    nf.setPixelSize(15);
    m_name->setFont(nf);
    m_name->setTextInteractionFlags(Qt::TextSelectableByMouse);

    m_sourceBadge = new QLabel(this);
    m_sourceBadge->setVisible(false);

    m_path = new QLabel(this);
    m_path->setFont(monospaceFont(11));
    m_path->setTextInteractionFlags(Qt::TextSelectableByMouse);
    QPalette pp = m_path->palette();
    pp.setColor(QPalette::WindowText, Utils::creatorColor(Utils::Theme::PanelTextColorMid));
    m_path->setPalette(pp);

    m_dupBtn = new QPushButton(tr("Customize a copy…"), this);
    m_dupBtn->setToolTip(
        tr("Create an editable user agent that inherits from this one — "
           "override only the fields you want."));
    m_openBtn = new QPushButton(tr("Open in editor"), this);
    m_deleteBtn = new QPushButton(tr("Delete"), this);
    connect(m_dupBtn, &QPushButton::clicked, this, [this] {
        if (m_current)
            emit customizeRequested(*m_current);
    });
    connect(m_openBtn, &QPushButton::clicked, this, [this] {
        if (m_current)
            emit openInEditorRequested(*m_current);
    });
    connect(m_deleteBtn, &QPushButton::clicked, this, [this] {
        if (m_current)
            emit deleteRequested(*m_current);
    });

    m_actionsHolder = new QWidget(this);
    auto *actions = new QHBoxLayout(m_actionsHolder);
    actions->setContentsMargins(0, 0, 0, 0);
    actions->setSpacing(6);
    actions->addWidget(m_dupBtn);
    actions->addWidget(m_openBtn);
    actions->addWidget(m_deleteBtn);

    auto *titleRow = new QHBoxLayout;
    titleRow->setContentsMargins(0, 0, 0, 0);
    titleRow->setSpacing(8);
    titleRow->addWidget(m_name);
    titleRow->addWidget(m_sourceBadge, 0, Qt::AlignVCenter);
    titleRow->addStretch(1);

    auto *headerLeft = new QVBoxLayout;
    headerLeft->setContentsMargins(0, 0, 0, 0);
    headerLeft->setSpacing(2);
    headerLeft->addLayout(titleRow);
    headerLeft->addWidget(m_path);

    auto *headerRow = new QHBoxLayout;
    headerRow->setContentsMargins(0, 0, 0, 0);
    headerRow->setSpacing(8);
    headerRow->addLayout(headerLeft, 1);
    headerRow->addWidget(m_actionsHolder);

    m_headerSep = new QFrame(this);
    m_headerSep->setFrameShape(QFrame::HLine);
    m_headerSep->setFrameShadow(QFrame::Sunken);

    m_description = new QLabel(this);
    m_description->setWordWrap(true);
    m_description->setTextInteractionFlags(Qt::TextSelectableByMouse);

    auto *setup = new SectionBox(tr("Configuration"), this);

    m_modelSelector = new ModelSelector(this);
    connect(m_modelSelector, &ModelSelector::modelSubmitted, this, [this](const QString &model) {
        onModelSubmitted(model);
    });

    m_modelResetBtn = new QPushButton(tr("Reset"), this);
    m_modelResetBtn->setToolTip(tr("Remove the model override and restore the agent's default"));
    m_modelResetBtn->setVisible(false);
    connect(m_modelResetBtn, &QPushButton::clicked, this, [this] { onResetModel(); });

    auto *modelHolder = new QWidget(this);
    auto *modelRow = new QHBoxLayout(modelHolder);
    modelRow->setContentsMargins(0, 0, 0, 0);
    modelRow->setSpacing(6);
    modelRow->addWidget(m_modelSelector, 1);
    modelRow->addWidget(m_modelResetBtn);

    m_modelStatus = makeHintLabel(QString(), this);
    m_modelStatus->setVisible(false);
    connect(m_modelSelector, &ModelSelector::statusChanged, this, [this](const QString &status) {
        m_modelStatus->setText(status);
        m_modelStatus->setVisible(!status.isEmpty());
    });

    m_providerCombo = new QComboBox(this);
    m_providerCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_providerCombo->setEnabled(false);
    connect(m_providerCombo, &QComboBox::activated, this, [this](int index) {
        onChangeProvider(index);
    });

    m_providerResetBtn = new QPushButton(tr("Reset"), this);
    m_providerResetBtn->setToolTip(
        tr("Remove the provider override and restore the agent's default"));
    m_providerResetBtn->setVisible(false);
    connect(m_providerResetBtn, &QPushButton::clicked, this, [this] { onResetProvider(); });

    auto *providerHolder = new QWidget(this);
    auto *providerRow = new QHBoxLayout(providerHolder);
    providerRow->setContentsMargins(0, 0, 0, 0);
    providerRow->setSpacing(6);
    providerRow->addWidget(m_providerCombo, 1);
    providerRow->addWidget(m_providerResetBtn);

    m_toolsCheck = new QCheckBox(tr("Allow tool calls"), this);
    m_toolsCheck->setEnabled(false);
    connect(m_toolsCheck, &QCheckBox::clicked, this, [this](bool on) { onToggleTools(on); });

    m_toolsResetBtn = new QPushButton(tr("Reset"), this);
    m_toolsResetBtn->setToolTip(tr("Remove the tools override and restore the agent's default"));
    m_toolsResetBtn->setVisible(false);
    connect(m_toolsResetBtn, &QPushButton::clicked, this, [this] { onResetTools(); });

    auto *toolsHolder = new QWidget(this);
    auto *toolsRow = new QHBoxLayout(toolsHolder);
    toolsRow->setContentsMargins(0, 0, 0, 0);
    toolsRow->setSpacing(6);
    toolsRow->addWidget(m_toolsCheck);
    toolsRow->addStretch(1);
    toolsRow->addWidget(m_toolsResetBtn);

    m_thinkingValue = new QLabel(this);

    auto *setupGrid = new QGridLayout;
    setupGrid->setContentsMargins(0, 0, 0, 0);
    setupGrid->setHorizontalSpacing(8);
    setupGrid->setVerticalSpacing(4);
    FormBuilder setupForm(setupGrid);
    setupForm.row(
        tr("Model:"),
        modelHolder,
        tr("Click to pick from the provider's models, or type a name and press Enter."));
    {
        const int row = setupForm.currentRow();
        setupGrid->addWidget(m_modelStatus, row, 1);
        setupForm = FormBuilder(setupGrid, row + 1);
    }
    setupForm.row(tr("Provider:"), providerHolder)
        .row(tr("Tools:"), toolsHolder)
        .row(tr("Thinking:"), m_thinkingValue);
    setup->bodyLayout()->addLayout(setupGrid);
    setup->bodyLayout()->addWidget(makeHintLabel(
        tr("Changes apply immediately and are saved as per-agent overrides in settings; "
           "Reset restores the value from the agent file."),
        this));

    m_extendsValue = makeReadOnlyLine();
    m_extendsValue->setPlaceholderText(tr("(none)"));
    m_tagsValue = makeReadOnlyLine();
    m_tagsValue->setPlaceholderText(tr("(none)"));
    m_endpointValue = makeReadOnlyLine(true);
    m_endpointValue->setPlaceholderText(tr("(provider default)"));
    m_filePatternsValue = makeReadOnlyLine(true);
    m_filePatternsValue->setPlaceholderText(tr("(matches every file)"));

    m_effectiveUrl = new QLabel(this);
    m_effectiveUrl->setFont(monospaceFont(11));
    m_effectiveUrl->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_effectiveUrl->setWordWrap(true);
    m_effectiveUrl->setContentsMargins(6, 4, 6, 4);
    m_effectiveUrl->setAutoFillBackground(true);

    m_rawToggle = new QToolButton(this);
    m_rawToggle->setText(tr("▸ Show raw TOML"));
    m_rawToggle->setCursor(Qt::PointingHandCursor);
    m_rawToggle->setAutoRaise(true);
    m_rawToggle->setCheckable(true);
    m_rawToml = new QPlainTextEdit(this);
    m_rawToml->setReadOnly(true);
    m_rawToml->setFont(monospaceFont(11));
    m_rawToml->setMinimumHeight(140);
    m_rawToml->setVisible(false);
    connect(m_rawToggle, &QToolButton::toggled, this, [this](bool on) {
        m_rawToml->setVisible(on);
        m_rawToggle->setText(on ? tr("▾ Hide raw TOML") : tr("▸ Show raw TOML"));
    });

    m_baseRawToggle = new QToolButton(this);
    m_baseRawToggle->setText(tr("▸ Show base agent TOML"));
    m_baseRawToggle->setCursor(Qt::PointingHandCursor);
    m_baseRawToggle->setAutoRaise(true);
    m_baseRawToggle->setCheckable(true);
    m_baseRawToml = new QPlainTextEdit(this);
    m_baseRawToml->setReadOnly(true);
    m_baseRawToml->setFont(monospaceFont(11));
    m_baseRawToml->setMinimumHeight(140);
    m_baseRawToml->setVisible(false);
    connect(m_baseRawToggle, &QToolButton::toggled, this, [this](bool on) {
        m_baseRawToml->setVisible(on);
        m_baseRawToggle->setText(on ? tr("▾ Hide base agent TOML") : tr("▸ Show base agent TOML"));
    });

    m_detailsBody = new QWidget(this);
    auto *detailsLayout = new QVBoxLayout(m_detailsBody);
    detailsLayout->setContentsMargins(0, 0, 0, 0);
    detailsLayout->setSpacing(10);

    auto *detailsGrid = new QGridLayout;
    detailsGrid->setContentsMargins(0, 0, 0, 0);
    detailsGrid->setHorizontalSpacing(8);
    detailsGrid->setVerticalSpacing(4);
    FormBuilder(detailsGrid)
        .row(tr("Extends:"), m_extendsValue)
        .row(tr("Tags:"), m_tagsValue, tr("Free-form — used to filter and group the agent list."))
        .row(tr("Endpoint:"), m_endpointValue, tr("Appended to the provider's URL."))
        .row(
            tr("File patterns:"),
            m_filePatternsValue,
            tr("Globs, comma-separated. When a feature slot has multiple bound "
               "agents, the first whose match rules satisfy the context wins."));
    detailsLayout->addLayout(detailsGrid);
    detailsLayout->addWidget(m_effectiveUrl);
    detailsLayout->addWidget(m_rawToggle, 0, Qt::AlignLeft);
    detailsLayout->addWidget(m_rawToml);
    detailsLayout->addWidget(m_baseRawToggle, 0, Qt::AlignLeft);
    detailsLayout->addWidget(m_baseRawToml);
    m_detailsBody->setVisible(false);

    m_detailsToggle = new QToolButton(this);
    m_detailsToggle->setText(tr("▸ More details"));
    m_detailsToggle->setCursor(Qt::PointingHandCursor);
    m_detailsToggle->setAutoRaise(true);
    m_detailsToggle->setCheckable(true);
    connect(m_detailsToggle, &QToolButton::toggled, this, [this](bool on) {
        m_detailsBody->setVisible(on);
        m_detailsToggle->setText(on ? tr("▾ More details") : tr("▸ More details"));
    });

    m_body = new QWidget(this);
    auto *bodyLayout = new QVBoxLayout(m_body);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(10);
    bodyLayout->addWidget(setup);
    bodyLayout->addWidget(m_detailsToggle, 0, Qt::AlignLeft);
    bodyLayout->addWidget(m_detailsBody);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(10);
    root->addLayout(headerRow);
    root->addWidget(m_headerSep);
    root->addWidget(m_description);
    root->addWidget(m_body);
    root->addStretch(1);

    clear();
    applyCodePalette();
}

void AgentDetailPane::setInstanceFactory(Providers::ProviderInstanceFactory *factory)
{
    if (m_instanceFactory)
        disconnect(m_instanceFactory, nullptr, this, nullptr);
    m_instanceFactory = factory;
    if (m_instanceFactory) {
        connect(
            m_instanceFactory,
            &Providers::ProviderInstanceFactory::instancesReloaded,
            this,
            [this]() {
                const QString selected = m_providerCombo->currentData().toString();
                m_providerComboPopulated = false;
                populateProviderCombo();
                const int idx = m_providerCombo->findData(selected);
                if (idx >= 0)
                    m_providerCombo->setCurrentIndex(idx);
            });
    }
    m_providerComboPopulated = false;
    populateProviderCombo();
}

void AgentDetailPane::setAgentFactory(AgentFactory *factory)
{
    m_agentFactory = factory;
}

void AgentDetailPane::populateProviderCombo()
{
    if (m_providerComboPopulated)
        return;
    m_providerCombo->clear();
    m_providerComboHasSentinel = false;
    if (m_instanceFactory) {
        for (const auto &inst : m_instanceFactory->instances()) {
            m_providerCombo
                ->addItem(QStringLiteral("%1  (%2)").arg(inst.name, inst.clientApi), inst.name);
        }
    }
    m_providerComboPopulated = true;
}

void AgentDetailPane::onModelSubmitted(const QString &model)
{
    if (!m_agentFactory || !m_current)
        return;

    const QString name = m_current->name;
    if (model == m_current->model)
        return;

    QString err;
    const bool ok = m_agentFactory->setAgentModelOverride(name, model, &err);
    if (!ok)
        QMessageBox::warning(this, tr("Set model"), err);
    if (const AgentConfig *cfg = m_agentFactory->configByName(name))
        setAgent(*cfg);
}

void AgentDetailPane::onResetModel()
{
    if (!m_agentFactory || !m_current)
        return;
    if (m_agentFactory->agentModelOverride(m_current->name).isEmpty())
        return;

    const QString name = m_current->name;
    QString err;
    if (!m_agentFactory->setAgentModelOverride(name, QString(), &err)) {
        QMessageBox::warning(this, tr("Reset model"), err);
        return;
    }
    if (const AgentConfig *cfg = m_agentFactory->configByName(name))
        setAgent(*cfg);
}

void AgentDetailPane::onChangeProvider(int index)
{
    if (!m_agentFactory || !m_current || index < 0)
        return;

    const QString name = m_current->name;
    const QString selected = m_providerCombo->itemData(index).toString();
    if (selected.isEmpty() || selected == m_current->providerInstance)
        return;

    const auto answer = QMessageBox::warning(
        this,
        tr("Change provider"),
        tr("Changing the agent's default provider may make it behave incorrectly — "
           "the agent template is tuned for its original provider.\n\n"
           "Switch '%1' to provider '%2'?")
            .arg(name, selected),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        const int cur = m_providerCombo->findData(m_current->providerInstance);
        if (cur >= 0)
            m_providerCombo->setCurrentIndex(cur);
        return;
    }

    QString err;
    if (!m_agentFactory->setAgentProviderOverride(name, selected, &err)) {
        QMessageBox::warning(this, tr("Change provider"), err);
        return;
    }
    if (const AgentConfig *cfg = m_agentFactory->configByName(name))
        setAgent(*cfg);
}

void AgentDetailPane::onResetProvider()
{
    if (!m_agentFactory || !m_current)
        return;
    if (m_agentFactory->agentProviderOverride(m_current->name).isEmpty())
        return;

    const QString name = m_current->name;
    QString err;
    if (!m_agentFactory->setAgentProviderOverride(name, QString(), &err)) {
        QMessageBox::warning(this, tr("Reset provider"), err);
        return;
    }
    if (const AgentConfig *cfg = m_agentFactory->configByName(name))
        setAgent(*cfg);
}

void AgentDetailPane::onToggleTools(bool enabled)
{
    if (!m_agentFactory || !m_current)
        return;

    const QString name = m_current->name;
    QString err;
    if (!m_agentFactory->setAgentToolsOverride(name, enabled, &err)) {
        QMessageBox::warning(this, tr("Set tools"), err);
        return;
    }
    if (const AgentConfig *cfg = m_agentFactory->configByName(name))
        setAgent(*cfg);
}

void AgentDetailPane::onResetTools()
{
    if (!m_agentFactory || !m_current)
        return;
    if (!m_agentFactory->agentToolsOverride(m_current->name).has_value())
        return;

    const QString name = m_current->name;
    QString err;
    if (!m_agentFactory->setAgentToolsOverride(name, std::nullopt, &err)) {
        QMessageBox::warning(this, tr("Reset tools"), err);
        return;
    }
    if (const AgentConfig *cfg = m_agentFactory->configByName(name))
        setAgent(*cfg);
}

void AgentDetailPane::setAgent(const AgentConfig &cfg)
{
    m_currentStorage = cfg;
    m_current = &m_currentStorage;
    const bool user = cfg.isUserSource();

    setDetailsVisible(true);

    m_name->setText(cfg.name);
    m_sourceBadge->setText(user ? tr("User") : tr("Bundled"));
    m_path->setText(cfg.sourcePath);
    m_description->setText(
        cfg.description.isEmpty() ? tr("No description provided.") : cfg.description);

    populateProviderCombo();

    if (m_providerComboHasSentinel) {
        m_providerCombo->removeItem(0);
        m_providerComboHasSentinel = false;
    }

    QString resolvedUrl;
    if (m_instanceFactory) {
        if (const auto *inst = m_instanceFactory->instanceByName(cfg.providerInstance))
            resolvedUrl = inst->url;
    }
    const int idx = m_providerCombo->findData(cfg.providerInstance);
    if (idx >= 0) {
        m_providerCombo->setCurrentIndex(idx);
    } else if (!cfg.providerInstance.isEmpty()) {
        m_providerCombo->insertItem(
            0,
            tr("%1  (missing — not in provider library)").arg(cfg.providerInstance),
            cfg.providerInstance);
        m_providerCombo->setCurrentIndex(0);
        m_providerComboHasSentinel = true;
    }

    m_providerCombo->setEnabled(m_agentFactory != nullptr);
    const bool hasProviderOverride = m_agentFactory
                                     && !m_agentFactory->agentProviderOverride(cfg.name).isEmpty();
    m_providerResetBtn->setVisible(hasProviderOverride);
    m_providerCombo->setToolTip(
        hasProviderOverride
            ? tr("Overridden in settings. Reset to use the agent's default provider.")
            : QString());

    m_modelSelector->setAgent(m_agentFactory, cfg.name, cfg.providerInstance, cfg.model);
    const bool hasModelOverride = m_agentFactory
                                  && !m_agentFactory->agentModelOverride(cfg.name).isEmpty();
    m_modelResetBtn->setVisible(hasModelOverride);
    m_modelSelector->setToolTip(
        hasModelOverride ? tr("Overridden in settings. Reset to use the agent's default model.")
                         : QString());

    m_thinkingValue->setText(cfg.enableThinking ? tr("Enabled") : tr("Disabled"));
    {
        QSignalBlocker block(m_toolsCheck);
        m_toolsCheck->setChecked(cfg.enableTools);
    }
    m_toolsCheck->setEnabled(m_agentFactory != nullptr);
    const bool hasToolsOverride = m_agentFactory
                                  && m_agentFactory->agentToolsOverride(cfg.name).has_value();
    m_toolsResetBtn->setVisible(hasToolsOverride);
    m_toolsCheck->setToolTip(
        hasToolsOverride ? tr("Overridden in settings. Reset to use the agent's default.")
                         : QString());

    m_extendsValue->setText(cfg.extendsName);
    m_tagsValue->setText(cfg.tags.join(QStringLiteral(", ")));
    m_endpointValue->setText(cfg.endpoint);
    m_filePatternsValue->setText(cfg.match.filePatterns.join(QStringLiteral(", ")));

    const QString eff = resolvedUrl + cfg.endpoint;
    m_effectiveUrl->setText(
        eff.isEmpty() ? tr("# effective request line\n(unknown — provider instance not found)")
                      : QStringLiteral("# %1\nPOST %2").arg(tr("effective request line"), eff));

    fillRawToml(m_rawToml, cfg.sourcePath);

    const QString basePath = m_agentFactory ? m_agentFactory->sourcePathForName(cfg.extendsName)
                                            : QString();
    const bool hasBase = !cfg.extendsName.isEmpty() && !basePath.isEmpty();
    m_baseRawToggle->setVisible(hasBase);
    m_baseRawToml->setVisible(hasBase && m_baseRawToggle->isChecked());
    if (hasBase)
        fillRawToml(m_baseRawToml, basePath);
    else
        m_baseRawToml->clear();

    m_openBtn->setEnabled(user);
    m_openBtn->setToolTip(
        user ? QString()
             : tr("Bundled agents are read-only — "
                  "duplicate to edit."));
    m_deleteBtn->setEnabled(user);
    m_deleteBtn->setToolTip(user ? QString() : tr("Bundled agents cannot be deleted."));
    m_dupBtn->setEnabled(true);
    applyCodePalette();
}

void AgentDetailPane::clear()
{
    m_currentStorage = AgentConfig{};
    m_current = nullptr;
    setDetailsVisible(false);
    m_name->setText(tr("Select an agent"));
    m_path->clear();
    m_description->setText(tr("Pick an agent from the list to see its details."));
    if (m_providerComboHasSentinel) {
        m_providerCombo->removeItem(0);
        m_providerComboHasSentinel = false;
    }
    m_providerCombo->setCurrentIndex(-1);
    m_providerCombo->setEnabled(false);
    m_providerResetBtn->setVisible(false);
    m_modelSelector->clearAgent();
    m_modelResetBtn->setVisible(false);
    m_thinkingValue->clear();
    {
        QSignalBlocker block(m_toolsCheck);
        m_toolsCheck->setChecked(false);
    }
    m_toolsCheck->setEnabled(false);
    m_toolsResetBtn->setVisible(false);
    m_extendsValue->clear();
    m_tagsValue->clear();
    m_endpointValue->clear();
    m_filePatternsValue->clear();
    m_effectiveUrl->clear();
    m_rawToml->clear();
    m_baseRawToml->clear();
    m_baseRawToggle->setVisible(false);
    m_baseRawToml->setVisible(false);
    m_openBtn->setEnabled(false);
    m_dupBtn->setEnabled(false);
    m_deleteBtn->setEnabled(false);
}

void AgentDetailPane::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (m_inApplyPalette)
        return;
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange)
        applyCodePalette();
}

QLineEdit *AgentDetailPane::makeReadOnlyLine(bool mono)
{
    auto *e = new QLineEdit(this);
    e->setReadOnly(true);
    if (mono)
        e->setFont(monospaceFont(11));
    return e;
}

void AgentDetailPane::fillRawToml(QPlainTextEdit *view, const QString &path)
{
    const FileReadResult raw = readFileTextCapped(path, kRawTomlMaxBytes);
    switch (raw.status) {
    case FileReadStatus::Ok:
        view->setPlainText(raw.content);
        break;
    case FileReadStatus::Truncated:
        view->setPlainText(
            raw.content + QStringLiteral("\n\n")
            + tr("(truncated at %1 bytes)").arg(kRawTomlMaxBytes));
        break;
    case FileReadStatus::Empty:
        view->setPlainText(tr("(source file is empty)"));
        break;
    case FileReadStatus::OpenFailed:
        view->setPlainText(tr("(source file unavailable: %1)").arg(raw.error));
        break;
    }
}

void AgentDetailPane::setDetailsVisible(bool visible)
{
    m_headerSep->setVisible(visible);
    m_sourceBadge->setVisible(visible);
    m_path->setVisible(visible);
    m_actionsHolder->setVisible(visible);
    m_body->setVisible(visible);
}

void AgentDetailPane::applyCodePalette()
{
    QScopedValueRollback<bool> guard(m_inApplyPalette, true);
    const QColor codeBg = Utils::creatorColor(Utils::Theme::BackgroundColorNormal);
    QPalette p = m_effectiveUrl->palette();
    p.setColor(QPalette::Window, codeBg);
    p.setColor(QPalette::WindowText, Utils::creatorColor(Utils::Theme::TextColorNormal));
    m_effectiveUrl->setPalette(p);
    m_effectiveUrl->setStyleSheet(
        QStringLiteral("QLabel { background:%1; border:1px solid %2; }")
            .arg(cssColor(codeBg), cssColor(Utils::creatorColor(Utils::Theme::SplitterColor))));
    if (m_current)
        styleSourceBadge(m_sourceBadge, m_current->isUserSource());
}

} // namespace QodeAssist::Settings
