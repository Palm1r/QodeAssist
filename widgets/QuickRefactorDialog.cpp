// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "QuickRefactorDialog.hpp"
#include "AddCustomInstructionDialog.hpp"
#include "CustomInstructionsManager.hpp"
#include "QodeAssisttr.h"

#include "settings/GeneralSettings.hpp"
#include "settings/SettingsConstants.hpp"

#include <QApplication>
#include <QComboBox>
#include <QCompleter>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QFontMetrics>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPlainTextEdit>
#include <QScreen>
#include <QStringListModel>
#include <QSvgRenderer>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

#include <coreplugin/icore.h>

#include <utils/theme/theme.h>
#include <utils/utilsicons.h>

namespace QodeAssist {

QuickRefactorDialog::QuickRefactorDialog(
    QWidget *parent, const QString &lastInstructions, bool refactorAgentAvailable)
    : QDialog(parent)
    , m_lastInstructions(lastInstructions)
    , m_refactorAgentAvailable(refactorAgentAvailable)
{
    setWindowTitle(Tr::tr("Quick Refactor"));
    setupUi();

    if (!m_lastInstructions.isEmpty()) {
        m_instructionEdit->setPlainText(m_lastInstructions);
        m_instructionEdit->selectAll();
    }

    QTimer::singleShot(0, this, &QuickRefactorDialog::updateDialogSize);
    m_instructionEdit->installEventFilter(this);
    m_commandsComboBox->installEventFilter(this);
    updateDialogSize();

    m_instructionEdit->setFocus();
}

void QuickRefactorDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(8);

    QHBoxLayout *actionsLayout = new QHBoxLayout();
    actionsLayout->setSpacing(4);
    createActionButtons();
    actionsLayout->addWidget(m_repeatButton);
    actionsLayout->addWidget(m_improveButton);
    actionsLayout->addWidget(m_alternativeButton);
    actionsLayout->addStretch();

    m_settingsButton = new QToolButton(this);
    m_settingsButton->setIcon(Utils::Icons::SETTINGS_TOOLBAR.icon());
    m_settingsButton->setToolTip(Tr::tr("Open Quick Refactor Settings"));
    m_settingsButton->setIconSize(QSize(16, 16));
    actionsLayout->addWidget(m_settingsButton);

    connect(m_settingsButton, &QToolButton::clicked, this, &QuickRefactorDialog::onOpenSettings);

    mainLayout->addLayout(actionsLayout);

    QLabel *instructionLabel = new QLabel(Tr::tr("Your Current Instruction:"), this);
    mainLayout->addWidget(instructionLabel);

    m_instructionEdit = new QPlainTextEdit(this);
    m_instructionEdit->setMinimumHeight(80);
    m_instructionEdit->setPlaceholderText(Tr::tr("Type or edit your instruction..."));
    mainLayout->addWidget(m_instructionEdit);

    QHBoxLayout *savedInstructionsLayout = new QHBoxLayout();
    savedInstructionsLayout->setSpacing(4);

    QLabel *savedLabel = new QLabel(Tr::tr("Or Load saved:"), this);
    savedInstructionsLayout->addWidget(savedLabel);

    m_commandsComboBox = new QComboBox(this);
    m_commandsComboBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_commandsComboBox->setEditable(true);
    m_commandsComboBox->setInsertPolicy(QComboBox::NoInsert);
    m_commandsComboBox->lineEdit()->setPlaceholderText(Tr::tr("Search saved instructions..."));

