// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "SystemPromptBuilder.hpp"

#include <algorithm>

namespace QodeAssist {

SystemPromptBuilder::SystemPromptBuilder(QObject *parent)
    : QObject(parent)
{}

void SystemPromptBuilder::setLayer(const QString &name, const QString &text, int priority)
{
    for (auto &layer : m_layers) {
        if (layer.name == name) {
            if (layer.text == text && layer.priority == priority)
                return;
            layer.text = text;
            layer.priority = priority;
            emit layersChanged();
            return;
        }
    }
    m_layers.append({name, text, priority});
    emit layersChanged();
}

void SystemPromptBuilder::clearLayer(const QString &name)
{
    for (auto it = m_layers.begin(); it != m_layers.end(); ++it) {
        if (it->name == name) {
            m_layers.erase(it);
            emit layersChanged();
            return;
        }
    }
}

void SystemPromptBuilder::clear()
{
    if (m_layers.isEmpty()) return;
    m_layers.clear();
    emit layersChanged();
}

QString SystemPromptBuilder::layer(const QString &name) const
{
    for (const auto &l : m_layers) {
        if (l.name == name)
            return l.text;
    }
    return {};
}

QStringList SystemPromptBuilder::layerNames() const
{
    QStringList out;
    out.reserve(m_layers.size());
    for (const auto &l : m_layers)
        out.append(l.name);
    return out;
}

QString SystemPromptBuilder::compose(const QString &separator) const
{
    QVector<Layer> ordered = m_layers;
    std::stable_sort(ordered.begin(), ordered.end(), [](const Layer &a, const Layer &b) {
        return a.priority < b.priority;
    });

    QStringList parts;
    parts.reserve(ordered.size());
    for (const auto &l : ordered) {
        if (!l.text.isEmpty())
            parts.append(l.text);
    }
    return parts.join(separator);
}

} // namespace QodeAssist
