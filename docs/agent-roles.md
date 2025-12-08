# Agent Roles

Agent Roles allow you to define different AI personas with specialized system prompts for various tasks. Switch between roles instantly in the chat interface to adapt the AI's behavior to your current needs.

## Overview

Agent Roles are reusable system prompt configurations that modify how the AI assistant responds. Instead of manually changing system prompts, you can create roles like "Developer", "Code Reviewer", or "Documentation Writer" and switch between them with a single click.

**Key Features:**
- **Quick Switching**: Change roles from the chat toolbar dropdown
- **Custom Prompts**: Each role has its own specialized system prompt
- **Built-in Roles**: Pre-configured Developer and Code Reviewer roles
- **Persistent**: Roles are saved locally and loaded on startup
- **Extensible**: Create unlimited custom roles for different tasks

## Default Roles

QodeAssist comes with three built-in roles:

### Developer
Experienced Qt/C++ developer with a structured workflow: analyze the problem, propose a solution, wait for approval, then implement. Best for implementation tasks where you want thoughtful, minimal code changes.

### Code Reviewer
Expert C++/QML code reviewer specializing in C++20 and Qt6. Checks for bugs, memory leaks, thread safety, Qt patterns, and production readiness. Provides direct, specific feedback with code examples.

### Researcher
Research-oriented developer who investigates problems and explores solutions. Analyzes problems, presents multiple approaches with trade-offs, and recommends the best option. Does not write implementation code — focuses on helping you make informed decisions.

## Using Agent Roles

### Switching Roles in Chat

1. Open the Chat Assistant (side panel, bottom panel, or popup window)
2. Locate the **Role selector** dropdown in the top toolbar (next to the configuration selector)
3. Select a role from the dropdown
4. The AI will now use the selected role's system prompt

**Note**: Selecting "No Role" uses only the base system prompt without role specialization.

### Viewing Active Role

Click the **Context** button (📋) in the chat toolbar to view:
- Base system prompt
- Current agent role and its system prompt
- Active project rules

## Managing Agent Roles

### Opening the Role Manager

Navigate to: `Qt Creator → Preferences → QodeAssist → Chat Assistant`

Scroll down to the **Agent Roles** section where you can manage all your roles.

### Creating a New Role

1. Click **Add...** button
2. Fill in the role details:
   - **Name**: Display name shown in the dropdown (e.g., "Documentation Writer")
   - **ID**: Unique identifier for the role file (e.g., "doc_writer")
   - **Description**: Brief explanation of the role's purpose
   - **System Prompt**: The specialized instructions for this role
3. Click **OK** to save

### Editing a Role

1. Select a role from the list
2. Click **Edit...** or double-click the role
3. Modify the fields as needed
4. Click **OK** to save changes

**Note**: Built-in roles cannot be edited directly. Duplicate them to create a modifiable copy.

### Duplicating a Role

1. Select a role to duplicate
2. Click **Duplicate...**
3. Modify the copy as needed
4. Click **OK** to save as a new role

### Deleting a Role

1. Select a custom role (built-in roles cannot be deleted)
2. Click **Delete**
3. Confirm deletion

## Creating Effective Roles

### System Prompt Tips

- **Be specific**: Clearly define the role's expertise and focus areas
- **Set expectations**: Describe the desired response format and style
- **Include guidelines**: Add specific rules or constraints for responses
- **Use structured prompts**: Break down complex roles into bullet points

## Storage Location

Agent roles are stored as JSON files in:

```
~/.config/QtProject/qtcreator/qodeassist/agent_roles/
```

**On different platforms:**
- **Linux**: `~/.config/QtProject/qtcreator/qodeassist/agent_roles/`
- **macOS**: `~/Library/Application Support/QtProject/Qt Creator/qodeassist/agent_roles/`
- **Windows**: `%APPDATA%\QtProject\qtcreator\qodeassist\agent_roles\`

### File Format

Each role is stored as a JSON file named `{id}.json`:

```json
{
    "id": "doc_writer",
    "name": "Documentation Writer",
    "description": "Technical documentation and code comments",
    "systemPrompt": "You are a technical documentation specialist...",
    "isBuiltin": false
}
```

### Manual Editing

You can:
- Edit JSON files directly in any text editor
- Copy role files between machines
- Share roles with team members
- Version control your roles
- Click **Open Roles Folder...** to quickly access the directory

## How Roles Work

When a role is selected, the final system prompt is composed as:

```
┌─────────────────────────────────────────────────┐
│ Final System Prompt = Base Prompt + Role Prompt │
├─────────────────────────────────────────────────┤
│ 1. Base System Prompt (from Chat Settings)      │
│ 2. Agent Role System Prompt                     │
│ 3. Project Rules (common/ + chat/)              │
│ 4. Linked Files Context                         │
└─────────────────────────────────────────────────┘
```

This allows roles to augment rather than replace your base configuration.

## Best Practices

1. **Keep roles focused**: Each role should have a clear, specific purpose
2. **Use descriptive names**: Make it easy to identify roles at a glance
3. **Test your prompts**: Verify roles produce the expected behavior
4. **Iterate and improve**: Refine prompts based on AI responses
5. **Share with team**: Export and share useful roles with colleagues

## Troubleshooting

### Role Not Appearing in Dropdown
- Restart Qt Creator after adding roles manually
- Check JSON file format validity
- Verify file is in the correct directory

### Role Behavior Not as Expected
- Review the system prompt for clarity
- Check if base system prompt conflicts with role prompt
- Try a more specific or detailed prompt

## Related Documentation

- [Project Rules](project-rules.md) - Project-specific AI behavior customization
- [Chat Assistant Features](../README.md#chat-assistant) - Overview of chat functionality
- [File Context](file-context.md) - Attaching files to chat context