    QCompleter *completer = new QCompleter(this);
    completer->setCompletionMode(QCompleter::PopupCompletion);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchContains);
    m_commandsComboBox->setCompleter(completer);

    savedInstructionsLayout->addWidget(m_commandsComboBox);

    m_addCommandButton = new QToolButton(this);
    m_addCommandButton->setText("+");
    m_addCommandButton->setToolTip(Tr::tr("Add Custom Instruction"));
    m_addCommandButton->setFocusPolicy(Qt::NoFocus);
    savedInstructionsLayout->addWidget(m_addCommandButton);

    m_editCommandButton = new QToolButton(this);
    m_editCommandButton->setText("✎");
    m_editCommandButton->setToolTip(Tr::tr("Edit Custom Instruction"));
    m_editCommandButton->setFocusPolicy(Qt::NoFocus);
    savedInstructionsLayout->addWidget(m_editCommandButton);

    m_deleteCommandButton = new QToolButton(this);
    m_deleteCommandButton->setText("−");
    m_deleteCommandButton->setToolTip(Tr::tr("Delete Custom Instruction"));
    m_deleteCommandButton->setFocusPolicy(Qt::NoFocus);
    savedInstructionsLayout->addWidget(m_deleteCommandButton);

    m_openFolderButton = new QToolButton(this);
    m_openFolderButton->setText("📁");
    m_openFolderButton->setToolTip(Tr::tr("Open Instructions Folder"));
    m_openFolderButton->setFocusPolicy(Qt::NoFocus);
    savedInstructionsLayout->addWidget(m_openFolderButton);

    mainLayout->addLayout(savedInstructionsLayout);

    connect(
        m_instructionEdit,
        &QPlainTextEdit::textChanged,
        this,
        &QuickRefactorDialog::updateDialogSize);
    connect(
        m_commandsComboBox,
        QOverload<int>::of(&QComboBox::currentIndexChanged),
        this,
        &QuickRefactorDialog::onCommandSelected);
    connect(m_addCommandButton, &QToolButton::clicked, this, &QuickRefactorDialog::onAddCustomCommand);
    connect(
        m_editCommandButton, &QToolButton::clicked, this, &QuickRefactorDialog::onEditCustomCommand);
    connect(
        m_deleteCommandButton,
        &QToolButton::clicked,
        this,
        &QuickRefactorDialog::onDeleteCustomCommand);
    connect(
        m_openFolderButton,
        &QToolButton::clicked,
        this,
        &QuickRefactorDialog::onOpenInstructionsFolder);

    loadCustomCommands();

    QDialogButtonBox *buttonBox
        = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QuickRefactorDialog::validateAndAccept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    QPushButton *okButton = buttonBox->button(QDialogButtonBox::Ok);
    QPushButton *cancelButton = buttonBox->button(QDialogButtonBox::Cancel);

    if (!m_refactorAgentAvailable) {
        if (okButton) {
            okButton->setEnabled(false);
            okButton->setToolTip(Tr::tr("Assign a Quick Refactor agent in the Pipelines settings"));
        }

        QLabel *agentHint = new QLabel(
            Tr::tr(
                "No Quick Refactor agent is set. "
                "<a href=\"pipelines\">Assign one in the Pipelines settings</a>."),
            this);
        agentHint->setWordWrap(true);
        agentHint->setTextInteractionFlags(
            Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard);
        connect(agentHint, &QLabel::linkActivated, this, [] {
            Settings::showSettings(Constants::QODE_ASSIST_GENERAL_SETTINGS_PAGE_ID);
        });
        mainLayout->addWidget(agentHint);
    }

    mainLayout->addWidget(buttonBox);

    if (okButton) {
        okButton->installEventFilter(this);
    }
    if (cancelButton) {
        cancelButton->installEventFilter(this);
    }

    setTabOrder(m_instructionEdit, m_commandsComboBox);
    setTabOrder(m_commandsComboBox, okButton);
    setTabOrder(okButton, cancelButton);
}

void QuickRefactorDialog::createActionButtons()
{
    Utils::Icon REPEAT_ICON(
        {{":/resources/images/repeat-last-instruct-icon.png", Utils::Theme::IconsBaseColor}});
    Utils::Icon IMPROVE_ICON(
        {{":/resources/images/improve-current-code-icon.png", Utils::Theme::IconsBaseColor}});
    Utils::Icon ALTER_ICON(
        {{":/resources/images/suggest-new-icon.png", Utils::Theme::IconsBaseColor}});
    m_repeatButton = new QToolButton(this);
    m_repeatButton->setIcon(REPEAT_ICON.icon());
    m_repeatButton->setToolTip(Tr::tr("Repeat Last Instructions"));
    m_repeatButton->setEnabled(!m_lastInstructions.isEmpty());
    connect(m_repeatButton, &QToolButton::clicked, this, &QuickRefactorDialog::useLastInstructions);

    m_improveButton = new QToolButton(this);
    m_improveButton->setIcon(IMPROVE_ICON.icon());
    m_improveButton->setToolTip(Tr::tr("Improve Current Code"));
    connect(
        m_improveButton, &QToolButton::clicked, this, &QuickRefactorDialog::useImproveCodeTemplate);

    m_alternativeButton = new QToolButton(this);
    m_alternativeButton->setIcon(ALTER_ICON.icon());
    m_alternativeButton->setToolTip(Tr::tr("Suggest Alternative Solution"));
    connect(
        m_alternativeButton,
        &QToolButton::clicked,
        this,
        &QuickRefactorDialog::useAlternativeSolutionTemplate);
}

