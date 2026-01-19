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

#include "settings/ConfigurationManager.hpp"
#include "settings/GeneralSettings.hpp"
#include "settings/QuickRefactorSettings.hpp"
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

static QIcon createThemedIcon(const QString &svgPath, const QColor &color)
{
    QSvgRenderer renderer(svgPath);
    if (!renderer.isValid()) {
        return QIcon();
    }

    QSize iconSize(16, 16);
    QPixmap pixmap(iconSize);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    renderer.render(&painter);
    painter.end();

    QImage image = pixmap.toImage().convertToFormat(QImage::Format_ARGB32);
    
    uchar *bits = image.bits();
    const int bytesPerPixel = 4;
    const int totalBytes = image.width() * image.height() * bytesPerPixel;
    
    const int newR = color.red();
    const int newG = color.green();
    const int newB = color.blue();
    
    for (int i = 0; i < totalBytes; i += bytesPerPixel) {
        int alpha = bits[i + 3];
        if (alpha > 0) {
            bits[i] = newB;
            bits[i + 1] = newG;
            bits[i + 2] = newR;
        }
    }

    return QIcon(QPixmap::fromImage(image));
}

QuickRefactorDialog::QuickRefactorDialog(QWidget *parent, const QString &lastInstructions)
    : QDialog(parent)
    , m_lastInstructions(lastInstructions)
{
    setWindowTitle(Tr::tr("Quick Refactor"));
    setupUi();

    QTimer::singleShot(0, this, &QuickRefactorDialog::updateDialogSize);
    m_quickInstructionEdit->installEventFilter(this);
    m_textEdit->installEventFilter(this);
    updateDialogSize();

    m_quickInstructionEdit->setFocus();
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

    m_configComboBox = new QComboBox(this);
    m_configComboBox->setMinimumWidth(200);
    m_configComboBox->setToolTip(Tr::tr("Switch AI configuration"));
    actionsLayout->addWidget(m_configComboBox);

    Utils::Theme *theme = Utils::creatorTheme();
    QColor iconColor = theme ? theme->color(Utils::Theme::TextColorNormal) : QColor(Qt::white);

    m_toolsIconOn = createThemedIcon(":/qt/qml/ChatView/icons/tools-icon-on.svg", iconColor);
    m_toolsIconOff = createThemedIcon(":/qt/qml/ChatView/icons/tools-icon-off.svg", iconColor);

    m_toolsButton = new QToolButton(this);
    m_toolsButton->setCheckable(true);
    m_toolsButton->setChecked(Settings::quickRefactorSettings().useTools());
    m_toolsButton->setIcon(m_toolsButton->isChecked() ? m_toolsIconOn : m_toolsIconOff);
    m_toolsButton->setToolTip(Tr::tr("Enable/Disable AI Tools"));
    m_toolsButton->setIconSize(QSize(16, 16));
    actionsLayout->addWidget(m_toolsButton);

    connect(m_toolsButton, &QToolButton::toggled, this, [this](bool checked) {
        m_toolsButton->setIcon(checked ? m_toolsIconOn : m_toolsIconOff);
        Settings::quickRefactorSettings().useTools.setValue(checked);
        Settings::quickRefactorSettings().writeSettings();
    });

    m_thinkingIconOn = createThemedIcon(":/qt/qml/ChatView/icons/thinking-icon-on.svg", iconColor);
    m_thinkingIconOff = createThemedIcon(":/qt/qml/ChatView/icons/thinking-icon-off.svg", iconColor);

    m_thinkingButton = new QToolButton(this);
    m_thinkingButton->setCheckable(true);
    m_thinkingButton->setChecked(Settings::quickRefactorSettings().useThinking());
    m_thinkingButton->setIcon(m_thinkingButton->isChecked() ? m_thinkingIconOn : m_thinkingIconOff);
    m_thinkingButton->setToolTip(Tr::tr("Enable/Disable Thinking Mode"));
    m_thinkingButton->setIconSize(QSize(16, 16));
    actionsLayout->addWidget(m_thinkingButton);

    connect(m_thinkingButton, &QToolButton::toggled, this, [this](bool checked) {
        m_thinkingButton->setIcon(checked ? m_thinkingIconOn : m_thinkingIconOff);
        Settings::quickRefactorSettings().useThinking.setValue(checked);
        Settings::quickRefactorSettings().writeSettings();
    });

    m_settingsButton = new QToolButton(this);
    m_settingsButton->setIcon(Utils::Icons::SETTINGS_TOOLBAR.icon());
    m_settingsButton->setToolTip(Tr::tr("Open Quick Refactor Settings"));
    m_settingsButton->setIconSize(QSize(16, 16));
    actionsLayout->addWidget(m_settingsButton);

    connect(m_settingsButton, &QToolButton::clicked, this, &QuickRefactorDialog::onOpenSettings);

    mainLayout->addLayout(actionsLayout);

    QLabel *quickInstructionLabel = new QLabel(Tr::tr("Quick Instruction:"), this);
    mainLayout->addWidget(quickInstructionLabel);

    m_quickInstructionEdit = new QLineEdit(this);
    m_quickInstructionEdit->setPlaceholderText(Tr::tr("Type your instruction here..."));
    mainLayout->addWidget(m_quickInstructionEdit);

    QHBoxLayout *savedInstructionsLayout = new QHBoxLayout();
    savedInstructionsLayout->setSpacing(4);

    QLabel *savedLabel = new QLabel(Tr::tr("Or select saved:"), this);
    savedInstructionsLayout->addWidget(savedLabel);

    m_commandsComboBox = new QComboBox(this);
    m_commandsComboBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    savedInstructionsLayout->addWidget(m_commandsComboBox);

    m_addCommandButton = new QToolButton(this);
    m_addCommandButton->setText("+");
    m_addCommandButton->setToolTip(Tr::tr("Add Custom Instruction"));
    savedInstructionsLayout->addWidget(m_addCommandButton);

    m_editCommandButton = new QToolButton(this);
    m_editCommandButton->setText("✎");
    m_editCommandButton->setToolTip(Tr::tr("Edit Custom Instruction"));
    savedInstructionsLayout->addWidget(m_editCommandButton);

    m_deleteCommandButton = new QToolButton(this);
    m_deleteCommandButton->setText("−");
    m_deleteCommandButton->setToolTip(Tr::tr("Delete Custom Instruction"));
    savedInstructionsLayout->addWidget(m_deleteCommandButton);

    m_openFolderButton = new QToolButton(this);
    m_openFolderButton->setText("📁");
    m_openFolderButton->setToolTip(Tr::tr("Open Instructions Folder"));
    savedInstructionsLayout->addWidget(m_openFolderButton);

    mainLayout->addLayout(savedInstructionsLayout);

    QFrame *separator = new QFrame(this);
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(separator);

    m_instructionsLabel = new QLabel(Tr::tr("Additional Context (optional):"), this);
    mainLayout->addWidget(m_instructionsLabel);

    m_textEdit = new QPlainTextEdit(this);
    m_textEdit->setMinimumHeight(60);
    m_textEdit->setPlaceholderText(Tr::tr("Add extra details or context..."));

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
    loadAvailableConfigurations();

    connect(
        m_configComboBox,
        QOverload<int>::of(&QComboBox::currentIndexChanged),
        this,
        &QuickRefactorDialog::onConfigurationChanged);

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
    
    QString quickInstruction = m_quickInstructionEdit->text().trimmed();
    if (!quickInstruction.isEmpty()) {
        result = quickInstruction;
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
    m_quickInstructionEdit->setText(instructions);
}

QuickRefactorDialog::Action QuickRefactorDialog::selectedAction() const
{
    return m_selectedAction;
}

bool QuickRefactorDialog::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            if (watched == m_quickInstructionEdit) {
                accept();
                return true;
            } else if (watched == m_textEdit) {
                if (keyEvent->modifiers() & Qt::ShiftModifier) {
                    return false;
                }
                accept();
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
        m_quickInstructionEdit->setText(m_lastInstructions);
        m_textEdit->clear();
        m_selectedAction = Action::RepeatLast;
    }
    accept();
}

