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

#include "RefactorWidget.hpp"

#include <texteditor/textdocument.h>
#include <texteditor/syntaxhighlighter.h>

#include <QCloseEvent>
#include <QEnterEvent>
#include <QEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPainter>
#include <QRegion>
#include <QScrollBar>
#include <QSharedPointer>
#include <QSplitter>
#include <QTextBlock>
#include <QVBoxLayout>

#include <utils/differ.h>
#include <utils/plaintextedit/plaintextedit.h>
#include <utils/theme/theme.h>

namespace QodeAssist {

namespace {
QString createButtonStyleSheet(const QColor &normalBg, const QColor &textColor, 
                                const QColor &borderColor, const QColor &hoverBg,
                                const QColor &selectedBg, bool boldText = false,
                                const QColor &checkedBg = QColor(), const QColor &checkedTextColor = QColor())
{
    QString style = QString(
        "QPushButton {"
        "    background-color: %1;"
        "    color: %2;"
        "    border: 1px solid %3;"
        "    border-radius: 3px;"
        "    padding: 3px 10px;"
        "    font-size: 11px;"
        "%4"
        "}"
        "QPushButton:hover {"
        "    background-color: %5;"
        "    border: 1px solid %2;"
        "}"
        "QPushButton:pressed {"
        "    background-color: %6;"
        "}")
        .arg(normalBg.name(), 
             textColor.name(), 
             borderColor.name(),
             boldText ? "    font-weight: bold;" : "",
             hoverBg.name(),
             selectedBg.name());
    
    if (checkedBg.isValid() && checkedTextColor.isValid()) {
        style += QString(
            "\nQPushButton:checked {"
            "    background-color: %1;"
            "    color: %2;"
            "    font-weight: bold;"
            "}")
            .arg(checkedBg.name(), checkedTextColor.name());
    }
    
    return style;
}
} // anonymous namespace

CustomSplitterHandle::CustomSplitterHandle(Qt::Orientation orientation, QSplitter *parent)
    : QSplitterHandle(orientation, parent)
{
    if (orientation == Qt::Horizontal) {
        setCursor(Qt::SplitHCursor);
    } else {
        setCursor(Qt::SplitVCursor);
    }
    
    setMouseTracking(true);
}

void CustomSplitterHandle::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    QColor bgColor = Utils::creatorColor(Utils::Theme::BackgroundColorHover);
    if (m_hovered) {
        bgColor.setAlpha(150);
    } else {
        bgColor.setAlpha(50);
    }
    painter.fillRect(rect(), bgColor);
    
    QColor lineColor = Utils::creatorColor(Utils::Theme::SplitterColor);
    lineColor.setAlpha(m_hovered ? 255 : 180);
    
    painter.setPen(QPen(lineColor, m_hovered ? 3 : 2));
    
    if (orientation() == Qt::Horizontal) {
        int x = width() / 2;
        painter.drawLine(x, 10, x, height() - 10);
        
        painter.setBrush(lineColor);
        int centerY = height() / 2;
        int dotSize = m_hovered ? 3 : 2;
        for (int i = -2; i <= 2; ++i) {
            painter.drawEllipse(QPoint(x, centerY + i * 8), dotSize, dotSize);
        }
    } else {
        int y = height() / 2;
        painter.drawLine(10, y, width() - 10, y);
    }
}

void CustomSplitterHandle::enterEvent(QEnterEvent *event)
{
    Q_UNUSED(event)
    m_hovered = true;
    update();
    QSplitterHandle::enterEvent(event);
}

void CustomSplitterHandle::leaveEvent(QEvent *event)
{
    m_hovered = false;
    update();
    QSplitterHandle::leaveEvent(event);
}

CustomSplitter::CustomSplitter(Qt::Orientation orientation, QWidget *parent)
    : QSplitter(orientation, parent)
{
}

QSplitterHandle *CustomSplitter::createHandle()
{
    return new CustomSplitterHandle(orientation(), this);
}

