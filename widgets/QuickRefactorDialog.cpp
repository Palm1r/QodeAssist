/* 
 * Copyright (C) 2025 Petr Mironychev
 *
 * This file is part of QodeAssist.
 *
 * QodeAssist is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * QodeAssist is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with QodeAssist. If not, see <https://www.gnu.org/licenses/>.
 */

#include "QuickRefactorDialog.hpp"
#include "AddCustomInstructionDialog.hpp"
#include "CustomInstructionsManager.hpp"
#include "QodeAssisttr.h"

#include <QApplication>
#include <QComboBox>
#include <QCompleter>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QScreen>
#include <QStringListModel>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

#include <coreplugin/icore.h>

#include <utils/theme/theme.h>
#include <utils/utilsicons.h>

namespace QodeAssist {

QuickRefactorDialog::QuickRefactorDialog(QWidget *parent, const QString &lastInstructions)
    : QDialog(parent)
    , m_lastInstructions(lastInstructions)
{
    setWindowTitle(Tr::tr("Quick Refactor"));
    setupUi();

    QTimer::singleShot(0, this, &QuickRefactorDialog::updateDialogSize);
    m_textEdit->installEventFilter(this);
    updateDialogSize();

    m_commandsComboBox->setFocus();
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
    mainLayout->addLayout(actionsLayout);

    QHBoxLayout *instructionsLayout = new QHBoxLayout();
    instructionsLayout->setSpacing(4);

    QLabel *instructionsLabel = new QLabel(Tr::tr("Custom Instructions:"), this);
    instructionsLayout->addWidget(instructionsLabel);

    m_commandsComboBox = new QComboBox(this);
    m_commandsComboBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_commandsComboBox->setEditable(true);
    m_commandsComboBox->setInsertPolicy(QComboBox::NoInsert);
    m_commandsComboBox->lineEdit()->setPlaceholderText("Search or select instruction...");
    
    QCompleter *completer = new QCompleter(this);
    completer->setCompletionMode(QCompleter::PopupCompletion);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchContains);
    m_commandsComboBox->setCompleter(completer);
    
    instructionsLayout->addWidget(m_commandsComboBox);

    m_addCommandButton = new QToolButton(this);
    m_addCommandButton->setText("+");
    m_addCommandButton->setToolTip(Tr::tr("Add Custom Instruction"));
    instructionsLayout->addWidget(m_addCommandButton);

    m_editCommandButton = new QToolButton(this);
    m_editCommandButton->setText("✎");
    m_editCommandButton->setToolTip(Tr::tr("Edit Custom Instruction"));
    instructionsLayout->addWidget(m_editCommandButton);

    m_deleteCommandButton = new QToolButton(this);
    m_deleteCommandButton->setText("−");
    m_deleteCommandButton->setToolTip(Tr::tr("Delete Custom Instruction"));
    instructionsLayout->addWidget(m_deleteCommandButton);

    m_openFolderButton = new QToolButton(this);
    m_openFolderButton->setText("📁");
    m_openFolderButton->setToolTip(Tr::tr("Open Instructions Folder"));
    instructionsLayout->addWidget(m_openFolderButton);

    mainLayout->addLayout(instructionsLayout);

    m_instructionsLabel = new QLabel(Tr::tr("Additional instructions (optional):"), this);
    mainLayout->addWidget(m_instructionsLabel);

    m_textEdit = new QPlainTextEdit(this);
    m_textEdit->setMinimumHeight(100);
    m_textEdit->setPlaceholderText(Tr::tr("Add extra details or modifications to the selected instruction..."));

    connect(m_textEdit, &QPlainTextEdit::textChanged, this, &QuickRefactorDialog::updateDialogSize);
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

    mainLayout->addWidget(m_textEdit);

    loadCustomCommands();

    QDialogButtonBox *buttonBox
        = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);
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
    QString result;
    
    CustomInstruction instruction = findCurrentInstruction();
    if (!instruction.id.isEmpty()) {
        result = instruction.body;
    }
    
    QString additionalText = m_textEdit->toPlainText().trimmed();
    if (!additionalText.isEmpty()) {
        if (!result.isEmpty()) {
            result += "\n\n";
        }
        result += additionalText;
    }
    
    return result;
}

