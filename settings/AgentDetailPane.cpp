// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "AgentDetailPane.hpp"

#include "AgentModelDialog.hpp"
#include "SectionBox.hpp"
#include "SettingsTheme.hpp"
#include "SettingsUiBuilders.hpp"

#include <utils/theme/theme.h>

#include <AgentFactory.hpp>
#include <ProviderInstance.hpp>
#include <ProviderInstanceFactory.hpp>

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

    m_path = new QLabel(this);
    m_path->setFont(monospaceFont(11));
    m_path->setTextInteractionFlags(Qt::TextSelectableByMouse);
    QPalette pp = m_path->palette();
    pp.setColor(QPalette::WindowText, Utils::creatorColor(Utils::Theme::PanelTextColorMid));
    m_path->setPalette(pp);

    m_openBtn = new QPushButton(tr("Open in editor"), this);
    m_dupBtn = new QPushButton(tr("Duplicate…"), this);
    m_deleteBtn = new QPushButton(tr("Delete"), this);
    connect(m_openBtn, &QPushButton::clicked, this, [this] {
        if (m_current)
            emit openInEditorRequested(*m_current);
    });
    connect(m_dupBtn, &QPushButton::clicked, this, [this] {
        if (m_current)
            emit customizeRequested(*m_current);
    });
    connect(m_deleteBtn, &QPushButton::clicked, this, [this] {
        if (m_current)
            emit deleteRequested(*m_current);
    });

    auto *actions = new QHBoxLayout;
    actions->setContentsMargins(0, 0, 0, 0);
    actions->setSpacing(6);
    actions->addWidget(m_openBtn);
    actions->addWidget(m_dupBtn);
    actions->addWidget(m_deleteBtn);

    auto *titleRow = new QHBoxLayout;
    titleRow->setContentsMargins(0, 0, 0, 0);
    titleRow->setSpacing(8);
    titleRow->addWidget(m_name);
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
    headerRow->addLayout(actions);

    auto *headerSep = new QFrame(this);
    headerSep->setFrameShape(QFrame::HLine);
    headerSep->setFrameShadow(QFrame::Sunken);

    m_description = new QLabel(this);
    m_description->setWordWrap(true);
    m_description->setTextInteractionFlags(Qt::TextSelectableByMouse);

    auto *identity = new SectionBox(tr("Identity"), this);
    m_nameValue = makeReadOnlyLine();
    m_extendsLabel = new QLabel(tr("Extends:"), this);
    m_extendsLabel->setMinimumWidth(96);
    m_extendsLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_extendsValue = makeReadOnlyLine();
    m_descriptionEdit = new QPlainTextEdit(this);
    m_descriptionEdit->setReadOnly(true);
    m_descriptionEdit->setMaximumHeight(56);
    m_tagsValue = makeReadOnlyLine();

    auto *idGrid = new QGridLayout;
    idGrid->setContentsMargins(0, 0, 0, 0);
    idGrid->setHorizontalSpacing(8);
    idGrid->setVerticalSpacing(4);
    FormBuilder idForm(idGrid);
    idForm.row(tr("Name:"), m_nameValue);
    {
        auto *holder = new QWidget;
        holder->setLayout(singleField(m_extendsValue));
        const int row = idForm.currentRow();
        idGrid->addWidget(m_extendsLabel, row, 0, Qt::AlignTop);
        idGrid->addWidget(holder, row, 1);
        m_extendsHolder = holder;
        idForm = FormBuilder(idGrid, row + 1);
    }
    idForm.row(tr("Description:"), m_descriptionEdit);
    idForm.row(
        tr("Tags:"),
        m_tagsValue,
        tr("Comma-separated. Free-form — used to filter and "
           "group the agent list."));
    identity->bodyLayout()->addLayout(idGrid);

    auto *connection = new SectionBox(tr("Connection"), this);
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

    m_endpointValue = makeReadOnlyLine(true);

    m_modelValue = makeReadOnlyLine(true);
    m_modelValue->setPlaceholderText(tr("(set a model)"));

    m_modelChangeBtn = new QPushButton(tr("Change…"), this);
    m_modelChangeBtn->setToolTip(tr("Pick a model from the provider or type one"));
    m_modelChangeBtn->setEnabled(false);
    connect(m_modelChangeBtn, &QPushButton::clicked, this, [this] { onChangeModel(); });

    m_modelResetBtn = new QPushButton(tr("Reset"), this);
    m_modelResetBtn->setToolTip(tr("Remove the model override and restore the agent's default"));
    m_modelResetBtn->setVisible(false);
    connect(m_modelResetBtn, &QPushButton::clicked, this, [this] { onResetModel(); });

    auto *modelHolder = new QWidget(this);
    auto *modelRow = new QHBoxLayout(modelHolder);
    modelRow->setContentsMargins(0, 0, 0, 0);
    modelRow->setSpacing(6);
    modelRow->addWidget(m_modelValue, 1);
    modelRow->addWidget(m_modelChangeBtn);
    modelRow->addWidget(m_modelResetBtn);

    auto *connGrid = new QGridLayout;
    connGrid->setContentsMargins(0, 0, 0, 0);
    connGrid->setHorizontalSpacing(8);
    connGrid->setVerticalSpacing(4);
    FormBuilder(connGrid)
        .row(
            tr("Provider:"),
            providerHolder,
            tr("The provider instance this agent uses. URL is "
               "inherited from the instance. Switching it is saved as a "
               "per-agent override; Reset restores the agent's default."))
        .row(
            tr("Endpoint:"),
            m_endpointValue,
            tr("Appended to the provider's URL. Blank uses the "
               "provider default."))
        .row(
            tr("Model:"),
            modelHolder,
            tr("Model sent to the provider. Click Change… to pick from the "
               "provider's available models or type one. The choice is saved "
               "as a per-agent override in settings; Reset restores the "
               "agent's default."));
    connection->bodyLayout()->addLayout(connGrid);

    m_effectiveUrl = new QLabel(this);
    m_effectiveUrl->setFont(monospaceFont(11));
    m_effectiveUrl->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_effectiveUrl->setWordWrap(true);
    m_effectiveUrl->setContentsMargins(6, 4, 6, 4);
    m_effectiveUrl->setAutoFillBackground(true);
    connection->bodyLayout()->addWidget(m_effectiveUrl);

    auto *match = new SectionBox(tr("Match"), this);
    auto *matchHint = makeHintLabel(
        tr("When a feature slot has multiple bound agents, the first whose "
           "match rules satisfy the current context wins."));
    m_filePatternsValue = makeReadOnlyLine(true);
    auto *matchGrid = new QGridLayout;
    matchGrid->setContentsMargins(0, 0, 0, 0);
    matchGrid->setHorizontalSpacing(8);
    matchGrid->setVerticalSpacing(4);
    FormBuilder(matchGrid).row(
        tr("File patterns:"),
        m_filePatternsValue,
        tr("Globs, comma-separated. Empty matches every file."));
    match->bodyLayout()->addWidget(matchHint);
    match->bodyLayout()->addLayout(matchGrid);

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

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(10);
    root->addLayout(headerRow);
    root->addWidget(headerSep);
    root->addWidget(m_description);
    root->addWidget(identity);
    root->addWidget(connection);
    root->addWidget(match);
    root->addWidget(m_rawToggle, 0, Qt::AlignLeft);
    root->addWidget(m_rawToml);
    root->addStretch(1);

    clear();
    applyCodePalette();
}