RefactorWidget::RefactorWidget(TextEditor::TextEditorWidget *sourceEditor, QWidget *parent)
    : QWidget(parent)
    , m_sourceEditor(sourceEditor)
    , m_leftEditor(nullptr)
    , m_rightEditor(nullptr)
    , m_leftContainer(nullptr)
    , m_splitter(nullptr)
    , m_statsLabel(nullptr)
    , m_toggleOriginalButton(nullptr)
    , m_applyButton(nullptr)
    , m_declineButton(nullptr)
    , m_editorWidth(800)
    , m_syncingScroll(false)
    , m_linesAdded(0)
    , m_linesRemoved(0)
{
    setupUi();
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_DeleteOnClose);
    setFocusPolicy(Qt::StrongFocus);
}

RefactorWidget::~RefactorWidget()
{
}

void RefactorWidget::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(4);

    m_statsLabel = new QLabel(this);
    m_statsLabel->setStyleSheet(QString(
        "color: %1; font-size: 11px; font-weight: bold; padding: 4px 6px; "
        "background-color: %2; border-radius: 3px;")
        .arg(Utils::creatorColor(Utils::Theme::TextColorNormal).name())
        .arg(Utils::creatorColor(Utils::Theme::BackgroundColorHover).name()));
    m_statsLabel->setAlignment(Qt::AlignLeft);
    mainLayout->addWidget(m_statsLabel);

    m_leftDocument = QSharedPointer<TextEditor::TextDocument>::create();
    m_rightDocument = QSharedPointer<TextEditor::TextDocument>::create();
    
    m_splitter = new CustomSplitter(Qt::Horizontal, this);
    m_splitter->setChildrenCollapsible(false);
    m_splitter->setHandleWidth(16);
    m_splitter->setStyleSheet("QSplitter::handle { background-color: transparent; }");
    
    m_leftEditor = new TextEditor::TextEditorWidget();
    m_leftEditor->setTextDocument(m_leftDocument);
    m_leftEditor->setReadOnly(true);
    m_leftEditor->setFrameStyle(QFrame::StyledPanel);
    m_leftEditor->setLineWrapMode(Utils::PlainTextEdit::NoWrap);
    m_leftEditor->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_leftEditor->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_leftEditor->setMinimumHeight(120);
    m_leftEditor->setMinimumWidth(200);
    m_leftEditor->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_leftEditor->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    
    m_rightEditor = new TextEditor::TextEditorWidget();
    m_rightEditor->setTextDocument(m_rightDocument);
    m_rightEditor->setReadOnly(false);
    m_rightEditor->setFrameStyle(QFrame::StyledPanel);
    m_rightEditor->setLineWrapMode(Utils::PlainTextEdit::NoWrap);
    m_rightEditor->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_rightEditor->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_rightEditor->setMinimumHeight(120);
    m_rightEditor->setMinimumWidth(200);
    m_rightEditor->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    m_leftContainer = new QWidget();
    m_leftContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto *leftLayout = new QVBoxLayout(m_leftContainer);
    leftLayout->setSpacing(2);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    
    auto *originalLabel = new QLabel(tr("◄ Original"), m_leftContainer);
    originalLabel->setStyleSheet("color: " + Utils::creatorColor(Utils::Theme::TextColorDisabled).name() + 
                                "; font-size: 10px; padding: 2px 4px;");
    leftLayout->addWidget(originalLabel);
    leftLayout->addWidget(m_leftEditor);
    
    auto *rightContainer = new QWidget();
    rightContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto *rightLayout = new QVBoxLayout(rightContainer);
    rightLayout->setSpacing(2);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    
    auto *refactoredLabel = new QLabel(tr("Refactored ►"), rightContainer);
    refactoredLabel->setStyleSheet("color: " + Utils::creatorColor(Utils::Theme::TextColorDisabled).name() + 
                                  "; font-size: 10px; padding: 2px 4px;");
    rightLayout->addWidget(refactoredLabel);
    rightLayout->addWidget(m_rightEditor);
    
    m_splitter->addWidget(m_leftContainer);
    m_splitter->addWidget(rightContainer);
    
    m_splitter->setStretchFactor(0, 1);
    m_splitter->setStretchFactor(1, 1);
    
    mainLayout->addWidget(m_splitter);
    
    connect(m_leftEditor->verticalScrollBar(), &QScrollBar::valueChanged,
            this, &RefactorWidget::syncLeftScroll);
    connect(m_rightEditor->verticalScrollBar(), &QScrollBar::valueChanged,
            this, &RefactorWidget::syncRightScroll);
    connect(m_leftEditor->horizontalScrollBar(), &QScrollBar::valueChanged,
            this, &RefactorWidget::syncLeftHorizontalScroll);
    connect(m_rightEditor->horizontalScrollBar(), &QScrollBar::valueChanged,
            this, &RefactorWidget::syncRightHorizontalScroll);
    
    connect(m_rightDocument->document(), &QTextDocument::contentsChanged,
            this, &RefactorWidget::onRightEditorTextChanged);

    auto *buttonLayout = new QHBoxLayout();
    buttonLayout->setContentsMargins(0, 2, 0, 0);
    buttonLayout->setSpacing(6);
    
    const QColor normalBg = Utils::creatorColor(Utils::Theme::BackgroundColorNormal);
    const QColor borderColor = Utils::creatorColor(Utils::Theme::SplitterColor);
    const QColor hoverBg = Utils::creatorColor(Utils::Theme::BackgroundColorHover);
    const QColor selectedBg = Utils::creatorColor(Utils::Theme::BackgroundColorSelected);
    const QColor successColor = Utils::creatorColor(Utils::Theme::TextColorNormal);
    const QColor errorColor = Utils::creatorColor(Utils::Theme::TextColorError);
    const QColor infoColor = Utils::creatorColor(Utils::Theme::TextColorDisabled);

    m_toggleOriginalButton = new QPushButton(tr("◄ Show Original"), this);
    m_toggleOriginalButton->setFocusPolicy(Qt::NoFocus);
    m_toggleOriginalButton->setCursor(Qt::PointingHandCursor);
    m_toggleOriginalButton->setMaximumHeight(26);
    m_toggleOriginalButton->setCheckable(true);
    m_toggleOriginalButton->setChecked(false);