void QuickRefactorDialog::setInstructions(const QString &instructions)
{
    m_textEdit->setPlainText(instructions);
}

QuickRefactorDialog::Action QuickRefactorDialog::selectedAction() const
{
    return m_selectedAction;
}

bool QuickRefactorDialog::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_textEdit && event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            if (keyEvent->modifiers() & Qt::ShiftModifier) {
                return false;
            }

            accept();
            return true;
        }
    }
    return QDialog::eventFilter(watched, event);
}

void QuickRefactorDialog::useLastInstructions()
{
    if (!m_lastInstructions.isEmpty()) {
        m_commandsComboBox->setCurrentIndex(0);
        m_commandsComboBox->clearEditText(); // Clear search text
        m_textEdit->setPlainText(m_lastInstructions);
        m_selectedAction = Action::RepeatLast;
    }
    accept();
}

void QuickRefactorDialog::useImproveCodeTemplate()
{
    m_commandsComboBox->setCurrentIndex(0);
    m_commandsComboBox->clearEditText(); // Clear search text
    m_textEdit->setPlainText(Tr::tr(
        "Improve the selected code by enhancing readability, efficiency, and maintainability. "
        "Follow best practices for C++/Qt and fix any potential issues."));
    m_selectedAction = Action::ImproveCode;
    accept();
}

void QuickRefactorDialog::useAlternativeSolutionTemplate()
{
    m_commandsComboBox->setCurrentIndex(0);
    m_commandsComboBox->clearEditText(); // Clear search text
    m_textEdit->setPlainText(
        Tr::tr("Suggest an alternative implementation approach for the selected code. "
               "Provide a different solution that might be cleaner, more efficient, "
               "or uses different Qt/C++ patterns or idioms."));
    m_selectedAction = Action::AlternativeSolution;
    accept();
}

void QuickRefactorDialog::updateDialogSize()
{
    QString text = m_textEdit->toPlainText();

    QFontMetrics fm(m_textEdit->font());

    QStringList lines = text.split('\n');
    int lineCount = lines.size();

    if (lineCount <= 1) {
        int singleLineHeight = fm.height() + 10;
        m_textEdit->setMinimumHeight(singleLineHeight);
        m_textEdit->setMaximumHeight(singleLineHeight);
    } else {
        m_textEdit->setMaximumHeight(QWIDGETSIZE_MAX);

        int lineHeight = fm.height() + 2;

        int textEditHeight = qMin(qMax(lineCount, 2) * lineHeight, 20 * lineHeight);
        m_textEdit->setMinimumHeight(textEditHeight);
    }

    int maxWidth = 500;
    for (const QString &line : lines) {
        int lineWidth = fm.horizontalAdvance(line) + 30;
        maxWidth = qMax(maxWidth, qMin(lineWidth, 800));
    }

    QScreen *screen = QApplication::primaryScreen();
    QRect screenGeometry = screen->availableGeometry();

    int newWidth = qMin(maxWidth + 40, screenGeometry.width() * 3 / 4);

    int newHeight;
    if (lineCount <= 1) {
        newHeight = 150;
    } else {
        newHeight = m_textEdit->minimumHeight() + 150;
    }
    newHeight = qMin(newHeight, screenGeometry.height() * 3 / 4);

    resize(newWidth, newHeight);
}

void QuickRefactorDialog::loadCustomCommands()
{
    m_commandsComboBox->clear();
    m_commandsComboBox->addItem("", QString()); // Empty item for no selection

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
    Q_UNUSED(index);
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
            
            m_textEdit->clear();
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
            this, Tr::tr("No Instruction Selected"), Tr::tr("Please select an instruction to edit."));
        return;
    }

    AddCustomInstructionDialog dialog(instruction, this);
    if (dialog.exec() == QDialog::Accepted) {
        CustomInstruction updatedInstruction = dialog.getInstruction();
        auto &manager = CustomInstructionsManager::instance();

        if (manager.saveInstruction(updatedInstruction)) {
            loadCustomCommands();
            m_commandsComboBox->setCurrentText(updatedInstruction.name);
            m_textEdit->clear();
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
            this, Tr::tr("No Instruction Selected"), Tr::tr("Please select an instruction to delete."));
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

} // namespace QodeAssist
