/* 
 * Copyright (C) 2024 Petr Mironychev
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

#include <texteditor/textdocument.h>
#include <QDateTime>
#include <QHash>
#include <QQueue>
#include <QTimer>

namespace QodeAssist::Context {

class ChangesManager : public QObject
{
    Q_OBJECT

public:
    struct ChangeInfo
    {
        QString fileName;
        int lineNumber;
        QString lineContent;
    };

    static ChangesManager &instance();

    void addChange(
        TextEditor::TextDocument *document, int position, int charsRemoved, int charsAdded);
    QString getRecentChangesContext(const TextEditor::TextDocument *currentDocument) const;

private:
    ChangesManager();
    ~ChangesManager();
    ChangesManager(const ChangesManager &) = delete;
    ChangesManager &operator=(const ChangesManager &) = delete;

    void cleanupOldChanges();

    QHash<TextEditor::TextDocument *, QQueue<ChangeInfo>> m_documentChanges;
};

} // namespace QodeAssist::Context