#ifdef Q_OS_MACOS
    m_applyButton = new QPushButton(tr("✓ Apply (⌘+Enter)"), this);
#else
    m_applyButton = new QPushButton(tr("✓ Apply (Ctrl+Enter)"), this);
#endif
    m_applyButton->setFocusPolicy(Qt::NoFocus);
    m_applyButton->setCursor(Qt::PointingHandCursor);
    m_applyButton->setMaximumHeight(26);
    
    m_declineButton = new QPushButton(tr("✗ Decline (Esc)"), this);
    m_declineButton->setFocusPolicy(Qt::NoFocus);
    m_declineButton->setCursor(Qt::PointingHandCursor);
    m_declineButton->setMaximumHeight(26);
    
    m_applyButton->setStyleSheet(createButtonStyleSheet(normalBg, successColor, borderColor, 
                                                        hoverBg, selectedBg, true));
    m_declineButton->setStyleSheet(createButtonStyleSheet(normalBg, errorColor, borderColor, 
                                                          hoverBg, selectedBg, false));
    m_toggleOriginalButton->setStyleSheet(createButtonStyleSheet(normalBg, infoColor, borderColor, 
                                                                 hoverBg, selectedBg, false, 
                                                                 selectedBg, successColor));
    
    buttonLayout->addWidget(m_toggleOriginalButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_applyButton);
    buttonLayout->addWidget(m_declineButton);
    
    mainLayout->addLayout(buttonLayout);
    
    connect(m_applyButton, &QPushButton::clicked, this, &RefactorWidget::applyRefactoring);
    connect(m_declineButton, &QPushButton::clicked, this, &RefactorWidget::declineRefactoring);
    connect(m_toggleOriginalButton, &QPushButton::toggled, this, &RefactorWidget::toggleOriginalVisibility);
}

