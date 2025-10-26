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

#include <QMutex>
#include <QQuickItem>
#include <QSet>
#include <QString>
#include <QtQmlIntegration>

namespace QodeAssist::Chat {

class FileEditItem : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT

public:
    enum class EditStatus {
        Pending,
        Applied,
        Rejected,
        Reverted
    };
    Q_ENUM(EditStatus)

    static constexpr const char *EDIT_MARKER = "QODEASSIST_FILE_EDIT:";
    static constexpr int RETRY_DELAY_MS = 100;
    static constexpr int AUTO_APPLY_RETRY_DELAY_MS = 50;
    static constexpr int MAX_RETRY_COUNT = 10;

    Q_PROPERTY(QString editId READ editId NOTIFY editIdChanged FINAL)
    Q_PROPERTY(QString mode READ mode NOTIFY modeChanged FINAL)
    Q_PROPERTY(QString filePath READ filePath NOTIFY filePathChanged FINAL)
    Q_PROPERTY(QString searchText READ searchText NOTIFY searchTextChanged FINAL)
    Q_PROPERTY(QString newText READ newText NOTIFY newTextChanged FINAL)
    Q_PROPERTY(QString lineBefore READ lineBefore NOTIFY lineBeforeChanged FINAL)
    Q_PROPERTY(QString lineAfter READ lineAfter NOTIFY lineAfterChanged FINAL)
    Q_PROPERTY(int addedLines READ addedLines NOTIFY addedLinesChanged FINAL)
    Q_PROPERTY(int removedLines READ removedLines NOTIFY removedLinesChanged FINAL)
    Q_PROPERTY(EditStatus status READ status NOTIFY statusChanged FINAL)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged FINAL)

public:
    explicit FileEditItem(QQuickItem *parent = nullptr);

    QString editId() const { return m_editId; }
    QString mode() const { return m_mode; }
    QString filePath() const { return m_filePath; }
    QString searchText() const { return m_searchText; }
    QString newText() const { return m_newText; }
    QString lineBefore() const { return m_lineBefore; }
    QString lineAfter() const { return m_lineAfter; }
    int addedLines() const { return m_addedLines; }
    int removedLines() const { return m_removedLines; }
    EditStatus status() const { return m_status; }
    QString statusMessage() const { return m_statusMessage; }

    Q_INVOKABLE void parseFromContent(const QString &content);
    Q_INVOKABLE void applyEdit();
    Q_INVOKABLE void revertEdit();

signals:
    void editIdChanged();
    void modeChanged();
    void filePathChanged();
    void searchTextChanged();
    void newTextChanged();
    void lineBeforeChanged();
    void lineAfterChanged();
    void addedLinesChanged();
    void removedLinesChanged();
    void statusChanged();
    void statusMessageChanged();

private:
    void setStatus(EditStatus status);
    void setStatusMessage(const QString &message);
    void applyEditInternal(bool isAutomatic, int retryCount = 0);
    void performApply();
    void performRevert();
    void rejectWithError(const QString &errorMessage);
    void finishWithSuccess(EditStatus status, const QString &message);
    
    bool writeFile(const QString &filePath, const QString &content);
    QString readFile(const QString &filePath);
    
    static bool acquireFileLock(const QString &filePath);
    static void releaseFileLock(const QString &filePath);
    static QMutex s_fileLockMutex;
    static QSet<QString> s_lockedFiles;

    QString m_editId;
    QString m_mode;
    QString m_filePath;
    QString m_searchText;
    QString m_newText;
    QString m_lineBefore;
    QString m_lineAfter;
    QString m_originalContent; // Stored when applying edit
    int m_addedLines = 0;
    int m_removedLines = 0;
    EditStatus m_status = EditStatus::Pending;
    QString m_statusMessage;
};

} // namespace QodeAssist::Chat

