# Quick Refactoring Feature

Quick Refactoring provides AI-assisted code improvements directly in your editor. Select code, press the hotkey, and get instant refactoring suggestions.

## Overview

Quick Refactoring provides AI-assisted code refactoring with its own dedicated provider and model configuration, allowing you to use different settings than your Chat Assistant. You can use built-in quick actions or create your own custom instructions library.

## Setup

Quick Refactoring has independent configuration separate from Chat Assistant:

### Provider & Model Configuration

Configure provider and model in: `Qt Creator → Preferences → QodeAssist → General Settings`

Under the **Quick Refactor** section, you can set:
- **Provider**: Choose from Ollama, Claude, OpenAI, Google AI, etc.
- **Model**: Select the specific model for refactoring tasks
- **Template**: Choose the chat template for this provider
- **URL**: Set the API endpoint
- **API Key**: Configure authentication (for cloud providers)

This allows you to:
- Use a faster/cheaper model for refactoring than for chat
- Use a local model for refactoring and cloud model for chat
- Optimize costs by using different providers for different tasks

### Quick Refactor Settings

Additional refactoring-specific options in: `Qt Creator → Preferences → QodeAssist → Quick Refactor`

Configure:
- **Context Settings**: How much code context to send
  - Read full file or N lines before/after selection
- **LLM Parameters**: Temperature, max tokens, top_p, top_k
- **Advanced Options**: Penalties, context window size
- **Features**: Tool calling, extended thinking mode
- **System Prompt**: Customize the base prompt for refactoring
- **How quick refactor looks**: Display type and sizes

## Using Quick Refactoring

### Basic Usage

1. **Select Code** (or place cursor for line-level refactoring)
2. **Trigger Quick Refactor**: Press `Ctrl+Alt+R` (Windows/Linux) or `⌥⌘R` (macOS)
3. **Choose Action**:
   - Use a built-in quick action button
   - Select a custom instruction from the dropdown
   - Type your own instruction
4. **Get Results**: AI generates refactored code directly replacing your selection

### Quick Action Buttons

The dialog provides three built-in quick actions:

| Button | Description |
|--------|-------------|
| **Repeat Last** | Reuses the last instruction from your session (enabled after first use) |
| **Improve Code** | Enhances readability, efficiency, and maintainability with best practices |
| **Alternative Solution** | Suggests different implementation approaches and patterns |

## Custom Instructions

### Overview

Custom Instructions allow you to create a reusable library of refactoring templates. Instead of typing the same instructions repeatedly, save them once and access them instantly through the searchable dropdown.

**Key Features:**
- **Quick Access**: Search and select instructions by typing
- **Flexible**: Use as-is or add extra details for each use
- **Manageable**: Easy create, edit, and delete interface
- **Persistent**: Instructions saved locally and loaded on startup
- **Accessible**: Direct access to instruction files folder

### Creating Custom Instructions

1. Click the **`+`** button in the Quick Refactor dialog
2. Fill in the form:
   - **Name**: Short descriptive title (e.g., "Add Documentation")
   - **Instruction Body**: Detailed prompt for the LLM

**Example instruction:**

```
Name: Add Documentation
Body: Add comprehensive documentation to the selected code or code afer cursor following: 
      Doxygen style. Include parameter descriptions, return value 
      documentation, and usage examples where applicable.
```


### Using Custom Instructions

#### Method 1: Select and Use
1. Open Quick Refactor dialog (`Ctrl+Alt+R` / `⌥⌘R`)
2. Click the dropdown or start typing instruction name
3. Select instruction (autocomplete will help)
4. Optionally add extra details in the text field below
5. Press OK


#### Method 2: Search by Typing
1. Open Quick Refactor dialog
2. Start typing in the instruction dropdown (e.g., "doc...")
3. Autocomplete shows matching instructions
4. Select with arrow keys or click
5. Add optional details and execute

**Search Features:**
- Case-insensitive search
- Match anywhere in instruction name
- Keyboard navigation (arrow keys, Enter)
- Instant filtering as you type

### Combining Instructions with Additional Details

Custom instructions serve as **base templates** that you can augment with specific requirements:

**Example 1 - Use instruction as-is:**
```
Selected: "Add Documentation"
Additional text: [empty]
→ Sends: "Add comprehensive documentation..."
```