void RefactorWidget::setDiffContent(const QString &originalText, const QString &refactoredText)
{
    setDiffContent(originalText, refactoredText, QString(), QString());
}

void RefactorWidget::setDiffContent(const QString &originalText, const QString &refactoredText,
                                     const QString &contextBefore, const QString &contextAfter)
{
    m_originalText = originalText;
    m_refactoredText = refactoredText;
    m_contextBefore = contextBefore;
    m_contextAfter = contextAfter;
    
    m_leftContainer->setVisible(false);
    
    bool hasOriginal = !originalText.trimmed().isEmpty();
    m_toggleOriginalButton->setEnabled(hasOriginal);
    m_toggleOriginalButton->setChecked(false);
    
    QString leftFullText;
    QString rightFullText;
    
    if (!contextBefore.isEmpty()) {
        leftFullText = contextBefore + "\n";
        rightFullText = contextBefore + "\n";
    }
    
    leftFullText += originalText;
    rightFullText += refactoredText;
    
    if (!contextAfter.isEmpty()) {
        leftFullText += "\n" + contextAfter;
        rightFullText += "\n" + contextAfter;
    }
    
    m_leftDocument->setPlainText(leftFullText);
    m_rightDocument->setPlainText(rightFullText);
    
    applySyntaxHighlighting();
    
    if (!contextBefore.isEmpty() || !contextAfter.isEmpty()) {
        dimContextLines(contextBefore, contextAfter);
    }
    
    Utils::Differ differ;
    m_cachedDiffList = differ.diff(m_originalText, m_refactoredText);
    
    highlightDifferences();
    addLineMarkers();
    
    calculateStats();
    updateStatsLabel();
    
    updateSizeToContent();
}

void RefactorWidget::highlightDifferences()
{
    if (m_cachedDiffList.isEmpty()) {
        return;
    }
    
    QList<Utils::Diff> leftDiffs;
    QList<Utils::Diff> rightDiffs;
    Utils::Differ::splitDiffList(m_cachedDiffList, &leftDiffs, &rightDiffs);
    
    int contextBeforeOffset = m_contextBefore.isEmpty() ? 0 : (m_contextBefore.length() + 1);
    
    QColor normalTextColor = Utils::creatorColor(Utils::Theme::TextColorNormal);
    
    QTextCursor leftCursor(m_leftDocument->document());
    QTextCharFormat removedFormat;
    QColor removedBg = Utils::creatorColor(Utils::Theme::TextColorError);
    removedBg.setAlpha(30);
    removedFormat.setBackground(removedBg);
    removedFormat.setForeground(normalTextColor);
    
    int leftPos = 0;
    for (const auto &diff : leftDiffs) {
        if (diff.command == Utils::Diff::Delete) {
            leftCursor.setPosition(contextBeforeOffset + leftPos);
            leftCursor.setPosition(contextBeforeOffset + leftPos + diff.text.length(), QTextCursor::KeepAnchor);
            leftCursor.setCharFormat(removedFormat);
        }
        if (diff.command != Utils::Diff::Insert) {
            leftPos += diff.text.length();
        }
    }
    
    QTextCursor rightCursor(m_rightDocument->document());
    QTextCharFormat addedFormat;
    QColor addedBg = Utils::creatorColor(Utils::Theme::IconsRunColor);
    addedBg.setAlpha(60);
    addedFormat.setBackground(addedBg);
    addedFormat.setForeground(normalTextColor);
    
    int rightPos = 0;
    for (const auto &diff : rightDiffs) {
        if (diff.command == Utils::Diff::Insert) {
            rightCursor.setPosition(contextBeforeOffset + rightPos);
            rightCursor.setPosition(contextBeforeOffset + rightPos + diff.text.length(), QTextCursor::KeepAnchor);
            rightCursor.setCharFormat(addedFormat);
        }
        if (diff.command != Utils::Diff::Delete) {
            rightPos += diff.text.length();
        }
    }
}

