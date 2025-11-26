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
#include "DiffStatistics.hpp"

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

#include <coreplugin/icore.h>
#include <utils/differ.h>
#include <utils/theme/theme.h>

#include "settings/QuickRefactorSettings.hpp"

namespace QodeAssist {

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

void CustomSplitterHandle::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    QColor bgColor = Utils::creatorColor(Utils::Theme::BackgroundColorHover);
    bgColor.setAlpha(m_hovered ? 150 : 50);
    painter.fillRect(rect(), bgColor);
    
    QColor lineColor = Utils::creatorColor(Utils::Theme::SplitterColor);
    lineColor.setAlpha(m_hovered ? 255 : 180);
    
    const int lineWidth = m_hovered ? 3 : 2;
    const int margin = 10;
    painter.setPen(QPen(lineColor, lineWidth));
    
    if (orientation() == Qt::Horizontal) {
        int x = width() / 2;
        painter.drawLine(x, margin, x, height() - margin);
        
        painter.setBrush(lineColor);
        int centerY = height() / 2;
        const int dotSize = m_hovered ? 3 : 2;
        const int dotSpacing = 8;
        for (int i = -2; i <= 2; ++i) {
            painter.drawEllipse(QPoint(x, centerY + i * dotSpacing), dotSize, dotSize);
        }
    } else {
        int y = height() / 2;
        painter.drawLine(margin, y, width() - margin, y);
    }
}