**Example 2 - Add specific requirements:**
```
Selected: "Optimize Performance"  
Additional text: "Focus on reducing memory allocations"
→ Sends: "Optimize Performance instructions... 

Focus on reducing memory allocations"
```

This approach allows maximum flexibility while maintaining a clean instruction library.

### Managing Custom Instructions

The Quick Refactor dialog provides full CRUD operations:

| Button | Action | Description |
|--------|--------|-------------|
| **+** | Add | Create new custom instruction |
| **✎** | Edit | Modify selected instruction |
| **−** | Delete | Remove selected instruction (with confirmation) |
| **📁** | Open Folder | Open instructions directory in file manager |

**Edit/Delete:**
- Select an instruction from dropdown (or type its name)
- Click Edit (✎) or Delete (−) button
- Confirm changes

<!-- PLACEHOLDER_IMAGE: Screenshot showing instruction management buttons -->

### Storage Location

Custom instructions are stored as JSON files in:

```
~/.config/QtProject/qtcreator/qodeassist/quick_refactor/instructions/
```

**File Naming Format:**
```
Instruction_Name_with_underscores_{unique-uuid}.json
```

**Examples:**
```
Add_Documentation_a7f3c92d-8e4b-4f1a-9c0e-1d2f3a4b5c6d.json
Optimize_Performance_3b8e4f9a-7c2d-4e1b-8f3a-9c1d2e3f4a5b.json
Fix_Code_Style_c5d6e7f8-9a0b-1c2d-3e4f-5a6b7c8d9e0f.json
```

**File Format:**
```json
{
    "id": "unique-uuid",
    "name": "Add Documentation",
    "body": "Add comprehensive documentation...",
    "version": "0.1"
}
```

### Backup and Sharing

Since instructions are simple JSON files, you can:

1. **Backup**: Copy the instructions directory
2. **Share**: Share JSON files with team members
3. **Version Control**: Add to your dotfiles repository
4. **Edit Manually**: Modify JSON files directly if needed

Click the **📁** button to quickly open the instructions folder in your file manager.

## Context and Scope

### What Gets Sent to the LLM

The LLM receives:
- **Selected Code** (or current line if no selection)
- **Context**: Surrounding code (configurable amount)
- **File Information**: Language, file path
- **Cursor Position**: Marked with `<cursor>` tag
- **Selection Markers**: `<selection_start>` and `<selection_end>` tags
- **Your Instructions**: Built-in, custom, or typed
- **Project Rules**: If configured (see [Project Rules](project-rules.md))

### Context Configuration

Configure context amount in: `Qt Creator → Preferences → QodeAssist → Quick Refactor`

Options:
- **Read Full File**: Send entire file as context
- **Read File Parts**: Send N lines before/after selection (configurable in "Read Strings Before/After Cursor")

## Advanced Settings

Access all refactoring settings in: `Qt Creator → Preferences → QodeAssist → Quick Refactor`

### Available Options:

**Context Settings:**
- Read full file vs. file parts
- Number of lines before/after cursor

**LLM Parameters:**
- Temperature (creativity/randomness)
- Max tokens (response length)
- Top P (nucleus sampling)
- Top K (vocabulary filtering)
- Presence penalty
- Frequency penalty

**Ollama-specific:**
- Lifetime parameter
- Context window size

**Features:**
- Enable/disable tool calling
- Extended thinking mode (for supported models)
- Thinking budget and max tokens

**Customization:**
- System prompt editing
- Use open files in context (optional)

## Troubleshooting

### Instruction Not Found
- Ensure you've typed the exact name or selected from dropdown
- Check if instruction file exists in instructions directory
- Reload Qt Creator if instructions were added externally

### Poor Results
- Try adding more specific details in the additional text field
- Adjust context settings to provide more/less code
- Use extended thinking mode for complex refactorings
- Check if your model supports the complexity of the task

### Instructions Not Loading
- Verify folder exists: `~/.config/QtProject/qtcreator/qodeassist/quick_refactor/instructions/`
- Check JSON file format validity
- Review Qt Creator logs for parsing errors
- Try restarting Qt Creator

Fully local setup for offline or secure environments.

## Related Documentation

- [Project Rules](project-rules.md) - Project-specific AI behavior customization
- [File Context](file-context.md) - Attaching files to chat context
- [Ignoring Files](ignoring-files.md) - Exclude files from AI context
- [Provider Configuration](../README.md#configuration) - Setting up LLM providers