void RefactorWidget::dimContextLines(const QString &contextBefore, const QString &contextAfter)
{
    QTextCharFormat dimFormat;
    QColor dimColor = Utils::creatorColor(Utils::Theme::TextColorDisabled);
    dimFormat.setForeground(dimColor);
    
    if (!contextBefore.isEmpty()) {
        int contextBeforeLines = contextBefore.count('\n');
        if (!contextBefore.endsWith('\n')) {
            contextBeforeLines++;
        }
        
        QTextCursor leftCursor(m_leftDocument->document());
        for (int i = 0; i < contextBeforeLines && leftCursor.block().isValid(); ++i) {
            leftCursor.movePosition(QTextCursor::StartOfBlock);
            leftCursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
            leftCursor.setCharFormat(dimFormat);
            leftCursor.movePosition(QTextCursor::NextBlock);
        }
        
        QTextCursor rightCursor(m_rightDocument->document());
        for (int i = 0; i < contextBeforeLines && rightCursor.block().isValid(); ++i) {
            rightCursor.movePosition(QTextCursor::StartOfBlock);
            rightCursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
            rightCursor.setCharFormat(dimFormat);
            rightCursor.movePosition(QTextCursor::NextBlock);
        }
    }
    
    if (!contextAfter.isEmpty()) {
        int contextAfterLines = contextAfter.count('\n');
        if (!contextAfter.endsWith('\n')) {
            contextAfterLines++;
        }
        
        QTextCursor leftCursor(m_leftDocument->document());
        leftCursor.movePosition(QTextCursor::End);
        for (int i = 0; i < contextAfterLines; ++i) {
            leftCursor.movePosition(QTextCursor::StartOfBlock);
            leftCursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
            leftCursor.setCharFormat(dimFormat);
            if (!leftCursor.movePosition(QTextCursor::PreviousBlock)) {
                break;
            }
        }
        
        QTextCursor rightCursor(m_rightDocument->document());
        rightCursor.movePosition(QTextCursor::End);
        for (int i = 0; i < contextAfterLines; ++i) {
            rightCursor.movePosition(QTextCursor::StartOfBlock);
            rightCursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
            rightCursor.setCharFormat(dimFormat);
            if (!rightCursor.movePosition(QTextCursor::PreviousBlock)) {
                break;
            }
        }
    }
}

QString RefactorWidget::getRefactoredText() const
{
    return m_applyText;
}

void RefactorWidget::setRange(const Utils::Text::Range &range)
{
    m_range = range;
}

void RefactorWidget::setEditorWidth(int width)
{
    m_editorWidth = width;
    updateSizeToContent();
}

void RefactorWidget::setApplyCallback(std::function<void(const QString &)> callback)
{
    m_applyCallback = callback;
}

void RefactorWidget::setDeclineCallback(std::function<void()> callback)
{
    m_declineCallback = callback;
}

void RefactorWidget::applyRefactoring()
{
    if (m_applyCallback) {
        m_applyCallback(m_applyText);
    }
    emit applied();
    close();
}

void RefactorWidget::declineRefactoring()
{
    if (m_declineCallback) {
        m_declineCallback();
    }
    emit declined();
    close();
}

void RefactorWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    const QColor bgColor = Utils::creatorColor(Utils::Theme::BackgroundColorNormal);
    const QColor borderColor = Utils::creatorColor(Utils::Theme::SplitterColor);
    
    painter.fillRect(rect(), bgColor);
    painter.setPen(QPen(borderColor, 2));
    painter.drawRoundedRect(rect().adjusted(2, 2, -2, -2), 6, 6);
}

