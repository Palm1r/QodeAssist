# ACP agents

QodeAssist can talk to external coding agents that speak the
[Agent Client Protocol](https://agentclientprotocol.com) (ACP). The **QodeAssist >
Agents** settings page lists the agents it knows about and lets you verify that one
starts and answers the ACP handshake.

## Where the list comes from

The list is merged from three sources. When the same agent `id` appears in more than
one, the higher entry wins:

1. **Your JSON files** in `qodeassist/agents/` inside the Qt Creator user resource
   directory (**Open Agents Folder...** opens it). Press **Reload** after editing.
2. **The ACP registry**, downloaded from
   `https://cdn.agentclientprotocol.com/registry/v1/latest/registry.json` when you press
   **Refresh from Registry** and cached on disk. There is no background polling: the
   list only changes when you ask for it.
3. **A bundled snapshot**, so the list is never empty on a fresh install or offline.

If a download fails, the cached copy and the bundled snapshot stay in place.

## Finding the agent's executable

Qt Creator started from the Dock or a launcher does not inherit your shell's `PATH`, so
`npx` or `uvx` installed through Homebrew, nvm or uv is often invisible to it — the agent
then fails to start with `execve: No such file or directory`. **Extra PATH for launching
agents** on the Agents page lists directories that are both searched for the executable
and prepended to the agent's own `PATH`. On macOS it defaults to
`/opt/homebrew/bin:/usr/local/bin`. An `env` entry in the agent definition still wins over
it.

## Credentials and other environment variables

Some agents authenticate through an environment variable rather than the ACP handshake —
the Claude adapter, for instance, declares no `authMethods` at all and reads
`CLAUDE_CODE_OAUTH_TOKEN`. Since a Qt Creator started from the dock inherits no shell
environment, that variable never reaches the agent and it fails with an expired-session
error.

**Forward these variables to agents** on the Agents page takes variable *names*, not
values, and defaults to `CLAUDE_CODE_OAUTH_TOKEN`. For each name QodeAssist uses the value
from its own environment when it has one — which covers Windows, and any platform when Qt
Creator was launched from a terminal. On macOS and Linux the remaining names are read once
per session from a login shell (`$SHELL -l -i -c env`), so a token exported in your shell
profile works without being copied anywhere. Nothing is stored in the settings but the
names.

An `env` entry in the agent definition still wins over a forwarded variable.

## Distributions

An entry describes how the agent is started:

- `npx` — launched as `npx -y <package> <args>`. Requires Node.js on `PATH`.
- `uvx` — launched as `uvx <package> <args>`. Requires uv on `PATH`.
- `command` — launched directly. This is the QodeAssist extension used by your own
  JSON files.
- `binary` — a downloadable platform archive. QodeAssist does not download binaries,
  so these agents are listed but cannot be started. Install the agent yourself and add
  a `command` entry for it.

## Defining your own agent

Create a `.json` file in the agents folder. A file holds either a single agent object
or a registry-shaped `{"agents": [...]}` document.

```json
{
  "id": "my-agent",
  "name": "My Agent",
  "version": "1.0.0",
  "description": "Locally installed agent",
  "distribution": {
    "command": {
      "cmd": "/usr/local/bin/my-agent",
      "args": ["acp"],
      "env": { "MY_AGENT_LOG": "debug" }
    }
  }
}
```

Reusing an `id` from the registry overrides that entry — which is how you make a
`binary` agent launchable after installing it manually:

```json
{
  "id": "cursor",
  "name": "Cursor",
  "distribution": {
    "command": { "cmd": "/usr/local/bin/cursor-agent", "args": ["acp"] }
  }
}
```

Agent definitions hold no secrets. Agents that need credentials authenticate through
the ACP handshake.

## Testing an agent

Select an agent and press **Test**. QodeAssist starts the process, runs the ACP
`initialize` handshake and reports the agent's name, version, protocol version,
whether it supports session persistence (`loadSession`), its prompt and MCP
capabilities, and its authentication methods. On failure it shows the error together
with the agent's own output.