QString QuickRefactorDialog::instructions() const
{
    return m_instructionEdit->toPlainText().trimmed();
}

void QuickRefactorDialog::keyPressEvent(QKeyEvent *event)
{
    QDialog::keyPressEvent(event);
}

bool QuickRefactorDialog::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);

        if (watched == m_instructionEdit) {
            if (keyEvent->key() == Qt::Key_Tab) {
                m_commandsComboBox->setFocus();
                return true;
            }
        }
        
        if (watched == m_commandsComboBox || watched == m_commandsComboBox->lineEdit()) {
            if (keyEvent->key() == Qt::Key_Tab) {
                QPushButton *okButton = findChild<QPushButton *>();
                if (okButton && okButton->text() == "OK") {
                    okButton->setFocus();
                } else {
                    focusNextChild();
                }
                return true;
            }
        }
    }
    return QDialog::eventFilter(watched, event);
}

void QuickRefactorDialog::useLastInstructions()
{
    if (!m_lastInstructions.isEmpty()) {
        m_commandsComboBox->setCurrentIndex(0);
        m_instructionEdit->setPlainText(m_lastInstructions);
    }
    accept();
}

void QuickRefactorDialog::useImproveCodeTemplate()
{
    m_commandsComboBox->setCurrentIndex(0);
    m_instructionEdit->setPlainText(
        Tr::tr(
            "Improve the selected code by enhancing readability, efficiency, and maintainability. "
            "Follow best practices for C++/Qt and fix any potential issues."));
    accept();
}

void QuickRefactorDialog::useAlternativeSolutionTemplate()
{
    m_commandsComboBox->setCurrentIndex(0);
    m_instructionEdit->setPlainText(
        Tr::tr(
            "Suggest an alternative implementation approach for the selected code. "
            "Provide a different solution that might be cleaner, more efficient, "
            "or uses different Qt/C++ patterns or idioms."));
    accept();
}

void QuickRefactorDialog::updateDialogSize()
{
    QString text = m_instructionEdit->toPlainText();

    QFontMetrics fm(m_instructionEdit->font());

    QStringList lines = text.split('\n');
    int lineCount = qMax(lines.size(), 3);

    m_instructionEdit->setMaximumHeight(QWIDGETSIZE_MAX);

    int lineHeight = fm.height() + 2;
    int textEditHeight = qMin(qMax(lineCount, 3) * lineHeight, 15 * lineHeight);
    m_instructionEdit->setMinimumHeight(textEditHeight);

    int maxWidth = 500;
    for (const QString &line : lines) {
        int lineWidth = fm.horizontalAdvance(line) + 30;
        maxWidth = qMax(maxWidth, qMin(lineWidth, 800));
    }

    QScreen *screen = QApplication::primaryScreen();
    QRect screenGeometry = screen->availableGeometry();

    int newWidth = qMin(maxWidth + 40, screenGeometry.width() * 3 / 4);
    int newHeight = qMin(m_instructionEdit->minimumHeight() + 200, screenGeometry.height() * 3 / 4);

    resize(newWidth, newHeight);
}

void QuickRefactorDialog::loadCustomCommands()
{
    m_commandsComboBox->clear();
    m_commandsComboBox->addItem("", QString());

    auto &manager = CustomInstructionsManager::instance();
    const QVector<CustomInstruction> &instructions = manager.instructions();

    QStringList instructionNames;

    for (const CustomInstruction &instruction : instructions) {
        m_commandsComboBox->addItem(instruction.name, instruction.id);
        instructionNames.append(instruction.name);
    }

    if (m_commandsComboBox->completer()) {
        QStringListModel *model = new QStringListModel(instructionNames, this);
        m_commandsComboBox->completer()->setModel(model);
    }

    bool hasInstructions = !instructions.isEmpty();
    m_editCommandButton->setEnabled(hasInstructions);
    m_deleteCommandButton->setEnabled(hasInstructions);
}

