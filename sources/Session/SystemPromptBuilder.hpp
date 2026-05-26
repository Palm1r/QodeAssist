// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

namespace QodeAssist {

class SystemPromptBuilder : public QObject
{
    Q_OBJECT
public:
    explicit SystemPromptBuilder(QObject *parent = nullptr);

    void setLayer(const QString &name, const QString &text);
    void clearLayer(const QString &name);
    void clear();

    QString layer(const QString &name) const;
    QStringList layerNames() const;
    bool isEmpty() const { return m_layers.isEmpty(); }

    QString compose(const QString &separator = QStringLiteral("\n\n")) const;

signals:
    void layersChanged();

private:
    QVector<QPair<QString, QString>> m_layers;
};

} // namespace QodeAssist