bool RefactorWidget::event(QEvent *event)
{
    if (event->type() == QEvent::ShortcutOverride) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        
        if (((keyEvent->key() == Qt::Key_Enter || keyEvent->key() == Qt::Key_Return) && 
             keyEvent->modifiers() == Qt::ControlModifier) ||
            keyEvent->key() == Qt::Key_Escape) {
            event->accept();
            return true;
        }
    }
    
    if (event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        
        if ((keyEvent->key() == Qt::Key_Enter || keyEvent->key() == Qt::Key_Return) && 
            keyEvent->modifiers() == Qt::ControlModifier) {
            applyRefactoring();
            return true;
        }
        
        if (keyEvent->key() == Qt::Key_Escape) {
            declineRefactoring();
            return true;
        }
    }
    
    return QWidget::event(event);
}

void RefactorWidget::syncLeftScroll(int value)
{
    if (m_syncingScroll) return;
    m_syncingScroll = true;
    m_rightEditor->verticalScrollBar()->setValue(value);
    m_syncingScroll = false;
}

void RefactorWidget::syncRightScroll(int value)
{
    if (m_syncingScroll) return;
    m_syncingScroll = true;
    m_leftEditor->verticalScrollBar()->setValue(value);
    m_syncingScroll = false;
}

void RefactorWidget::syncLeftHorizontalScroll(int value)
{
    if (m_syncingScroll) return;
    m_syncingScroll = true;
    m_rightEditor->horizontalScrollBar()->setValue(value);
    m_syncingScroll = false;
}

void RefactorWidget::syncRightHorizontalScroll(int value)
{
    if (m_syncingScroll) return;
    m_syncingScroll = true;
    m_leftEditor->horizontalScrollBar()->setValue(value);
    m_syncingScroll = false;
}

void RefactorWidget::onRightEditorTextChanged()
{
    QString fullText = m_rightDocument->plainText();
    
    int startPos = 0;
    int endPos = fullText.length();
    
    if (!m_contextBefore.isEmpty()) {
        startPos = m_contextBefore.length() + 1;
    }
    
    if (!m_contextAfter.isEmpty()) {
        endPos = fullText.length() - m_contextAfter.length() - 1;
    }
    
    QString editedText = fullText.mid(startPos, endPos - startPos);
    
    if (!m_displayIndent.isEmpty()) {
        if (editedText.startsWith(m_displayIndent)) {
            editedText = editedText.mid(m_displayIndent.length());
        }
    }
    
    m_applyText = editedText;
}

void RefactorWidget::toggleOriginalVisibility()
{
    bool isVisible = m_toggleOriginalButton->isChecked();
    m_leftContainer->setVisible(isVisible);
    
    if (isVisible) {
        m_toggleOriginalButton->setText(tr("► Hide Original"));
    } else {
        m_toggleOriginalButton->setText(tr("◄ Show Original"));
    }
    
    updateSizeToContent();
}

void RefactorWidget::closeEvent(QCloseEvent *event)
{
    declineRefactoring();
    event->accept();
}

void RefactorWidget::calculateStats()
{
    m_linesAdded = 0;
    m_linesRemoved = 0;
    
    for (const auto &diff : m_cachedDiffList) {
        if (diff.command == Utils::Diff::Insert) {
            m_linesAdded += diff.text.count('\n') + (diff.text.isEmpty() ? 0 : 1);
        } else if (diff.command == Utils::Diff::Delete) {
            m_linesRemoved += diff.text.count('\n') + (diff.text.isEmpty() ? 0 : 1);
        }
    }
}

void RefactorWidget::updateStatsLabel()
{
    QString statsText;
    
    if (m_linesAdded > 0 && m_linesRemoved > 0) {
        statsText = tr("+%1 lines, -%2 lines").arg(m_linesAdded).arg(m_linesRemoved);
    } else if (m_linesAdded > 0) {
        statsText = tr("+%1 lines").arg(m_linesAdded);
    } else if (m_linesRemoved > 0) {
        statsText = tr("-%1 lines").arg(m_linesRemoved);
    } else {
        statsText = tr("No changes");
    }
    
    m_statsLabel->setText("📊 " + statsText);
}

