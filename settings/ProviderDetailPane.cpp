// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "ProviderDetailPane.hpp"

#include <array>

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

#include <solutions/terminal/terminalview.h>
#include <utils/theme/theme.h>

#include "ProviderInstanceWriter.hpp"
#include "ProviderSettings.hpp"
#include "SectionBox.hpp"
#include "SettingsTheme.hpp"
#include "SettingsUiBuilders.hpp"

namespace QodeAssist::Settings {

ProviderDetailPane::ProviderDetailPane(QWidget *parent)
    : QWidget(parent)
{
    m_nameLabel = new QLabel(this);
    QFont nf = m_nameLabel->font();
    nf.setBold(true);
    nf.setPixelSize(15);
    m_nameLabel->setFont(nf);

    m_sourcePathLabel = new QLabel(this);
    m_sourcePathLabel->setFont(monospaceFont(11));
    QPalette spp = m_sourcePathLabel->palette();
    spp.setColor(QPalette::WindowText, Utils::creatorColor(Utils::Theme::PanelTextColorMid));
    m_sourcePathLabel->setPalette(spp);

    m_protocolLabel = new QLabel(this);
    m_protocolLabel->setPalette(spp);
    m_protocolLabel->setToolTip(
        tr("The client API (protocol) this provider speaks. Fixed when the provider is created."));

    m_editBtn = new QPushButton(tr("Edit…"), this);
    m_editBtn->setDefault(true);
    m_openInEditorBtn = new QPushButton(tr("Open in editor"), this);
    m_openInEditorBtn->setToolTip(
        tr("Open this provider's TOML file in Qt Creator. "
           "Bundled providers are read-only — duplicate first."));
    m_dupBtn = new QPushButton(tr("Duplicate…"), this);
    m_deleteBtn = new QPushButton(tr("Delete"), this);
    m_cancelBtn = new QPushButton(tr("Cancel"), this);
    m_saveBtn = new QPushButton(tr("Save"), this);
    m_saveBtn->setDefault(true);
    m_cancelBtn->hide();
    m_saveBtn->hide();

    connect(m_editBtn, &QPushButton::clicked, this, [this] { setEditing(true); });
    connect(m_cancelBtn, &QPushButton::clicked, this, [this] {
        setEditing(false);
        populate(m_current, m_currentHasStoredKey);
    });
    connect(m_saveBtn, &QPushButton::clicked, this, [this] {
        emit saveRequested(collectEdits());
    });
    connect(m_openInEditorBtn, &QPushButton::clicked, this,
            [this] { emit openInEditorRequested(m_current.sourcePath); });
    connect(m_dupBtn, &QPushButton::clicked, this, [this] { emit duplicateRequested(); });
    connect(m_deleteBtn, &QPushButton::clicked, this, [this] { emit deleteRequested(); });

    auto *btnBar = new QHBoxLayout;
    btnBar->setContentsMargins(0, 0, 0, 0);
    btnBar->setSpacing(4);
    btnBar->addWidget(m_editBtn);
    btnBar->addWidget(m_openInEditorBtn);
    btnBar->addWidget(m_dupBtn);
    btnBar->addWidget(m_deleteBtn);
    btnBar->addWidget(m_cancelBtn);
    btnBar->addWidget(m_saveBtn);

    auto *titleRow = new QHBoxLayout;
    titleRow->setContentsMargins(0, 0, 0, 0);
    titleRow->setSpacing(8);
    titleRow->addWidget(m_nameLabel);
    titleRow->addStretch(1);

    auto *headerLeft = new QVBoxLayout;
    headerLeft->setContentsMargins(0, 0, 0, 0);
    headerLeft->setSpacing(2);
    headerLeft->addLayout(titleRow);
    headerLeft->addWidget(m_protocolLabel);
    headerLeft->addWidget(m_sourcePathLabel);

    auto *headerRow = new QHBoxLayout;
    headerRow->setContentsMargins(0, 0, 0, 0);
    headerRow->setSpacing(8);
    headerRow->addLayout(headerLeft, 1);
    headerRow->addLayout(btnBar);

    auto *headerSep = new QFrame(this);
    headerSep->setFrameShape(QFrame::HLine);
    headerSep->setFrameShadow(QFrame::Sunken);

    m_descriptionLabel = new QLabel(this);
    m_descriptionLabel->setWordWrap(true);
    m_descriptionLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    auto *identitySection = new SectionBox(tr("Identity"), this);
    m_nameEdit = new QLineEdit(this);
    m_descriptionEdit = new QPlainTextEdit(this);
    m_descriptionEdit->setMaximumHeight(60);
    m_descriptionEdit->setReadOnly(true);
    auto *identityGrid = new QGridLayout;
    identityGrid->setContentsMargins(0, 0, 0, 0);
    identityGrid->setHorizontalSpacing(8);
    identityGrid->setVerticalSpacing(4);
    FormBuilder(identityGrid)
        .row(tr("Name:"), m_nameEdit)
        .row(tr("Description:"), m_descriptionEdit);
    identitySection->bodyLayout()->addLayout(identityGrid);

    auto *endpointSection = new SectionBox(tr("Endpoint"), this);
    m_urlEdit = new QLineEdit(this);
    m_urlEdit->setFont(monospaceFont(11));
    auto *endpointGrid = new QGridLayout;
    endpointGrid->setContentsMargins(0, 0, 0, 0);
    endpointGrid->setHorizontalSpacing(8);
    endpointGrid->setVerticalSpacing(4);
    FormBuilder(endpointGrid).row(tr("URL:"), m_urlEdit,
                                  tr("Base URL. Agents append their endpoint path "
                                     "(e.g. /chat/completions) to this."));
    endpointSection->bodyLayout()->addLayout(endpointGrid);

    m_samplePreview = new QLabel(this);
    m_samplePreview->setFont(monospaceFont(11));
    m_samplePreview->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_samplePreview->setWordWrap(true);
    m_samplePreview->setContentsMargins(6, 4, 6, 4);
    m_samplePreview->setAutoFillBackground(true);
    endpointSection->bodyLayout()->addWidget(m_samplePreview);

    auto *credSection = new SectionBox(tr("Credentials"), this);
    m_apiKeyEdit = new QLineEdit(this);
    m_apiKeyEdit->setEchoMode(QLineEdit::Password);
    m_apiKeyEdit->setPlaceholderText(tr("Enter API key…"));
    m_revealKeyBtn = new QToolButton(this);
    m_revealKeyBtn->setText(QStringLiteral("👁"));
    m_revealKeyBtn->setCheckable(true);
    m_revealKeyBtn->setToolTip(tr("Show / hide the API key (loads the stored key)"));
    connect(m_revealKeyBtn, &QToolButton::toggled, this, [this](bool on) {
        if (on && m_apiKeyEdit->text().isEmpty() && m_currentHasStoredKey)
            emit apiKeyRevealRequested();
        m_apiKeyEdit->setEchoMode(on ? QLineEdit::Normal : QLineEdit::Password);
    });
    m_apiKeySaveBtn = new QPushButton(tr("Save key"), this);
    m_apiKeySaveBtn->setEnabled(false);
    m_apiKeyClearBtn = new QPushButton(tr("Clear"), this);
    m_apiKeyClearBtn->setToolTip(tr("Erase the stored API key for this provider"));
    m_legacyKeyBtn = new QPushButton(tr("Insert legacy key"), this);
    m_legacyKeyBtn->setVisible(false);
    connect(m_legacyKeyBtn, &QPushButton::clicked, this, [this] {
        if (m_legacyKeyValue.isEmpty())
            return;
        m_apiKeyEdit->setText(m_legacyKeyValue);
        m_revealKeyBtn->setChecked(true);
    });
    connect(m_apiKeyEdit, &QLineEdit::textChanged, this, [this](const QString &t) {
        m_apiKeySaveBtn->setEnabled(!t.isEmpty());
    });
    connect(m_apiKeyEdit, &QLineEdit::returnPressed, this, [this] {
        if (!m_apiKeyEdit->text().isEmpty())
            m_apiKeySaveBtn->click();
    });
    connect(m_apiKeySaveBtn, &QPushButton::clicked, this, [this] {
        const QString key = m_apiKeyEdit->text();
        if (key.isEmpty())
            return;
        emit apiKeySaveRequested(key);
        m_apiKeyEdit->clear();
    });
    connect(m_apiKeyClearBtn, &QPushButton::clicked, this,
            [this] { emit apiKeyClearRequested(); });
    m_keyHint = makeHintLabel(QString{}, this);

    auto *keyRow = new QHBoxLayout;
    keyRow->setContentsMargins(0, 0, 0, 0);
    keyRow->setSpacing(4);
    keyRow->addWidget(m_apiKeyEdit, 1);
    keyRow->addWidget(m_revealKeyBtn);
    keyRow->addWidget(m_apiKeySaveBtn);
    keyRow->addWidget(m_apiKeyClearBtn);

    auto *credGrid = new QGridLayout;
    credGrid->setContentsMargins(0, 0, 0, 0);
    credGrid->setHorizontalSpacing(8);
    credGrid->setVerticalSpacing(4);
    FormBuilder credForm(credGrid);
    credForm.row(tr("API key:"), keyRow);
    credGrid->addWidget(m_legacyKeyBtn, credForm.currentRow(), 1, Qt::AlignLeft);
    credGrid->addWidget(m_keyHint, credForm.currentRow() + 1, 1);
    credSection->bodyLayout()->addLayout(credGrid);

    m_launchSection = new SectionBox(tr("Launch"), this);
    m_launchEmptyHint = new QLabel(this);
    m_launchEmptyHint->setWordWrap(true);
    QPalette lehp = m_launchEmptyHint->palette();
    lehp.setColor(QPalette::WindowText, Utils::creatorColor(Utils::Theme::PanelTextColorMid));
    m_launchEmptyHint->setPalette(lehp);
    m_launchCmdLabel = new QLabel(this);
    m_launchCmdLabel->setFont(monospaceFont(11));
    m_launchCmdLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_launchCmdLabel->setWordWrap(true);
    m_launchStatusPill = new QLabel(tr("idle"), this);
    m_startBtn = new QPushButton(tr("Start"), this);
    m_stopBtn = new QPushButton(tr("Stop"), this);
    m_restartBtn = new QPushButton(tr("Restart"), this);
    connect(m_startBtn, &QPushButton::clicked, this,
            [this] { emit launchStartRequested(m_current.name); });
    connect(m_stopBtn, &QPushButton::clicked, this,
            [this] { emit launchStopRequested(m_current.name); });
    connect(m_restartBtn, &QPushButton::clicked, this,
            [this] { emit launchRestartRequested(m_current.name); });
    auto *launchBtnRow = new QHBoxLayout;
    launchBtnRow->setContentsMargins(0, 0, 0, 0);
    launchBtnRow->setSpacing(6);
    launchBtnRow->addWidget(m_launchStatusPill);
    launchBtnRow->addStretch(1);
    launchBtnRow->addWidget(m_startBtn);
    launchBtnRow->addWidget(m_stopBtn);
    launchBtnRow->addWidget(m_restartBtn);

    m_launchTerminalToggle = new QToolButton(this);
    m_launchTerminalToggle->setText(tr("▸ Show launch terminal"));
    m_launchTerminalToggle->setCursor(Qt::PointingHandCursor);
    m_launchTerminalToggle->setAutoRaise(true);
    m_launchTerminalToggle->setCheckable(true);
    m_launchTerminal = new TerminalSolution::TerminalView(this);
    {
        QFont termFont(TerminalSolution::defaultFontFamily());
        const int sz = TerminalSolution::defaultFontSize();
        if (sz > 0)
            termFont.setPixelSize(sz);
        termFont.setStyleHint(QFont::Monospace);
        m_launchTerminal->setFont(termFont);
        applyTerminalPalette();
    }
    m_launchTerminal->setMinimumHeight(180);
    m_launchTerminal->setVisible(false);
    connect(m_launchTerminalToggle, &QToolButton::toggled, this, [this](bool on) {
        m_launchTerminal->setVisible(on);
        m_launchTerminalToggle->setText(
            on ? tr("▾ Hide launch terminal") : tr("▸ Show launch terminal"));
    });

    m_launchSection->bodyLayout()->addWidget(m_launchEmptyHint);
    m_launchSection->bodyLayout()->addWidget(m_launchCmdLabel);
    m_launchSection->bodyLayout()->addLayout(launchBtnRow);
    m_launchSection->bodyLayout()->addWidget(m_launchTerminalToggle, 0, Qt::AlignLeft);
    m_launchSection->bodyLayout()->addWidget(m_launchTerminal);

    m_rawToggle = new QToolButton(this);
    m_rawToggle->setText(tr("▸ Show raw TOML"));
    m_rawToggle->setCursor(Qt::PointingHandCursor);
    m_rawToggle->setAutoRaise(true);
    m_rawToggle->setCheckable(true);
    m_rawToml = new QPlainTextEdit(this);
    m_rawToml->setReadOnly(true);
    m_rawToml->setFont(monospaceFont(11));
    m_rawToml->setMinimumHeight(120);
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
    root->addWidget(m_descriptionLabel);
    root->addWidget(identitySection);
    root->addWidget(endpointSection);
    root->addWidget(credSection);
    root->addWidget(m_launchSection);
    root->addWidget(m_rawToggle, 0, Qt::AlignLeft);
    root->addWidget(m_rawToml);
    root->addStretch(1);

    clear();
}

void ProviderDetailPane::populate(const Providers::ProviderInstance &inst, bool hasStoredKey)
{
    m_current = inst;
    m_currentHasStoredKey = hasStoredKey;
    const bool isUser = inst.isUserSource();
    const bool needsKey = !inst.apiKeyRef.isEmpty();

    m_nameLabel->setText(inst.name);
    m_sourcePathLabel->setText(inst.sourcePath);

    m_descriptionLabel->setText(
        inst.description.isEmpty() ? tr("No description provided.") : inst.description);

    m_nameEdit->setText(inst.name);
    m_protocolLabel->setText(tr("via %1").arg(inst.clientApi));
    m_descriptionEdit->setPlainText(inst.description);
    m_urlEdit->setText(inst.url);

    m_apiKeyEdit->clear();
    m_apiKeyEdit->setEnabled(needsKey);
    m_apiKeySaveBtn->setEnabled(false);
    m_apiKeyClearBtn->setEnabled(needsKey && hasStoredKey);
    m_revealKeyBtn->setEnabled(needsKey);
    m_revealKeyBtn->setChecked(false);
    if (!needsKey) {
        m_apiKeyEdit->setPlaceholderText(tr("— not required (local provider)"));
        m_keyHint->setText(tr("This provider type does not use a key."));
    } else if (hasStoredKey) {
        m_apiKeyEdit->setPlaceholderText(tr("Stored — enter a new key to replace it."));
        m_keyHint->setText(tr("A key is stored. Type a new key and press Save key to "
                              "replace it, or Clear to erase it."));
    } else {
        m_apiKeyEdit->setPlaceholderText(tr("Enter API key…"));
        m_keyHint->setText(tr("No key stored yet. Type a key and press Save key."));
    }

    const LegacyApiKeyEntry legacy
        = needsKey ? legacyApiKeyForClientApi(inst.clientApi) : LegacyApiKeyEntry{};
    m_legacyKeyValue = legacy.value;
    if (!legacy.value.isEmpty()) {
        m_legacyKeyBtn->setToolTip(
            tr("Insert the API key saved in the old %1 settings into the field.")
                .arg(legacy.label));
        m_legacyKeyBtn->setVisible(true);
    } else {
        m_legacyKeyBtn->setVisible(false);
    }

    m_samplePreview->setText(
        QStringLiteral("# sample request line\nPOST %1/<agent endpoint>").arg(inst.url));
    applyPreviewPalette();

    m_deleteBtn->setEnabled(isUser);
    m_dupBtn->setEnabled(true);
    m_editBtn->setVisible(isUser);
    m_openInEditorBtn->setEnabled(isUser);
    setEditing(false);

    QString toml = QStringLiteral("# %1\n").arg(inst.sourcePath);
    toml += Providers::ProviderInstanceWriter::toToml(inst);
    m_rawToml->setPlainText(toml);
}

void ProviderDetailPane::clear()
{
    m_current = {};
    m_nameLabel->setText(tr("Select a provider"));
    m_sourcePathLabel->clear();
    m_descriptionLabel->clear();
    m_nameEdit->clear();
    m_protocolLabel->clear();
    m_descriptionEdit->clear();
    m_urlEdit->clear();
    m_apiKeyEdit->clear();
    m_apiKeyEdit->setEnabled(false);
    m_apiKeySaveBtn->setEnabled(false);
    m_apiKeyClearBtn->setEnabled(false);
    m_revealKeyBtn->setEnabled(false);
    m_legacyKeyValue.clear();
    m_legacyKeyBtn->setVisible(false);
    m_samplePreview->clear();
    m_rawToml->clear();
    m_editBtn->setVisible(false);
    m_dupBtn->setEnabled(false);
    m_deleteBtn->setEnabled(false);
    m_openInEditorBtn->setEnabled(false);
}

void ProviderDetailPane::refreshKeyStatus(bool hasStoredKey)
{
    m_currentHasStoredKey = hasStoredKey;
    const bool needsKey = !m_current.apiKeyRef.isEmpty();
    m_apiKeyClearBtn->setEnabled(needsKey && hasStoredKey);
    if (!needsKey)
        return;
    if (hasStoredKey) {
        m_apiKeyEdit->setPlaceholderText(tr("Stored — enter a new key to replace it."));
        m_keyHint->setText(tr("A key is stored. Type a new key and press Save key to "
                              "replace it, or Clear to erase it."));
    } else {
        m_apiKeyEdit->setPlaceholderText(tr("Enter API key…"));
        m_keyHint->setText(tr("No key stored yet. Type a key and press Save key."));
    }
}

void ProviderDetailPane::setLaunchState(
    Providers::ProviderLauncher::State st, const QString &lastError)
{
    const bool hasLaunch = !m_current.launch.isEmpty();
    m_launchSection->setVisible(true);
    m_launchEmptyHint->setVisible(!hasLaunch);
    m_launchCmdLabel->setVisible(hasLaunch);
    m_startBtn->setVisible(hasLaunch);
    m_stopBtn->setVisible(hasLaunch);
    m_restartBtn->setVisible(hasLaunch);
    m_launchStatusPill->setVisible(hasLaunch);
    m_launchTerminalToggle->setVisible(hasLaunch);

    if (!hasLaunch) {
        m_launchEmptyHint->setText(tr(
            "No [launch] block. This provider is treated as external — "
            "the plugin will not spawn or supervise any process. "
            "Add a [launch] block to the TOML to have the plugin manage "
            "a local server here."));
        m_launchCmdLabel->clear();
        m_launchTerminal->clearContents();
        return;
    }

    const QString detachedNote = m_current.launch.detach
        ? QStringLiteral(" <span style='color:%1'>%2</span>")
              .arg(Utils::creatorColor(Utils::Theme::PanelTextColorMid).name(),
                   tr("(detached — survives Qt Creator restart)"))
        : QString();
    m_launchCmdLabel->setText(
        QStringLiteral("<b>%1</b> %2%3")
            .arg(m_current.launch.command.toHtmlEscaped(),
                 m_current.launch.args.join(QLatin1Char(' ')).toHtmlEscaped(),
                 detachedNote));

    QString statusText;
    switch (st) {
    case Providers::ProviderLauncher::Idle:     statusText = tr("idle"); break;
    case Providers::ProviderLauncher::Starting: statusText = tr("starting…"); break;
    case Providers::ProviderLauncher::Probing:  statusText = tr("probing…"); break;
    case Providers::ProviderLauncher::Ready:    statusText = tr("ready"); break;
    case Providers::ProviderLauncher::Stopping: statusText = tr("stopping…"); break;
    case Providers::ProviderLauncher::Failed:
        statusText = lastError.isEmpty() ? tr("failed")
                                         : tr("failed — %1").arg(lastError);
        break;
    }
    m_launchStatusPill->setText(statusText);

    const bool running = st == Providers::ProviderLauncher::Starting
                         || st == Providers::ProviderLauncher::Probing
                         || st == Providers::ProviderLauncher::Ready;
    m_startBtn->setEnabled(!running && st != Providers::ProviderLauncher::Stopping);
    m_stopBtn->setEnabled(running);
    m_restartBtn->setEnabled(running || st == Providers::ProviderLauncher::Failed);
}

void ProviderDetailPane::resetLaunchTerminal(const QByteArray &scrollback)
{
    m_launchTerminal->clearContents();
    if (!scrollback.isEmpty())
        m_launchTerminal->writeToTerminal(scrollback, true);
}

void ProviderDetailPane::appendLaunchBytes(const QByteArray &chunk)
{
    m_launchTerminal->writeToTerminal(chunk, true);
}

void ProviderDetailPane::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange) {
        applyPreviewPalette();
        applyTerminalPalette();
    }
}