void QuickRefactorDialog::useImproveCodeTemplate()
{
    m_commandsComboBox->setCurrentIndex(0);
    m_quickInstructionEdit->setText(Tr::tr(
        "Improve the selected code by enhancing readability, efficiency, and maintainability. "
        "Follow best practices for C++/Qt and fix any potential issues."));
    m_textEdit->clear();
    m_selectedAction = Action::ImproveCode;
    accept();
}

void QuickRefactorDialog::useAlternativeSolutionTemplate()
{
    m_commandsComboBox->setCurrentIndex(0);
    m_quickInstructionEdit->setText(
        Tr::tr("Suggest an alternative implementation approach for the selected code. "
               "Provide a different solution that might be cleaner, more efficient, "
               "or uses different Qt/C++ patterns or idioms."));
    m_textEdit->clear();
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
    m_commandsComboBox->addItem(Tr::tr("-- Select saved instruction --"), QString());

    auto &manager = CustomInstructionsManager::instance();
    const QVector<CustomInstruction> &instructions = manager.instructions();

    int defaultInstructionIndex = -1;
    
    for (int i = 0; i < instructions.size(); ++i) {
        const CustomInstruction &instruction = instructions[i];
        m_commandsComboBox->addItem(instruction.name, instruction.id);
        
        if (instruction.isDefault) {
            defaultInstructionIndex = i + 1;
        }
    }

    if (defaultInstructionIndex > 0) {
        m_commandsComboBox->setCurrentIndex(defaultInstructionIndex);
    }

    bool hasInstructions = !instructions.isEmpty();
    m_editCommandButton->setEnabled(hasInstructions);
    m_deleteCommandButton->setEnabled(hasInstructions);
}

