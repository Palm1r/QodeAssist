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

#pragma once

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QKeyEvent>
#include <QEnterEvent>
#include <QSharedPointer>
#include <QSplitter>
#include <QSplitterHandle>
#include <functional>

#include <texteditor/texteditor.h>
#include <utils/textutils.h>
#include <utils/differ.h>

namespace QodeAssist {

class CustomSplitterHandle : public QSplitterHandle
{
    Q_OBJECT
public:
    explicit CustomSplitterHandle(Qt::Orientation orientation, QSplitter *parent);
protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
private:
    bool m_hovered = false;
};

class CustomSplitter : public QSplitter
{
    Q_OBJECT
public:
    explicit CustomSplitter(Qt::Orientation orientation, QWidget *parent = nullptr);
protected:
    QSplitterHandle *createHandle() override;
};

class RefactorWidget : public QWidget
{
    Q_OBJECT

public:
    explicit RefactorWidget(TextEditor::TextEditorWidget *sourceEditor, QWidget *parent = nullptr);
    ~RefactorWidget() override;

    void setDiffContent(const QString &originalText, const QString &refactoredText);
    void setDiffContent(const QString &originalText, const QString &refactoredText,
                        const QString &contextBefore, const QString &contextAfter);
    
    void setApplyText(const QString &text) { m_applyText = text; }
    void setRange(const Utils::Text::Range &range);
    void setEditorWidth(int width);
    
    QString getRefactoredText() const;
    
    void setApplyCallback(std::function<void(const QString &)> callback);
    void setDeclineCallback(std::function<void()> callback);

signals:
    void applied();
    void declined();

protected:
    void paintEvent(QPaintEvent *event) override;
    bool event(QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private slots:
    void syncLeftScroll(int value);
    void syncRightScroll(int value);
    void syncLeftHorizontalScroll(int value);
    void syncRightHorizontalScroll(int value);
    void onRightEditorTextChanged();

private:
    TextEditor::TextEditorWidget *m_sourceEditor;
    TextEditor::TextEditorWidget *m_leftEditor;
    TextEditor::TextEditorWidget *m_rightEditor;
    QSharedPointer<TextEditor::TextDocument> m_leftDocument;
    QSharedPointer<TextEditor::TextDocument> m_rightDocument;
    QWidget *m_leftContainer;
    QSplitter *m_splitter;
    QLabel *m_statsLabel;
    QPushButton *m_applyButton;
    QPushButton *m_declineButton;
    
    QString m_originalText;
    QString m_refactoredText;
    QString m_applyText;
    QString m_contextBefore;
    QString m_contextAfter;
    Utils::Text::Range m_range;
    int m_editorWidth;
    bool m_syncingScroll;
    bool m_isClosing;
    int m_linesAdded;
    int m_linesRemoved;
    
    QList<Utils::Diff> m_cachedDiffList;
    
    std::function<void(const QString &)> m_applyCallback;
    std::function<void()> m_declineCallback;

    void setupUi();
    void applyRefactoring();
    void declineRefactoring();
    void updateSizeToContent();
    void highlightDifferences();
    void dimContextLines(const QString &contextBefore, const QString &contextAfter);
    void calculateStats();
    void updateStatsLabel();
    void applySyntaxHighlighting();
    void addLineMarkers();
    void applyEditorSettings();
    void updateButtonStyles();
};

} // namespace QodeAssist
