// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "AgentsSettings.hpp"

#include <QDir>

#include "SettingsConstants.hpp"
#include "SettingsTr.hpp"

namespace QodeAssist::Settings {

AgentsSettings &agentsSettings()
{
    static AgentsSettings settings;
    return settings;
}

AgentsSettings::AgentsSettings()
{
    setAutoApply(false);

    setDisplayName(Tr::tr("Agents"));

    agentExtraPaths.setSettingsKey(Constants::AGENT_EXTRA_PATHS);
    agentExtraPaths.setLabelText(Tr::tr("Extra PATH for launching agents:"));
    agentExtraPaths.setDisplayStyle(Utils::StringAspect::LineEditDisplay);
    agentExtraPaths.setToolTip(
        Tr::tr(
            "Directories to prepend to PATH when launching ACP agents, and to search for "
            "the agent's executable. Useful when Qt Creator is started from the dock and "
            "doesn't see Homebrew, nvm, uv, etc. Separate multiple entries with '%1'.")
            .arg(QDir::listSeparator()));
#ifdef Q_OS_MACOS
    agentExtraPaths.setDefaultValue(QStringLiteral("/opt/homebrew/bin:/usr/local/bin"));
#else
    agentExtraPaths.setDefaultValue(QString{});
#endif

    agentForwardedVariables.setSettingsKey(Constants::AGENT_FORWARDED_VARIABLES);
    agentForwardedVariables.setLabelText(Tr::tr("Forward these variables to agents:"));
    agentForwardedVariables.setDisplayStyle(Utils::StringAspect::LineEditDisplay);
    agentForwardedVariables.setToolTip(
        Tr::tr(
            "Names of environment variables (not their values) passed on to ACP agents, "
            "separated by spaces or commas. A variable already present in Qt Creator's own "
            "environment is used as is; on macOS and Linux the rest are read once from a login "
            "shell, so a token exported in your shell profile reaches agents even when Qt "
            "Creator was started from the dock."));
    agentForwardedVariables.setDefaultValue(QStringLiteral("CLAUDE_CODE_OAUTH_TOKEN"));

    readSettings();
}

} // namespace QodeAssist::Settings
