// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "McpSettings.hpp"

#include <coreplugin/dialogs/ioptionspage.h>
#include <coreplugin/icore.h>
#include <utils/layoutbuilder.h>
#include <QApplication>
#include <QClipboard>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>

#include "SettingsConstants.hpp"
#include "SettingsTr.hpp"
#include "SettingsUtils.hpp"

namespace QodeAssist::Settings {

McpSettings &mcpSettings()
{
    static McpSettings settings;
    return settings;
}

McpSettings::McpSettings()
{
    setAutoApply(false);

    setDisplayName(Tr::tr("MCP"));

    enableMcpServer.setSettingsKey(Constants::MCP_ENABLE_SERVER);
    enableMcpServer.setLabelText(Tr::tr("Enable MCP server"));
    enableMcpServer.setToolTip(
        Tr::tr("Expose QodeAssist tools to external MCP clients over HTTP. "
               "Which tools are visible is controlled on the client side."));
    enableMcpServer.setDefaultValue(false);

    mcpServerPort.setSettingsKey(Constants::MCP_SERVER_PORT);
    mcpServerPort.setLabelText(Tr::tr("Server port"));
    mcpServerPort.setToolTip(
        Tr::tr("TCP port the MCP server listens on (localhost only). "
               "Requires restart of the server after change."));
    mcpServerPort.setRange(1, 65535);
    mcpServerPort.setDefaultValue(3456);

    resetToDefaults.m_buttonText = Tr::tr("Reset Page to Defaults");
    showConnectionInstructions.m_buttonText = Tr::tr("How to connect...");

    readSettings();

    setupConnections();

    setLayouter([this]() {
        using namespace Layouting;

        return Column{
            Row{Stretch{1}, resetToDefaults},
            Space{8},
            Group{
                title(Tr::tr("Server")),
                Column{
                    enableMcpServer,
                    mcpServerPort,
                    Row{Stretch{1}, showConnectionInstructions}}},
            Stretch{1}};
    });
}

void McpSettings::setupConnections()
{
    connect(
        &resetToDefaults,
        &ButtonAspect::clicked,
        this,
        &McpSettings::resetSettingsToDefaults);

    connect(
        &showConnectionInstructions,
        &ButtonAspect::clicked,
        this,
        &McpSettings::showConnectionInstructionsDialog);
}

void McpSettings::resetSettingsToDefaults()
{
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(
        Core::ICore::dialogParent(),
        Tr::tr("Reset Settings"),
        Tr::tr("Are you sure you want to reset all settings to default values?"),
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        resetAspect(enableMcpServer);
        resetAspect(mcpServerPort);
        writeSettings();
    }
}

void McpSettings::showConnectionInstructionsDialog()
{
    const quint16 port = static_cast<quint16>(mcpServerPort.volatileValue());
    const QString serverUrl = QString("http://127.0.0.1:%1/mcp").arg(port);

    const QString bridgeUrl = QStringLiteral("https://github.com/Palm1r/llmqore/releases");

    const QString directJson = QString(R"({
  "mcpServers": {
    "qodeassist": {
      "type": "sse",
      "url": "%1"
    }
  }
})").arg(serverUrl);

    const QString claudeCodeCmd
        = QString("claude mcp add --transport sse qodeassist %1").arg(serverUrl);

