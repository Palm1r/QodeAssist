// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SystemPromptBuilder.hpp"

namespace QodeAssist {

SystemPromptBuilder::SystemPromptBuilder(QObject *parent)
    : QObject(parent)
{}

void SystemPromptBuilder::setLayer(const QString &name, const QString &text)
{
    for (auto &pair : m_layers) {
        if (pair.first == name) {
            if (pair.second == text) return;
            pair.second = text;
            emit layersChanged();
            return;
        }
    }
    m_layers.append({name, text});
    emit layersChanged();
}

void SystemPromptBuilder::clearLayer(const QString &name)
{
    for (auto it = m_layers.begin(); it != m_layers.end(); ++it) {
        if (it->first == name) {
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
    for (const auto &pair : m_layers) {
        if (pair.first == name) return pair.second;
    }
    return {};
}

QStringList SystemPromptBuilder::layerNames() const
{
    QStringList out;
    out.reserve(m_layers.size());
    for (const auto &pair : m_layers) out.append(pair.first);
    return out;
}

QString SystemPromptBuilder::compose(const QString &separator) const
{
    QStringList parts;
    parts.reserve(m_layers.size());
    for (const auto &pair : m_layers) {
        if (!pair.second.isEmpty())
            parts.append(pair.second);
    }
    return parts.join(separator);
}

} // namespace QodeAssist
