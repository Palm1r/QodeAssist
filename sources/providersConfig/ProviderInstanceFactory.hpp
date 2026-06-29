// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <vector>

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>

#include "ProviderInstance.hpp"

namespace QodeAssist::Providers {

class ProviderInstanceFactory : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ProviderInstanceFactory)
public:
    explicit ProviderInstanceFactory(QObject *parent = nullptr);
    ~ProviderInstanceFactory() override;

    void reload();

    [[nodiscard]] static QString userInstancesDir();

    [[nodiscard]] const ProviderInstance *instanceByName(const QString &name) const;
    [[nodiscard]] QStringList knownClientApis() const;
    [[nodiscard]] const std::vector<ProviderInstance> &instances() const noexcept
    {
        return m_instances;
    }

    [[nodiscard]] QStringList lastLoadErrors() const { return m_errors; }
    [[nodiscard]] QStringList lastLoadWarnings() const { return m_warnings; }

    void clear();

signals:
    void instanceChanged(const QString &name);
    void instancesReloaded();

private:
    void rebuildIndexes();

    std::vector<ProviderInstance> m_instances;
    QHash<QString, qsizetype> m_nameIndex;
    QStringList m_knownClientApisCache;
    QStringList m_errors;
    QStringList m_warnings;
};

} // namespace QodeAssist::Providers
