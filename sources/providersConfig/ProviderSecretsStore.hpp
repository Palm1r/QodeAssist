// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QString>

namespace QodeAssist::Providers {

class ProviderSecretsStore : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ProviderSecretsStore)
public:
    explicit ProviderSecretsStore(QObject *parent = nullptr);
    ~ProviderSecretsStore() override;

    [[nodiscard]] QString readKeySync(const QString &ref) const;

    void writeKey(const QString &ref, const QString &value);
    void eraseKey(const QString &ref);

    [[nodiscard]] bool hasKey(const QString &ref) const;

signals:
    void keyChanged(const QString &ref);
};

} // namespace QodeAssist::Providers
