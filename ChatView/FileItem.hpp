// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

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
