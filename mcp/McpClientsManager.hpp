// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>

#include "McpServerConnection.hpp"

namespace QodeAssist::Mcp {

class McpClientsManager : public QObject
{
    Q_OBJECT
public:
    static McpClientsManager &instance();

    static QString configFilePath();
    static QByteArray emptyConfigTemplate();

    void init();

    QList<McpServerConnection *> connections() const;

    bool setServerEnabled(const QString &name, bool enabled);
    bool addServer(const QString &name, const QJsonObject &entry);
    bool removeServer(const QString &name);
    void reload();

    void registerToolsOn(::LLMQore::ToolsManager *tools) const;

signals:
    void serversChanged();
    void writeFailed(const QString &reason);

private:
    explicit McpClientsManager(QObject *parent = nullptr);
    ~McpClientsManager() override;
    McpClientsManager(const McpClientsManager &) = delete;
    McpClientsManager &operator=(const McpClientsManager &) = delete;

    void loadFromDisk();
    void ensureFileExists();

    static QJsonObject builtinServers();
    QJsonObject readRoot() const;
    bool writeRoot(const QJsonObject &root);

    bool m_initialized = false;
    QList<McpServerConnection *> m_connections;
};

} // namespace QodeAssist::Mcp
