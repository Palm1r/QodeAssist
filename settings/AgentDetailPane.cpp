// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "AgentDetailPane.hpp"

#include "SectionBox.hpp"
#include "SettingsTheme.hpp"
#include "SettingsUiBuilders.hpp"

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
    pp.setColor(QPalette::WindowText, pp.color(QPalette::Mid));
    m_path->setPalette(pp);

    m_openBtn = new QPushButton(tr("Open in editor"), this);
    m_dupBtn = new QPushButton(tr("Duplicate…"), this);
    m_deleteBtn = new QPushButton(tr("Delete"), this);
    connect(m_openBtn, &QPushButton::clicked, this,
            [this] { if (m_current) emit openInEditorRequested(*m_current); });
    connect(m_dupBtn, &QPushButton::clicked, this,
            [this] { if (m_current) emit customizeRequested(*m_current); });
    connect(m_deleteBtn, &QPushButton::clicked, this,
            [this] { if (m_current) emit deleteRequested(*m_current); });

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
    idForm.row(tr("Tags:"), m_tagsValue,
               tr("Comma-separated. Free-form — used to filter and "
                  "group the agent list."));
    identity->bodyLayout()->addLayout(idGrid);

    auto *roleSection = new SectionBox(tr("System role"), this);
    auto *roleHint = makeHintLabel(
        tr("Prepended to every request as the system message."));
    m_roleText = new QPlainTextEdit(this);
    m_roleText->setReadOnly(true);
    m_roleText->setMinimumHeight(120);
    roleSection->bodyLayout()->addWidget(roleHint);
    roleSection->bodyLayout()->addWidget(m_roleText);

    auto *contextSection = new SectionBox(tr("Context"), this);
    auto *contextHint = makeHintLabel(
        tr("Jinja2 template rendered with ContextManager bindings into the "
           "agent.context system-prompt layer. Empty = no context block."));
    m_contextText = new QPlainTextEdit(this);
    m_contextText->setReadOnly(true);
    m_contextText->setFont(monospaceFont(11));
    m_contextText->setMinimumHeight(120);
    contextSection->bodyLayout()->addWidget(contextHint);
    contextSection->bodyLayout()->addWidget(m_contextText);

    auto *connection = new SectionBox(tr("Connection"), this);
    m_providerCombo = new QComboBox(this);
    m_providerCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_providerCombo->setEnabled(false);
    m_endpointValue = makeReadOnlyLine(true);
    m_modelValue = makeReadOnlyLine(true);

    auto *connGrid = new QGridLayout;
    connGrid->setContentsMargins(0, 0, 0, 0);
    connGrid->setHorizontalSpacing(8);
    connGrid->setVerticalSpacing(4);
    FormBuilder(connGrid)
        .row(tr("Provider:"), m_providerCombo,
             tr("The provider instance this agent uses. URL is "
                "inherited from the instance."))
        .row(tr("Endpoint:"), m_endpointValue,
             tr("Appended to the provider's URL. Blank uses the "
                "provider default."))
        .row(tr("Model:"), m_modelValue);
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
    FormBuilder(matchGrid).row(tr("File patterns:"), m_filePatternsValue,
                               tr("Globs, comma-separated. Empty matches every file."));
    match->bodyLayout()->addWidget(matchHint);
    match->bodyLayout()->addLayout(matchGrid);

    auto *templ = new SectionBox(tr("Template"), this);
    auto *templHint = makeHintLabel(
        tr("Jinja2 template (via inja) rendered to the request body. "
           "Built-in context: ctx.prefix, ctx.suffix, ctx.history, "
           "ctx.system_prompt, agent.model."));
    m_messageFormat = new QPlainTextEdit(this);
    m_messageFormat->setReadOnly(true);
    m_messageFormat->setFont(monospaceFont(11));
    m_messageFormat->setMinimumHeight(140);

    templ->bodyLayout()->addWidget(templHint);
    auto *mfLabel = new QLabel(tr("message_format:"), this);
    templ->bodyLayout()->addWidget(mfLabel);
    templ->bodyLayout()->addWidget(m_messageFormat);

    m_diagnostics = new SectionBox(tr("Load errors"), this);
    m_diagnosticsView = new QPlainTextEdit(this);
    m_diagnosticsView->setReadOnly(true);
    m_diagnosticsView->setMaximumHeight(110);
    m_diagnosticsView->setFont(monospaceFont(11));
    m_diagnostics->bodyLayout()->addWidget(m_diagnosticsView);
    m_diagnostics->setVisible(false);

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
    root->addWidget(templ);
    root->addWidget(roleSection);
    root->addWidget(contextSection);
    root->addWidget(m_diagnostics);
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