void CustomSplitterHandle::enterEvent(QEnterEvent *event)
{
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
    , m_applyButton(nullptr)
    , m_declineButton(nullptr)
    , m_editorWidth(800)
    , m_syncingScroll(false)
    , m_isClosing(false)
    , m_linesAdded(0)
    , m_linesRemoved(0)
{
    setupUi();
    applyEditorSettings();
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
    mainLayout->setContentsMargins(6, 6, 6, 6);
    mainLayout->setSpacing(4);

    m_statsLabel = new QLabel(this);
    m_statsLabel->setAlignment(Qt::AlignLeft);
    mainLayout->addWidget(m_statsLabel);

    m_leftDocument = QSharedPointer<TextEditor::TextDocument>::create();
    m_rightDocument = QSharedPointer<TextEditor::TextDocument>::create();
    
    Qt::Orientation initialOrientation = Settings::quickRefactorSettings().widgetOrientation.value() == 1 
        ? Qt::Vertical : Qt::Horizontal;
    
    m_splitter = new CustomSplitter(initialOrientation, this);
    m_splitter->setChildrenCollapsible(false);
    m_splitter->setHandleWidth(12);
    m_splitter->setStyleSheet("QSplitter::handle { background-color: transparent; }");
    
    m_leftEditor = new TextEditor::TextEditorWidget();
    m_leftEditor->setTextDocument(m_leftDocument);
    m_leftEditor->setReadOnly(true);
    m_leftEditor->setFrameStyle(QFrame::StyledPanel);
    m_leftEditor->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_leftEditor->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_leftEditor->setMinimumHeight(100);
    m_leftEditor->setMinimumWidth(150);
    m_leftEditor->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_leftEditor->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    
    m_rightEditor = new TextEditor::TextEditorWidget();
    m_rightEditor->setTextDocument(m_rightDocument);
    m_rightEditor->setReadOnly(false);
    m_rightEditor->setFrameStyle(QFrame::StyledPanel);
    m_rightEditor->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_rightEditor->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_rightEditor->setMinimumHeight(100);
    m_rightEditor->setMinimumWidth(150);
    m_rightEditor->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    m_leftContainer = new QWidget();
    m_leftContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto *leftLayout = new QVBoxLayout(m_leftContainer);
    leftLayout->setSpacing(2);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    
    auto *originalLabel = new QLabel(tr("◄ Original"), m_leftContainer);
    leftLayout->addWidget(originalLabel);
    leftLayout->addWidget(m_leftEditor);
    
    auto *rightContainer = new QWidget();
    rightContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto *rightLayout = new QVBoxLayout(rightContainer);
    rightLayout->setSpacing(2);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    
    auto *refactoredLabel = new QLabel(tr("Refactored ►"), rightContainer);
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

#ifdef Q_OS_MACOS
    m_applyButton = new QPushButton(tr("✓ Apply (⌘+Enter)"), this);
#else
    m_applyButton = new QPushButton(tr("✓ Apply (Ctrl+Enter)"), this);
#endif
    m_applyButton->setFocusPolicy(Qt::NoFocus);
    m_applyButton->setCursor(Qt::PointingHandCursor);
    m_applyButton->setMaximumHeight(24);
    
    m_declineButton = new QPushButton(tr("✗ Decline (Esc)"), this);
    m_declineButton->setFocusPolicy(Qt::NoFocus);
    m_declineButton->setCursor(Qt::PointingHandCursor);
    m_declineButton->setMaximumHeight(24);
    
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_applyButton);
    buttonLayout->addWidget(m_declineButton);
    
    mainLayout->addLayout(buttonLayout);
    
    connect(m_applyButton, &QPushButton::clicked, this, &RefactorWidget::applyRefactoring);
    connect(m_declineButton, &QPushButton::clicked, this, &RefactorWidget::declineRefactoring);
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
    
    m_leftContainer->setVisible(true);
    
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
    dimFormat.setForeground(Utils::creatorColor(Utils::Theme::TextColorDisabled));
    
    auto dimLines = [&](QTextDocument *doc, int lineCount, bool fromStart) {
        QTextCursor cursor(doc);
        if (!fromStart) {
            cursor.movePosition(QTextCursor::End);
        }
        
        for (int i = 0; i < lineCount; ++i) {
            cursor.movePosition(QTextCursor::StartOfBlock);
            cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
            cursor.setCharFormat(dimFormat);
            
            if (fromStart) {
                if (!cursor.block().isValid() || !cursor.movePosition(QTextCursor::NextBlock)) {
                    break;
                }
            } else {
                if (!cursor.movePosition(QTextCursor::PreviousBlock)) {
                    break;
                }
            }
        }
    };
    
    if (!contextBefore.isEmpty()) {
        int lines = contextBefore.count('\n') + (contextBefore.endsWith('\n') ? 0 : 1);
        dimLines(m_leftDocument->document(), lines, true);
        dimLines(m_rightDocument->document(), lines, true);
    }
    
    if (!contextAfter.isEmpty()) {
        int lines = contextAfter.count('\n') + (contextAfter.endsWith('\n') ? 0 : 1);
        dimLines(m_leftDocument->document(), lines, false);
        dimLines(m_rightDocument->document(), lines, false);
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
    if (m_isClosing) return;
    m_isClosing = true;
    
    if (m_applyCallback) {
        m_applyCallback(m_applyText);
    }
    emit applied();
    close();
}

void RefactorWidget::declineRefactoring()
{
    if (m_isClosing) return;
    m_isClosing = true;
    
    if (m_declineCallback) {
        m_declineCallback();
    }
    emit declined();
    close();
}

void RefactorWidget::paintEvent(QPaintEvent *)
{
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
    int startPos = m_contextBefore.isEmpty() ? 0 : m_contextBefore.length() + 1;
    int endPos = m_contextAfter.isEmpty() ? fullText.length() : fullText.length() - m_contextAfter.length() - 1;
    m_applyText = fullText.mid(startPos, endPos - startPos);
}

void RefactorWidget::closeEvent(QCloseEvent *event)
{
    if (!m_isClosing) {
        declineRefactoring();
    }
    event->accept();
}

void RefactorWidget::calculateStats()
{
    DiffStatistics stats;
    stats.calculate(m_cachedDiffList);
    m_linesAdded = stats.linesAdded();
    m_linesRemoved = stats.linesRemoved();
}

void RefactorWidget::updateStatsLabel()
{
    DiffStatistics stats;
    stats.calculate(m_cachedDiffList);
    m_statsLabel->setText("📊 " + stats.formatSummary());
}

void RefactorWidget::applySyntaxHighlighting()
{
    if (!m_sourceEditor) {
        return;
    }
    
    auto *sourceDoc = m_sourceEditor->textDocument();
    if (!sourceDoc || !sourceDoc->syntaxHighlighter()) {
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
    int lineHeight = fm.height();
    int lineCount = m_rightDocument->document()->blockCount();
    
    bool horizontal = m_splitter->orientation() == Qt::Horizontal;
    
    const int minWidth = Settings::quickRefactorSettings().widgetMinWidth();
    const int maxWidth = qMin(Settings::quickRefactorSettings().widgetMaxWidth(), m_editorWidth - 40);
    const int minHeight = Settings::quickRefactorSettings().widgetMinHeight();
    const int maxHeight = Settings::quickRefactorSettings().widgetMaxHeight();
    
    if (horizontal) {
        int totalWidth = qBound(minWidth, charWidth * 60 * 2 + 90, maxWidth);
        setFixedWidth(totalWidth);
        
        int editorHeight = qBound(minHeight, lineCount * lineHeight, maxHeight);
        m_leftEditor->setMinimumHeight(editorHeight);
        m_leftEditor->setMaximumHeight(editorHeight);
        m_rightEditor->setMinimumHeight(editorHeight);
        m_rightEditor->setMaximumHeight(editorHeight);
    } else {
        int editorWidth = qBound(minWidth, charWidth * 85 + 80, maxWidth);
        setFixedWidth(editorWidth);
        
        int editorHeight = qBound(minHeight, lineCount * lineHeight, maxHeight);
        m_leftEditor->setMinimumHeight(editorHeight);
        m_leftEditor->setMaximumHeight(editorHeight);
        m_rightEditor->setMinimumHeight(editorHeight);
        m_rightEditor->setMaximumHeight(editorHeight);
    }
    
    updateGeometry();
    adjustSize();
}

void RefactorWidget::applyEditorSettings()
{
    if (!m_sourceEditor || !m_leftEditor || !m_rightEditor) {
        return;
    }
    
    QFont editorFont = m_sourceEditor->font();
    m_leftEditor->setFont(editorFont);
    m_rightEditor->setFont(editorFont);
    
    QString labelStyle = QString("color: %1; padding: 2px 4px;")
        .arg(Utils::creatorColor(Utils::Theme::TextColorDisabled).name());
    
    for (auto *label : findChildren<QLabel *>()) {
        if (label != m_statsLabel) {
            QFont labelFont = label->font();
            labelFont.setPointSize(qMax(8, editorFont.pointSize() - 2));
            label->setFont(labelFont);
            label->setStyleSheet(labelStyle);
        }
    }
    
    QFont statsFont = m_statsLabel->font();
    statsFont.setBold(true);
    statsFont.setPointSize(qMax(9, editorFont.pointSize() - 1));
    m_statsLabel->setFont(statsFont);
    
    m_statsLabel->setStyleSheet(QString(
        "color: %1; padding: 4px 6px; background-color: %2; border-radius: 3px;")
        .arg(Utils::creatorColor(Utils::Theme::TextColorNormal).name())
        .arg(Utils::creatorColor(Utils::Theme::BackgroundColorHover).name()));
    
    updateButtonStyles();
}

void RefactorWidget::updateButtonStyles()
{
    if (!m_applyButton || !m_declineButton) {
        return;
    }
    
    int baseFontSize = m_sourceEditor ? qMax(9, m_sourceEditor->font().pointSize() - 2) : 10;
    
    auto createStyle = [&](const QColor &color, bool bold) {
        return QString(
            "QPushButton {"
            "    background-color: %1; color: %2; border: 1px solid %3;"
            "    border-radius: 3px; padding: 2px 8px; font-size: %4pt;%5"
            "}"
            "QPushButton:hover { background-color: %6; border: 1px solid %2; }"
            "QPushButton:pressed { background-color: %7; }")
            .arg(Utils::creatorColor(Utils::Theme::BackgroundColorNormal).name())
            .arg(color.name())
            .arg(Utils::creatorColor(Utils::Theme::SplitterColor).name())
            .arg(baseFontSize)
            .arg(bold ? QLatin1StringView(" font-weight: bold;") : QLatin1StringView(""))
            .arg(Utils::creatorColor(Utils::Theme::BackgroundColorHover).name())
            .arg(Utils::creatorColor(Utils::Theme::BackgroundColorSelected).name());
    };
    
    m_applyButton->setStyleSheet(createStyle(Utils::creatorColor(Utils::Theme::TextColorNormal), true));
    m_declineButton->setStyleSheet(createStyle(Utils::creatorColor(Utils::Theme::TextColorError), false));
}

} // namespace QodeAssist