    const QString vscodeJson = QString(R"({
  "servers": {
    "qodeassist": {
      "type": "sse",
      "url": "%1"
    }
  }
})").arg(serverUrl);

    const QString bridgeJson = QString(R"({
  "port": 8808,
  "host": "127.0.0.1",
  "mcpServers": {
    "qodeassist": {
      "type": "sse",
      "url": "%1"
    }
  }
})").arg(serverUrl);

    const QString claudeDesktopJson = QStringLiteral(R"({
  "mcpServers": {
    "qodeassist": {
      "command": "<path-to>/mcp-bridge",
      "args": ["--stdio", "<path-to>/mcp-bridge.json"]
    }
  }
})");

    auto *dialog = new QDialog(Core::ICore::dialogParent());
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(Tr::tr("Connect to QodeAssist MCP"));
    dialog->resize(640, 480);

    auto *intro = new QLabel(
        Tr::tr("Server URL: <code>%1</code>. If your MCP client speaks HTTP/SSE "
               "natively, use the <b>Direct</b> tab. If it only speaks stdio "
               "(e.g. Claude Desktop), use the <b>Bridge</b> tab.")
            .arg(serverUrl),
        dialog);
    intro->setWordWrap(true);
    intro->setTextFormat(Qt::RichText);

    auto makeCodeBlock = [](QWidget *parent, const QString &text) -> QWidget * {
        auto *container = new QWidget(parent);
        auto *layout = new QVBoxLayout(container);
        layout->setContentsMargins(0, 0, 0, 0);

        auto *edit = new QPlainTextEdit(text, container);
        edit->setReadOnly(true);
        edit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
        edit->setLineWrapMode(QPlainTextEdit::NoWrap);
        edit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        edit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        edit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

        const int lines = text.count(QLatin1Char('\n')) + 1;
        const QFontMetrics fm(edit->font());
        const int contentHeight = lines * fm.lineSpacing();
        const int frame = 2 * edit->frameWidth();
        edit->setFixedHeight(contentHeight + frame + 8);

        auto *copyBtn = new QPushButton(Tr::tr("Copy"), container);
        copyBtn->setAutoDefault(false);
        copyBtn->setDefault(false);
        QObject::connect(copyBtn, &QPushButton::clicked, container, [edit] {
            QApplication::clipboard()->setText(edit->toPlainText());
        });

        auto *btnRow = new QHBoxLayout();
        btnRow->addStretch(1);
        btnRow->addWidget(copyBtn);

        layout->addWidget(edit);
        layout->addLayout(btnRow);
        return container;
    };

    auto *tabs = new QTabWidget(dialog);

    // Direct tab
    {
        auto *tab = new QWidget(tabs);
        auto *layout = new QVBoxLayout(tab);

        auto *hint = new QLabel(Tr::tr("<b>Claude Code</b> (CLI): run once —"), tab);
        hint->setTextFormat(Qt::RichText);
        layout->addWidget(hint);
        layout->addWidget(makeCodeBlock(tab, claudeCodeCmd));

        auto *vscodeHint = new QLabel(
            Tr::tr("<b>VS Code</b>: save as <code>.vscode/mcp.json</code> in the workspace:"),
            tab);
        vscodeHint->setTextFormat(Qt::RichText);
        vscodeHint->setWordWrap(true);
        layout->addWidget(vscodeHint);
        layout->addWidget(makeCodeBlock(tab, vscodeJson));

        auto *jsonHint = new QLabel(
            Tr::tr("Any other client that reads an <code>mcpServers</code> JSON block:"),
            tab);
        jsonHint->setTextFormat(Qt::RichText);
        jsonHint->setWordWrap(true);
        layout->addWidget(jsonHint);
        layout->addWidget(makeCodeBlock(tab, directJson));
        layout->addStretch(1);

        tabs->addTab(tab, Tr::tr("Direct (HTTP/SSE)"));
    }

    // Bridge tab
    {
        auto *tab = new QWidget(tabs);
        auto *layout = new QVBoxLayout(tab);

        auto *step1 = new QLabel(
            Tr::tr("<b>1.</b> Download <code>mcp-bridge</code> for your OS from "
                   "<a href=\"%1\">%1</a>.")
                .arg(bridgeUrl),
            tab);
        step1->setTextFormat(Qt::RichText);
        step1->setWordWrap(true);
        step1->setOpenExternalLinks(true);
        layout->addWidget(step1);

        auto *step2 = new QLabel(
            Tr::tr("<b>2.</b> Save the following as <code>mcp-bridge.json</code>:"), tab);
        step2->setTextFormat(Qt::RichText);
        layout->addWidget(step2);
        layout->addWidget(makeCodeBlock(tab, bridgeJson));

        auto *step3 = new QLabel(
            Tr::tr("<b>3.</b> Point the stdio-only client at the bridge. "
                   "Example for <code>claude_desktop_config.json</code>:"),
            tab);
        step3->setTextFormat(Qt::RichText);
        step3->setWordWrap(true);
        layout->addWidget(step3);
        layout->addWidget(makeCodeBlock(tab, claudeDesktopJson));
        layout->addStretch(1);

        tabs->addTab(tab, Tr::tr("Bridge (stdio)"));
    }

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
    QObject::connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::close);

    auto *layout = new QVBoxLayout(dialog);
    layout->addWidget(intro);
    layout->addWidget(tabs, 1);
    layout->addWidget(buttons);

    dialog->show();
}

class McpSettingsPage : public Core::IOptionsPage
{
public:
    McpSettingsPage()
    {
        setId(Constants::QODE_ASSIST_MCP_SETTINGS_PAGE_ID);
        setDisplayName(Tr::tr("MCP"));
        setCategory(Constants::QODE_ASSIST_GENERAL_OPTIONS_CATEGORY);
        setSettingsProvider([] { return &mcpSettings(); });
    }
};

const McpSettingsPage mcpSettingsPage;

} // namespace QodeAssist::Settings
