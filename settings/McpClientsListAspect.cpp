// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "McpClientsListAspect.hpp"

#include <coreplugin/editormanager/editormanager.h>
#include <utils/filepath.h>
#include <utils/theme/theme.h>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFrame>
#include <QHBoxLayout>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QPalette>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

#include "McpSettings.hpp"
#include "SettingsTr.hpp"
#include "StatusDot.hpp"
#include "mcp/McpClientsManager.hpp"
#include "mcp/McpServerConnection.hpp"

namespace QodeAssist::Settings {

namespace {

QString mutedColorHex()
{
    if (auto *t = Utils::creatorTheme())
        return t->color(Utils::Theme::TextColorDisabled).name();
    return qApp->palette().color(QPalette::PlaceholderText).name();
}

QString errorColorHex()
{
    if (auto *t = Utils::creatorTheme())
        return t->color(Utils::Theme::TextColorError).name();
    return QStringLiteral("#d8444d");
}

QColor dotColorFor(Mcp::McpConnectionState state)
{
    auto *t = Utils::creatorTheme();
    switch (state) {
    case Mcp::McpConnectionState::Connected:
        return t ? t->color(Utils::Theme::IconsRunColor) : QColor(0x3fb950);
    case Mcp::McpConnectionState::Failed:
        return t ? t->color(Utils::Theme::TextColorError) : QColor(0xd8444d);
    case Mcp::McpConnectionState::Connecting:
        return t ? t->color(Utils::Theme::IconsWarningColor) : QColor(0xd29922);
    case Mcp::McpConnectionState::Disabled:
        break;
    }
    return t ? t->color(Utils::Theme::TextColorDisabled) : QColor(0x888888);
}

QString statusTooltip(Mcp::McpConnectionState state, const QString &detail)
{
    switch (state) {
    case Mcp::McpConnectionState::Connected:
        return detail.isEmpty() ? McpClientsListAspect::tr("Connected.") : detail;
    case Mcp::McpConnectionState::Connecting:
        return McpClientsListAspect::tr("Connecting…");
    case Mcp::McpConnectionState::Failed:
        return detail.isEmpty() ? McpClientsListAspect::tr("Failed.")
                                : McpClientsListAspect::tr("Failed: %1").arg(detail);
    case Mcp::McpConnectionState::Disabled:
        break;
    }
    return McpClientsListAspect::tr("Disabled.");
}

QString formatDetails(const Mcp::McpServerConfig &cfg)
{
    const QString muted = mutedColorHex();
    const QString type = cfg.transport == Mcp::McpTransportKind::Http
                             ? QStringLiteral("sse")
                             : QStringLiteral("stdio");

    QString details;
    if (cfg.transport == Mcp::McpTransportKind::Http) {
        details = QString("<span style=\"color:%1\">%2</span>")
                      .arg(muted, cfg.url.toString().toHtmlEscaped());
    } else {
        QStringList parts;
        if (!cfg.command.isEmpty())
            parts << cfg.command.toHtmlEscaped();
        for (const QString &a : cfg.args)
            parts << a.toHtmlEscaped();
        details = QString("<span style=\"color:%1\">%2</span>").arg(muted, parts.join(' '));
    }

    if (!cfg.env.isEmpty()) {
        QStringList envKeys;
        for (auto it = cfg.env.begin(); it != cfg.env.end(); ++it)
            envKeys << it.key().toHtmlEscaped();
        details
            += QString(" &nbsp; <span style=\"color:%1\">env: %2</span>")
                   .arg(muted, envKeys.join(", "));
    }

    return QString("<b>%1</b> &nbsp; <span style=\"color:%2\">[%3]</span><br><tt>%4</tt>")
        .arg(cfg.name.toHtmlEscaped(), muted, type, details);
}

struct ExamplePreset
{
    QString label;
    QString defaultName;
    QJsonObject body;
};

QList<ExamplePreset> buildExamplePresets()
{
    QList<ExamplePreset> out;

    out.append(
        {McpClientsListAspect::tr("everything (reference test server)"),
         QStringLiteral("everything"),
         QJsonObject{
             {"enable", true},
             {"type", "stdio"},
             {"command", "npx"},
             {"args", QJsonArray{"-y", "@modelcontextprotocol/server-everything"}}}});

    out.append(
        {McpClientsListAspect::tr("filesystem (local files)"),
         QStringLiteral("filesystem"),
         QJsonObject{
             {"enable", true},
             {"type", "stdio"},
             {"command", "npx"},
             {"args",
              QJsonArray{
                  "-y", "@modelcontextprotocol/server-filesystem", QDir::homePath()}}}});

    out.append(
        {McpClientsListAspect::tr("memory (in-memory key-value)"),
         QStringLiteral("memory"),
         QJsonObject{
             {"enable", true},
             {"type", "stdio"},
             {"command", "npx"},
             {"args", QJsonArray{"-y", "@modelcontextprotocol/server-memory"}}}});

    out.append(
        {McpClientsListAspect::tr("git (local git ops)"),
         QStringLiteral("git"),
         QJsonObject{
             {"enable", true},
             {"type", "stdio"},
             {"command", "uvx"},
             {"args", QJsonArray{"mcp-server-git"}}}});

    out.append(
        {McpClientsListAspect::tr("time (system clock)"),
         QStringLiteral("time"),
         QJsonObject{
             {"enable", true},
             {"type", "stdio"},
             {"command", "uvx"},
             {"args", QJsonArray{"mcp-server-time"}}}});

    out.append(
        {McpClientsListAspect::tr("qtcreator (Qt Creator's built-in MCP server)"),
         QStringLiteral("qtcreator"),
         QJsonObject{
             {"enable", false},
             {"type", "sse"},
             {"url", "http://127.0.0.1:3001/sse"},
             {"spec", "2024-11-05"}}});

    out.append(
        {McpClientsListAspect::tr("remote (SSE / HTTP)"),
         QStringLiteral("remote"),
         QJsonObject{
             {"enable", false},
             {"type", "sse"},
             {"url", "https://example.com/mcp"},
             {"headers", QJsonObject{{"Authorization", "Bearer <token>"}}}}});

    return out;
}

struct RowWidgets
{
    QPointer<StatusDot> dot;
    QPointer<QLabel> status;
    QPointer<QLabel> tools;
};

void applyState(const RowWidgets &w, Mcp::McpServerConnection *conn)
{
    if (!conn)
        return;
    if (w.dot) {
        w.dot->setColor(dotColorFor(conn->state()));
        w.dot->setToolTip(statusTooltip(conn->state(), conn->statusText()));
    }
    if (w.status) {
        w.status->setText(
            QString("<span style=\"color:%1\">%2</span>")
                .arg(mutedColorHex(),
                     statusTooltip(conn->state(), conn->statusText()).toHtmlEscaped()));
    }
    if (w.tools) {
        const QStringList names = conn->toolNames();
        if (names.isEmpty()) {
            if (conn->state() == Mcp::McpConnectionState::Connected) {
                w.tools->setText(
                    QString("<i style=\"color:%1\">%2</i>")
                        .arg(mutedColorHex(),
                             McpClientsListAspect::tr("Server reports no tools.")));
                w.tools->show();
            } else {
                w.tools->clear();
                w.tools->hide();
            }
        } else {
            QStringList escaped;
            escaped.reserve(names.size());
            for (const QString &n : names)
                escaped << n.toHtmlEscaped();
            w.tools->setText(
                QString("<b>%1</b> (%2): %3")
                    .arg(McpClientsListAspect::tr("Tools"))
                    .arg(names.size())
                    .arg(escaped.join(", ")));
            w.tools->show();
        }
    }
}

QWidget *makeRow(Mcp::McpServerConnection *conn, QHash<QString, RowWidgets> *widgets, QWidget *host)
{
    auto *entry = new QFrame(host);
    entry->setFrameShape(QFrame::StyledPanel);
    auto *outer = new QVBoxLayout(entry);
    outer->setContentsMargins(8, 6, 8, 6);
    outer->setSpacing(2);

    auto *row = new QWidget(entry);
    auto *rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(6);

    auto *dot = new StatusDot(row);
    rowLayout->addWidget(dot, 0, Qt::AlignTop);

    auto *check = new QCheckBox(row);
    check->setChecked(conn->config().enabled);
    check->setToolTip(McpClientsListAspect::tr("Enable / disable this MCP server"));
    rowLayout->addWidget(check, 0, Qt::AlignTop);

    auto *info = new QLabel(formatDetails(conn->config()), row);
    info->setTextInteractionFlags(Qt::TextSelectableByMouse);
    info->setWordWrap(true);
    rowLayout->addWidget(info, 1);

    auto *statusLabel = new QLabel(row);
    statusLabel->setMinimumWidth(120);
    statusLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    rowLayout->addWidget(statusLabel, 0, Qt::AlignTop);

    auto *removeBtn = new QPushButton(QStringLiteral("✕"), row);
    removeBtn->setToolTip(McpClientsListAspect::tr("Remove this server from the config."));
    removeBtn->setFlat(true);
    removeBtn->setFixedWidth(24);
    removeBtn->setCursor(Qt::PointingHandCursor);
    removeBtn->setStyleSheet(
        QString("QPushButton:hover { color: %1; }").arg(errorColorHex()));
    rowLayout->addWidget(removeBtn, 0, Qt::AlignTop);

    outer->addWidget(row);

    auto *tools = new QLabel(entry);
    tools->setTextInteractionFlags(Qt::TextSelectableByMouse);
    tools->setWordWrap(true);
    tools->setContentsMargins(38, 0, 0, 0);
    tools->hide();
    outer->addWidget(tools);

    RowWidgets w{dot, statusLabel, tools};
    widgets->insert(conn->config().name, w);
    applyState(w, conn);

    QObject::connect(check, &QCheckBox::toggled, row, [name = conn->config().name](bool on) {
        Mcp::McpClientsManager::instance().setServerEnabled(name, on);
    });

    QObject::connect(removeBtn, &QPushButton::clicked, row, [name = conn->config().name, host]() {
        const auto reply = QMessageBox::question(
            host,
            McpClientsListAspect::tr("Remove server"),
            McpClientsListAspect::tr("Remove server '%1' from the config?").arg(name),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (reply == QMessageBox::Yes)
            Mcp::McpClientsManager::instance().removeServer(name);
    });

    return entry;
}

void clearLayout(QVBoxLayout *lay)
{
    while (auto *item = lay->takeAt(0)) {
        if (auto *w = item->widget())
            w->deleteLater();
        delete item;
    }
}

} // namespace

McpClientsListAspect::McpClientsListAspect(Utils::AspectContainer *container)
    : Utils::BaseAspect(container)
{}

void McpClientsListAspect::addToLayoutImpl(Layouting::Layout &parent)
{
    auto *outer = new QWidget();
    auto *outerLayout = new QVBoxLayout(outer);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(4);

    auto *openBtn = new QPushButton(tr("Open Config"), outer);
    openBtn->setToolTip(Mcp::McpClientsManager::configFilePath());
    auto *refreshBtn = new QPushButton(tr("Refresh MCP List"), outer);

    auto *buttonsRow = new QHBoxLayout();
    buttonsRow->setContentsMargins(0, 0, 0, 0);
    buttonsRow->addWidget(openBtn);
    buttonsRow->addWidget(refreshBtn);
    buttonsRow->addStretch(1);

    auto *restartHint = new QLabel(outer);
    restartHint->setWordWrap(true);
    restartHint->setText(
        tr("Note: restart Qt Creator to apply MCP changes to already-opened chats "
           "and running sessions."));

    auto *summaryLabel = new QLabel(outer);
    summaryLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    auto *serversHost = new QWidget(outer);
    auto *serversLayout = new QVBoxLayout(serversHost);
    serversLayout->setContentsMargins(0, 0, 0, 0);
    serversLayout->setSpacing(4);

    auto *scroll = new QScrollArea(outer);
    scroll->setWidgetResizable(true);
    scroll->setMinimumHeight(160);
    scroll->setFrameShape(QFrame::StyledPanel);
    scroll->setWidget(serversHost);

    auto *quickSetupLabel = new QLabel(tr("Quick Setup"), outer);
    auto *presetsCombo = new QComboBox(outer);
    presetsCombo->setToolTip(
        tr("Pick a preset to append a ready-made server entry to the config "
           "(auto-suffixed if the name is taken)."));
    presetsCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    presetsCombo->addItem(tr("-- Select Preset --"));
    const auto presets = buildExamplePresets();
    for (const auto &p : presets)
        presetsCombo->addItem(p.label);

    auto *btnRow = new QHBoxLayout();
    btnRow->setContentsMargins(0, 0, 0, 0);
    btnRow->addWidget(quickSetupLabel);
    btnRow->addWidget(presetsCombo);
    btnRow->addStretch(1);

    outerLayout->addLayout(buttonsRow);
    outerLayout->addWidget(restartHint);
    outerLayout->addWidget(summaryLabel);
    outerLayout->addWidget(scroll);
    outerLayout->addLayout(btnRow);

    auto rowsState = std::make_shared<QHash<QString, RowWidgets>>();

    auto rebuild = [outer, serversLayout, summaryLabel, rowsState]() {
        clearLayout(serversLayout);
        rowsState->clear();

        const auto connections = Mcp::McpClientsManager::instance().connections();
        if (connections.isEmpty()) {
            auto *empty = new QLabel(
                QString("<p style=\"color:%1\">%2</p>")
                    .arg(mutedColorHex(),
                         tr("No servers configured. Add a preset below or edit the JSON.")),
                outer);
            serversLayout->addWidget(empty);
            serversLayout->addStretch();
            summaryLabel->setText(
                QString("<i>%1</i>").arg(tr("0 server(s) defined.")));
            return;
        }

        int enabled = 0;
        for (auto *conn : connections) {
            serversLayout->addWidget(makeRow(conn, rowsState.get(), outer));
            if (conn->config().enabled)
                ++enabled;
        }
        serversLayout->addStretch();

        summaryLabel->setText(
            QString("<i>%1</i>")
                .arg(tr("%1 server(s) defined, %2 enabled.")
                         .arg(connections.size())
                         .arg(enabled)));
    };

    rebuild();

    QObject::connect(
        &Mcp::McpClientsManager::instance(),
        &Mcp::McpClientsManager::serversChanged,
        outer,
        [rebuild, rowsState]() {
            const auto connections = Mcp::McpClientsManager::instance().connections();

            QStringList currentNames;
            currentNames.reserve(connections.size());
            for (auto *c : connections)
                currentNames << c->config().name;

            QStringList knownNames = rowsState->keys();
            std::sort(currentNames.begin(), currentNames.end());
            std::sort(knownNames.begin(), knownNames.end());

            if (currentNames != knownNames) {
                rebuild();
                return;
            }

            for (auto *conn : connections)
                applyState(rowsState->value(conn->config().name), conn);
        });

    QObject::connect(refreshBtn, &QPushButton::clicked, outer, []() {
        Mcp::McpClientsManager::instance().reload();
    });
    QObject::connect(openBtn, &QPushButton::clicked, outer, []() {
        Core::EditorManager::openEditor(
            Utils::FilePath::fromString(Mcp::McpClientsManager::configFilePath()));
    });
    QObject::connect(
        &Mcp::McpClientsManager::instance(),
        &Mcp::McpClientsManager::writeFailed,
        outer,
        [outer](const QString &reason) {
            QMessageBox::warning(
                outer,
                McpClientsListAspect::tr("MCP configuration"),
                McpClientsListAspect::tr("Failed to write %1:\n%2")
                    .arg(Mcp::McpClientsManager::configFilePath(), reason));
        });
    auto syncEnabled = [outer]() {
        outer->setEnabled(mcpSettings().enableMcpClients.volatileValue());
    };
    syncEnabled();
    QObject::connect(
        &mcpSettings().enableMcpClients,
        &Utils::BoolAspect::volatileValueChanged,
        outer,
        syncEnabled);

    QObject::connect(
        presetsCombo,
        &QComboBox::currentIndexChanged,
        outer,
        [presetsCombo, presets](int idx) {
            if (idx <= 0)
                return;
            const int presetIdx = idx - 1;
            if (presetIdx < 0 || presetIdx >= presets.size()) {
                presetsCombo->setCurrentIndex(0);
                return;
            }
            Mcp::McpClientsManager::instance().addServer(
                presets[presetIdx].defaultName, presets[presetIdx].body);
            // Snap back to the placeholder so the next pick fires again even
            // if the user chooses the same preset twice.
            presetsCombo->setCurrentIndex(0);
        });

    parent.addItem(outer);
}

} // namespace QodeAssist::Settings
