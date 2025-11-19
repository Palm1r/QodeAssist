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

#include <QQuickItem>

namespace QodeAssist::Chat {

class FileItem: public QQuickItem
{
    Q_OBJECT
    QML_NAMED_ELEMENT(FileItem)

    Q_PROPERTY(QString filePath READ filePath WRITE setFilePath NOTIFY filePathChanged)

public:
    FileItem(QQuickItem *parent = nullptr);

    Q_INVOKABLE void openFileInEditor();
    Q_INVOKABLE void openFileInExternalEditor();

    QString filePath() const;
    void setFilePath(const QString &newFilePath);

signals:
    void filePathChanged();

private:
    QString m_filePath;
};
}