void RefactorWidget::applySyntaxHighlighting()
{
    if (!m_sourceEditor) {
        return;
    }
    
    // Get the syntax highlighter from source editor
    auto *sourceDoc = m_sourceEditor->textDocument();
    if (!sourceDoc) {
        return;
    }
    
    auto *sourceHighlighter = sourceDoc->syntaxHighlighter();
    if (!sourceHighlighter) {
        return;
    }
    
    m_leftDocument->setMimeType(sourceDoc->mimeType());
    m_rightDocument->setMimeType(sourceDoc->mimeType());
}

void RefactorWidget::addLineMarkers()
{
    if (m_cachedDiffList.isEmpty()) {
        return;
    }
    
    QList<Utils::Diff> leftDiffs;
    QList<Utils::Diff> rightDiffs;
    Utils::Differ::splitDiffList(m_cachedDiffList, &leftDiffs, &rightDiffs);
    
    int contextBeforeOffset = m_contextBefore.isEmpty() ? 0 : (m_contextBefore.length() + 1);
    
    QColor removedMarker = Utils::creatorColor(Utils::Theme::TextColorError);
    QColor addedMarker = Utils::creatorColor(Utils::Theme::IconsRunColor);
    
    QTextCursor leftCursor(m_leftDocument->document());
    int leftPos = 0;
    
    for (const auto &diff : leftDiffs) {
        if (diff.command == Utils::Diff::Delete) {
            leftCursor.setPosition(contextBeforeOffset + leftPos);
            leftCursor.movePosition(QTextCursor::StartOfBlock);
            leftCursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
            
            QTextBlockFormat blockFormat;
            blockFormat.setBackground(QBrush(removedMarker.lighter(185)));
            blockFormat.setLeftMargin(4);
            blockFormat.setProperty(QTextFormat::FullWidthSelection, true);
            
            leftCursor.setBlockFormat(blockFormat);
        }
        if (diff.command != Utils::Diff::Insert) {
            leftPos += diff.text.length();
        }
    }
    
    QTextCursor rightCursor(m_rightDocument->document());
    int rightPos = 0;
    
    for (const auto &diff : rightDiffs) {
        if (diff.command == Utils::Diff::Insert) {
            rightCursor.setPosition(contextBeforeOffset + rightPos);
            rightCursor.movePosition(QTextCursor::StartOfBlock);
            rightCursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
            
            QTextBlockFormat blockFormat;
            blockFormat.setBackground(QBrush(addedMarker.lighter(195)));
            blockFormat.setLeftMargin(4);
            blockFormat.setProperty(QTextFormat::FullWidthSelection, true);
            
            rightCursor.setBlockFormat(blockFormat);
        }
        if (diff.command != Utils::Diff::Delete) {
            rightPos += diff.text.length();
        }
    }
}

void RefactorWidget::updateSizeToContent()
{
    QFontMetrics fm(m_rightEditor->font());
    int charWidth = fm.horizontalAdvance('m');
    
    int singleEditorWidth = charWidth * 85 + 80;
    
    int totalWidth;
    if (m_leftContainer->isVisible()) {
        totalWidth = singleEditorWidth * 2 + 30;
    } else {
        totalWidth = singleEditorWidth + 20;
    }
    
    int minWidth = 650;
    int maxWidth = qMin(m_editorWidth - 40, 1600);
    totalWidth = qBound(minWidth, totalWidth, maxWidth);
    
    setFixedWidth(totalWidth);
    
    int lineHeight = fm.height();
    int lineCount = m_rightDocument->document()->blockCount();
    
    int contentHeight = lineCount * lineHeight;
    int minHeight = 150;
    int maxHeight = 600;
    
    int totalHeight = 90 + qBound(minHeight, contentHeight, maxHeight);
    
    int editorHeight = qBound(minHeight, contentHeight, maxHeight);
    m_leftEditor->setMinimumHeight(editorHeight);
    m_leftEditor->setMaximumHeight(editorHeight);
    m_rightEditor->setMinimumHeight(editorHeight);
    m_rightEditor->setMaximumHeight(editorHeight);
    
    updateGeometry();
    adjustSize();
}

} // namespace QodeAssist