CustomInstruction QuickRefactorDialog::findCurrentInstruction() const
{
    QString currentText = m_commandsComboBox->currentText().trimmed();
    if (currentText.isEmpty()) {
        return CustomInstruction();
    }

    auto &manager = CustomInstructionsManager::instance();
    const QVector<CustomInstruction> &instructions = manager.instructions();

    for (const CustomInstruction &instruction : instructions) {
        if (instruction.name == currentText) {
            return instruction;
        }
    }

    int currentIndex = m_commandsComboBox->currentIndex();
    if (currentIndex > 0) {
        QString instructionId = m_commandsComboBox->itemData(currentIndex).toString();
        if (!instructionId.isEmpty()) {
            return manager.getInstructionById(instructionId);
        }
    }

    return CustomInstruction();
}

void QuickRefactorDialog::onCommandSelected(int index)
{
    if (index <= 0) {
        return;
    }

    CustomInstruction instruction = findCurrentInstruction();
    if (!instruction.id.isEmpty()) {
        m_instructionEdit->setPlainText(instruction.body);
    }
}

void QuickRefactorDialog::onAddCustomCommand()
{
    AddCustomInstructionDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        CustomInstruction instruction = dialog.getInstruction();
        auto &manager = CustomInstructionsManager::instance();

        if (manager.saveInstruction(instruction)) {
            loadCustomCommands();
            m_commandsComboBox->setCurrentText(instruction.name);
        } else {
            QMessageBox::warning(
                this,
                Tr::tr("Error"),
                Tr::tr("Failed to save custom instruction. Check logs for details."));
        }
    }
}

void QuickRefactorDialog::onEditCustomCommand()
{
    CustomInstruction instruction = findCurrentInstruction();

    if (instruction.id.isEmpty()) {
        QMessageBox::information(
            this,
            Tr::tr("No Instruction Selected"),
            Tr::tr("Please select an instruction to edit."));
        return;
    }

    AddCustomInstructionDialog dialog(instruction, this);
    if (dialog.exec() == QDialog::Accepted) {
        CustomInstruction updatedInstruction = dialog.getInstruction();
        auto &manager = CustomInstructionsManager::instance();

        if (manager.saveInstruction(updatedInstruction)) {
            loadCustomCommands();
            m_commandsComboBox->setCurrentText(updatedInstruction.name);
        } else {
            QMessageBox::warning(
                this,
                Tr::tr("Error"),
                Tr::tr("Failed to update custom instruction. Check logs for details."));
        }
    }
}

void QuickRefactorDialog::onDeleteCustomCommand()
{
    CustomInstruction instruction = findCurrentInstruction();

    if (instruction.id.isEmpty()) {
        QMessageBox::information(
            this,
            Tr::tr("No Instruction Selected"),
            Tr::tr("Please select an instruction to delete."));
        return;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        Tr::tr("Confirm Deletion"),
        Tr::tr("Are you sure you want to delete the instruction '%1'?").arg(instruction.name),
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        auto &manager = CustomInstructionsManager::instance();
        if (manager.deleteInstruction(instruction.id)) {
            loadCustomCommands();
            m_commandsComboBox->setCurrentIndex(0);
            m_commandsComboBox->clearEditText();
        } else {
            QMessageBox::warning(
                this,
                Tr::tr("Error"),
                Tr::tr("Failed to delete custom instruction. Check logs for details."));
        }
    }
}

void QuickRefactorDialog::onOpenInstructionsFolder()
{
    QString path = QString("%1/qodeassist/quick_refactor/instructions")
                       .arg(Core::ICore::userResourcePath().toFSPathString());

    QDir dir(path);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    QUrl url = QUrl::fromLocalFile(dir.absolutePath());
    QDesktopServices::openUrl(url);
}

void QuickRefactorDialog::onOpenSettings()
{
    Settings::showSettings(Constants::QODE_ASSIST_QUICK_REFACTOR_SETTINGS_PAGE_ID);
}

void QuickRefactorDialog::validateAndAccept()
{
    QString instruction = m_instructionEdit->toPlainText().trimmed();

    if (instruction.isEmpty()) {
        QMessageBox::warning(
            this,
            Tr::tr("No Instruction"),
            Tr::tr("Please type an instruction or select a saved one."));
        m_instructionEdit->setFocus();
        return;
    }

    accept();
}

} // namespace QodeAssist
