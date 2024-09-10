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

#include "ChangesManager.h"
#include "QodeAssistUtils.hpp"

namespace QodeAssist {

ChangesManager &ChangesManager::instance()
{
    static ChangesManager instance;
    return instance;
}

ChangesManager::ChangesManager()
    : QObject(nullptr)
{
}

ChangesManager::~ChangesManager()
{
}

void ChangesManager::addChange(TextEditor::TextDocument *document,
                               int position,
                               int charsRemoved,
                               int charsAdded)
{
    auto &documentQueue = m_documentChanges[document];

    QTextBlock block = document->document()->findBlock(position);
    int lineNumber = block.blockNumber();
    QString lineContent = block.text();
    QString fileName = document->filePath().fileName();

    ChangeInfo change{fileName, lineNumber, lineContent};

    auto it = std::find_if(documentQueue.begin(),
                           documentQueue.end(),
                           [lineNumber](const ChangeInfo &c) { return c.lineNumber == lineNumber; });

    if (it != documentQueue.end()) {
        it->lineContent = lineContent;
    } else {
        documentQueue.enqueue(change);

        if (documentQueue.size() > MAX_CACHED_CHANGES) {
            documentQueue.dequeue();
        }
    }

    logMessage(QString("ChangesManager: Updated %1 line %2: '%3'")
                   .arg(fileName)
                   .arg(lineNumber)
                   .arg(lineContent));
    logMessage(QString("ChangesManager: Document queue size %1").arg(documentQueue.size()));
}

QString ChangesManager::getRecentChangesContext(const TextEditor::TextDocument *currentDocument) const
{
    QString context;
    for (auto it = m_documentChanges.constBegin(); it != m_documentChanges.constEnd(); ++it) {
        if (it.key() != currentDocument) {
            for (const auto &change : it.value()) {
                context += change.lineContent + "\n";
            }
        }
    }
    return context;
}

} // namespace QodeAssist