void ProviderDetailPane::showRevealedKey(const QString &key)
{
    if (key.isEmpty())
        return;
    m_apiKeyEdit->setText(key);
    m_apiKeyEdit->setEchoMode(QLineEdit::Normal);
    if (!m_revealKeyBtn->isChecked())
        m_revealKeyBtn->setChecked(true);
}

void ProviderDetailPane::setEditing(bool on)
{
    m_editing = on;
    m_nameEdit->setReadOnly(!on);
    m_descriptionEdit->setReadOnly(!on);
    m_urlEdit->setReadOnly(!on);
    m_editBtn->setVisible(!on && m_current.isUserSource());
    m_dupBtn->setVisible(!on);
    m_deleteBtn->setVisible(!on);
    m_cancelBtn->setVisible(on);
    m_saveBtn->setVisible(on);
}

Providers::ProviderInstance ProviderDetailPane::collectEdits() const
{
    Providers::ProviderInstance out = m_current;
    out.name = m_nameEdit->text().trimmed();
    out.description = m_descriptionEdit->toPlainText().trimmed();
    out.url = m_urlEdit->text().trimmed();
    return out;
}

void ProviderDetailPane::applyPreviewPalette()
{
    m_samplePreview->setStyleSheet(
        QStringLiteral("QLabel { background:%1; border:1px solid %2; }")
            .arg(cssColor(Utils::creatorColor(Utils::Theme::BackgroundColorNormal)),
                 cssColor(Utils::creatorColor(Utils::Theme::SplitterColor))));
}

