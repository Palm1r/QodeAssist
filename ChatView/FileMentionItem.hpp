/*
 * Copyright (C) 2026 Petr Mironychev
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

#include <QHash>
#include <QQuickItem>
#include <QRegularExpression>
#include <QVariantList>

namespace QodeAssist::Chat {

class FileMentionItem : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(QVariantList searchResults READ searchResults NOTIFY searchResultsChanged FINAL)
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentIndexChanged FINAL)

    QML_ELEMENT
public:
    explicit FileMentionItem(QQuickItem *parent = nullptr);

    QVariantList searchResults() const;
    int currentIndex() const;
    void setCurrentIndex(int index);

    Q_INVOKABLE void updateSearch(const QString &query);
    Q_INVOKABLE void refreshSearch();
    Q_INVOKABLE void moveUp();
    Q_INVOKABLE void moveDown();
    Q_INVOKABLE void selectCurrent();
    Q_INVOKABLE void dismiss();

    Q_INVOKABLE QVariantMap handleFileSelection(
        const QString &absolutePath,
        const QString &relativePath,
        const QString &projectName,
        const QString &currentQuery,
        bool useTools);

    Q_INVOKABLE QVariantMap applyCurrentSelection(
        const QString &text, int cursorPosition, bool useTools);

    Q_INVOKABLE void registerMention(const QString &mentionKey, const QString &absolutePath);
    Q_INVOKABLE void clearMentions();
    Q_INVOKABLE QString expandMentions(const QString &text);

signals:
    void searchResultsChanged();
    void currentIndexChanged();
    void fileSelected(const QString &absolutePath,
                      const QString &relativePath,
                      const QString &projectName);
    void projectSelected(const QString &projectName);
    void dismissed();
    void fileAttachRequested(const QStringList &filePaths);

private:
    QVariantList searchProjectFiles(const QString &query);
    QVariantList getOpenFiles(const QString &query);
    QString readFileLines(const QString &filePath, int startLine, int endLine);
    static QString fileExtension(const QString &filePath);

    QVariantList m_searchResults;
    int m_currentIndex = 0;
    QString m_lastQuery;
    QHash<QString, QString> m_atMentionMap;
};

} // namespace QodeAssist::Chat