void AgentDetailPane::populateProviderCombo()
{
    if (m_providerComboPopulated)
        return;
    m_providerCombo->clear();
    m_providerComboHasSentinel = false;
    if (m_instanceFactory) {
        for (const auto &inst : m_instanceFactory->instances()) {
            m_providerCombo->addItem(
                QStringLiteral("%1  (%2)").arg(inst.name, inst.clientApi), inst.name);
        }
    }
    m_providerComboPopulated = true;
}

void AgentDetailPane::setAgent(const AgentConfig &cfg)
{
    m_currentStorage = cfg;
    m_current = &m_currentStorage;
    const bool user = cfg.isUserSource();

    m_name->setText(cfg.name);
    m_path->setText(cfg.sourcePath);
    m_description->setText(cfg.description.isEmpty()
                               ? tr("No description provided.")
                               : cfg.description);

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
            0, tr("%1  (missing — not in provider library)")
                   .arg(cfg.providerInstance),
            cfg.providerInstance);
        m_providerCombo->setCurrentIndex(0);
        m_providerComboHasSentinel = true;
    }

    m_endpointValue->setText(cfg.endpoint);
    m_endpointValue->setPlaceholderText(tr("(provider default)"));
    m_modelValue->setText(cfg.model);

    const QString eff = resolvedUrl + cfg.endpoint;
    m_effectiveUrl->setText(
        eff.isEmpty()
            ? tr("# effective request line\n(unknown — provider instance not found)")
            : QStringLiteral("# %1\nPOST %2")
                  .arg(tr("effective request line"), eff));

    m_roleText->setPlainText(
        cfg.role.isEmpty() ? tr("(no system role set)") : cfg.role);
    m_contextText->setPlainText(
        cfg.context.isEmpty() ? tr("(no context block)") : cfg.context);

    m_filePatternsValue->setText(cfg.match.filePatterns.join(QStringLiteral(", ")));
    m_filePatternsValue->setPlaceholderText(tr("(matches every file)"));

    m_messageFormat->setPlainText(
        cfg.messageFormat.isEmpty() ? tr("(inherited from parent / none)")
                                    : cfg.messageFormat);

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
    m_openBtn->setToolTip(user ? QString()
                               : tr("Bundled agents are read-only — "
                                    "duplicate to edit."));
    m_deleteBtn->setEnabled(user);
    m_deleteBtn->setToolTip(user ? QString()
                                 : tr("Bundled agents cannot be deleted."));
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
    m_endpointValue->clear();
    m_modelValue->clear();
    m_effectiveUrl->clear();
    m_roleText->clear();
    m_contextText->clear();
    m_filePatternsValue->clear();
    m_messageFormat->clear();
    m_rawToml->clear();
    m_openBtn->setEnabled(false);
    m_dupBtn->setEnabled(false);
    m_deleteBtn->setEnabled(false);
}

void AgentDetailPane::setLoadDiagnostics(const QStringList &errors, const QStringList &warnings)
{
    QStringList lines;
    for (const QString &e : errors)
        lines << tr("error: %1").arg(e);
    for (const QString &w : warnings)
        lines << tr("warning: %1").arg(w);
    m_diagnostics->setVisible(!lines.isEmpty());
    m_diagnosticsView->setPlainText(lines.join(QLatin1Char('\n')));
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
    const Theme theme = themeFor(palette());
    QPalette p = m_effectiveUrl->palette();
    p.setColor(QPalette::Window, QColor(theme.codeBg));
    p.setColor(QPalette::WindowText, palette().color(QPalette::Text));
    m_effectiveUrl->setPalette(p);
    m_effectiveUrl->setStyleSheet(QStringLiteral(
                                      "QLabel { background:%1; border:1px solid %2; }")
                                      .arg(theme.codeBg, theme.rowSeparator));
}

} // namespace QodeAssist::Settings
