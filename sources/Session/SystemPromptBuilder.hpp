// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

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
    static constexpr int kAgentPriority = 0;
    static constexpr int kDefaultPriority = 100;

    explicit SystemPromptBuilder(QObject *parent = nullptr);

    void setLayer(const QString &name, const QString &text, int priority = kDefaultPriority);
    void clearLayer(const QString &name);
    void clear();

    QString layer(const QString &name) const;
    QStringList layerNames() const;
    bool isEmpty() const { return m_layers.isEmpty(); }

    QString compose(const QString &separator = QStringLiteral("\n\n")) const;

signals:
    void layersChanged();

private:
    struct Layer
    {
        QString name;
        QString text;
        int priority = kDefaultPriority;
    };

    QVector<Layer> m_layers;
};

} // namespace QodeAssist