void AgentDetailPane::setInstanceFactory(Providers::ProviderInstanceFactory *factory)
{
    m_instanceFactory = factory;
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

void AgentDetailPane::onChangeModel()
{
    if (!m_agentFactory || !m_current)
        return;

    const QString name = m_current->name;
    AgentModelDialog dialog(m_agentFactory, name, m_current->model, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const QString model = dialog.selectedModel();
    if (model == m_current->model)
        return;

    QString err;
    if (!m_agentFactory->setAgentModelOverride(name, model, &err)) {
        QMessageBox::warning(this, tr("Set model"), err);
        return;
    }
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

void AgentDetailPane::setAgent(const AgentConfig &cfg)
{
    m_currentStorage = cfg;
    m_current = &m_currentStorage;
    const bool user = cfg.isUserSource();

    m_name->setText(cfg.name);
    m_path->setText(cfg.sourcePath);
    m_description->setText(
        cfg.description.isEmpty() ? tr("No description provided.") : cfg.description);

    m_nameValue->setText(cfg.name);
    if (cfg.extendsName.isEmpty()) {
        m_extendsLabel->setVisible(false);
        m_extendsHolder->setVisible(false);
    } else {
        m_extendsLabel->setVisible(true);
        m_extendsHolder->setVisible(true);
        m_extendsValue->setText(cfg.extendsName);
    }
    m_descriptionEdit->setPlainText(cfg.description);
    m_tagsValue->setText(cfg.tags.join(QStringLiteral(", ")));

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

    m_endpointValue->setText(cfg.endpoint);
    m_endpointValue->setPlaceholderText(tr("(provider default)"));
    m_modelValue->setText(cfg.model);
    m_modelChangeBtn->setEnabled(m_agentFactory != nullptr);
    const bool hasModelOverride = m_agentFactory
                                  && !m_agentFactory->agentModelOverride(cfg.name).isEmpty();
    m_modelResetBtn->setVisible(hasModelOverride);
    m_modelValue->setToolTip(
        hasModelOverride ? tr("Overridden in settings. Reset to use the agent's default model.")
                         : QString());

    const QString eff = resolvedUrl + cfg.endpoint;
    m_effectiveUrl->setText(
        eff.isEmpty() ? tr("# effective request line\n(unknown — provider instance not found)")
                      : QStringLiteral("# %1\nPOST %2").arg(tr("effective request line"), eff));

    m_filePatternsValue->setText(cfg.match.filePatterns.join(QStringLiteral(", ")));
    m_filePatternsValue->setPlaceholderText(tr("(matches every file)"));

    const FileReadResult raw = readFileTextCapped(cfg.sourcePath, kRawTomlMaxBytes);
    switch (raw.status) {
    case FileReadStatus::Ok:
        m_rawToml->setPlainText(raw.content);
        break;
    case FileReadStatus::Truncated:
        m_rawToml->setPlainText(
            raw.content + QStringLiteral("\n\n")
            + tr("(truncated at %1 bytes)").arg(kRawTomlMaxBytes));
        break;
    case FileReadStatus::Empty:
        m_rawToml->setPlainText(tr("(source file is empty)"));
        break;
    case FileReadStatus::OpenFailed:
        m_rawToml->setPlainText(tr("(source file unavailable: %1)").arg(raw.error));
        break;
    }

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
    m_name->setText(tr("Select an agent"));
    m_path->clear();
    m_description->setText(tr("Pick an agent from the list to see its details."));
    m_nameValue->clear();
    m_extendsLabel->setVisible(false);
    m_extendsHolder->setVisible(false);
    m_descriptionEdit->clear();
    m_tagsValue->clear();
    if (m_providerComboHasSentinel) {
        m_providerCombo->removeItem(0);
        m_providerComboHasSentinel = false;
    }
    m_providerCombo->setCurrentIndex(-1);
    m_providerCombo->setEnabled(false);
    m_providerResetBtn->setVisible(false);
    m_endpointValue->clear();
    m_modelValue->clear();
    m_modelChangeBtn->setEnabled(false);
    m_modelResetBtn->setVisible(false);
    m_effectiveUrl->clear();
    m_filePatternsValue->clear();
    m_rawToml->clear();
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
}

} // namespace QodeAssist::Settings