void ProviderDetailPane::applyTerminalPalette()
{
    if (!m_launchTerminal)
        return;
    const QPalette pal = palette();
    const bool dark = isDarkPalette(pal);
    const std::array<QColor, 16> ansi = dark
        ? std::array<QColor, 16>{
              QColor("#000000"), QColor("#cd3131"), QColor("#0dbc79"),
              QColor("#e5e510"), QColor("#2472c8"), QColor("#bc3fbc"),
              QColor("#11a8cd"), QColor("#e5e5e5"),
              QColor("#666666"), QColor("#f14c4c"), QColor("#23d18b"),
              QColor("#f5f543"), QColor("#3b8eea"), QColor("#d670d6"),
              QColor("#29b8db"), QColor("#ffffff"),
          }
        : std::array<QColor, 16>{
              QColor("#000000"), QColor("#c91b00"), QColor("#00c200"),
              QColor("#c7c400"), QColor("#0037da"), QColor("#c930c7"),
              QColor("#00c5c7"), QColor("#c7c7c7"),
              QColor("#676767"), QColor("#ff6d67"), QColor("#5ff967"),
              QColor("#fefb67"), QColor("#6871ff"), QColor("#ff76ff"),
              QColor("#5ffdff"), QColor("#ffffff"),
          };
    std::array<QColor, 20> colors{};
    for (int i = 0; i < 16; ++i)
        colors[i] = ansi[i];
    colors[16] = pal.color(QPalette::Text);
    colors[17] = pal.color(QPalette::Base);
    colors[18] = pal.color(QPalette::Highlight);
    colors[19] = dark ? QColor("#5a5a40") : QColor("#fff59d");
    m_launchTerminal->setColors(colors);
}

} // namespace QodeAssist::Settings
