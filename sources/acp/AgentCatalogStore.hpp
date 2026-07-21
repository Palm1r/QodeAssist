// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>

#include "acp/AgentCatalog.hpp"

QT_BEGIN_NAMESPACE
class QNetworkAccessManager;
class QNetworkReply;
QT_END_NAMESPACE

namespace QodeAssist::Acp {

class AgentCatalogStore : public QObject
{
    Q_OBJECT

public:
    explicit AgentCatalogStore(QObject *parent = nullptr);

    static QString userAgentsDirectory();
    static QString registryCachePath();
    static QUrl registryUrl();
    static QString bundledSnapshotPath();

    const AgentCatalog &catalog() const { return m_catalog; }
    QStringList warnings() const { return m_warnings; }

    bool hasCachedRegistry() const;
    bool isRefreshing() const { return m_reply != nullptr; }

    void reload();
    void refreshFromRegistry();

signals:
    void catalogChanged();
    void refreshFinished(bool ok, const QString &error);

private:
    void loadBundledSnapshot();
    void loadRegistryCache();
    void loadUserFiles();

    QNetworkAccessManager *m_network = nullptr;
    QNetworkReply *m_reply = nullptr;
    AgentCatalog m_catalog;
    QStringList m_warnings;
};

} // namespace QodeAssist::Acp