CustomInstruction QuickRefactorDialog::findCurrentInstruction() const
{
    int currentIndex = m_commandsComboBox->currentIndex();
    if (currentIndex <= 0) {
        return CustomInstruction();
    }

    QString instructionId = m_commandsComboBox->itemData(currentIndex).toString();
    if (instructionId.isEmpty()) {
        return CustomInstruction();
    }

    auto &manager = CustomInstructionsManager::instance();
    return manager.getInstructionById(instructionId);
}

void QuickRefactorDialog::onCommandSelected(int index)
{
    if (index <= 0) {
        return;
    }

    CustomInstruction instruction = findCurrentInstruction();
    if (!instruction.id.isEmpty()) {
        m_quickInstructionEdit->setText(instruction.body);
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
            
            for (int i = 0; i < m_commandsComboBox->count(); ++i) {
                if (m_commandsComboBox->itemData(i).toString() == instruction.id) {
                    m_commandsComboBox->setCurrentIndex(i);
                    break;
                }
            }
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
            
            for (int i = 0; i < m_commandsComboBox->count(); ++i) {
                if (m_commandsComboBox->itemData(i).toString() == updatedInstruction.id) {
                    m_commandsComboBox->setCurrentIndex(i);
                    break;
                }
            }
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
            m_quickInstructionEdit->clear();
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
    Core::ICore::showOptionsDialog(Constants::QODE_ASSIST_QUICK_REFACTOR_SETTINGS_PAGE_ID);
}

QString QuickRefactorDialog::selectedConfiguration() const
{
    return m_selectedConfiguration;
}

void QuickRefactorDialog::loadAvailableConfigurations()
{
    auto &manager = Settings::ConfigurationManager::instance();
    manager.loadConfigurations(Settings::ConfigurationType::QuickRefactor);

    QVector<Settings::AIConfiguration> configs
        = manager.configurations(Settings::ConfigurationType::QuickRefactor);

    m_configComboBox->clear();
    m_configComboBox->addItem(Tr::tr("Current"), QString());

    for (const Settings::AIConfiguration &config : configs) {
        m_configComboBox->addItem(config.name, config.id);
    }

    auto &settings = Settings::generalSettings();
    QString currentProvider = settings.qrProvider.value();
    QString currentModel = settings.qrModel.value();
    QString currentConfigText = QString("%1/%2").arg(currentProvider, currentModel);
    m_configComboBox->setItemText(0, Tr::tr("Current (%1)").arg(currentConfigText));
}

void QuickRefactorDialog::onConfigurationChanged(int index)
{
    if (index == 0) {
        m_selectedConfiguration.clear();
        return;
    }

    QString configId = m_configComboBox->itemData(index).toString();
    m_selectedConfiguration = m_configComboBox->itemText(index);

    auto &manager = Settings::ConfigurationManager::instance();
    Settings::AIConfiguration config
        = manager.getConfigurationById(configId, Settings::ConfigurationType::QuickRefactor);

    if (!config.id.isEmpty()) {
        auto &settings = Settings::generalSettings();

        settings.qrProvider.setValue(config.provider);
        settings.qrModel.setValue(config.model);
        settings.qrTemplate.setValue(config.templateName);
        settings.qrUrl.setValue(config.url);
        settings.qrEndpointMode.setValue(
            settings.qrEndpointMode.indexForDisplay(config.endpointMode));
        settings.qrCustomEndpoint.setValue(config.customEndpoint);

        settings.writeSettings();
    }
}

} // namespace QodeAssist
