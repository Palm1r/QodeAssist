# Chat Summarization

Chat Summarization allows you to compress long conversations into concise AI-generated summaries. This helps save context tokens and makes it easier to continue work on complex topics without losing important information.

## Overview

When conversations grow long, they consume more context tokens with each message. Chat Summarization uses your configured Chat Assistant provider to create an intelligent summary that preserves:

- Key decisions and conclusions
- Technical details and code references
- Important context for continuing the conversation

**Key Features:**
- **One-click compression**: Summarize directly from the chat toolbar
- **Preserves original**: Creates a new chat file, keeping the original intact
- **Smart summaries**: AI extracts the most relevant information
- **Markdown formatted**: Summaries are well-structured and readable

## Using Chat Summarization

### Compressing a Chat

1. Open any chat with conversation history
2. Click the **Compress** button (📦) in the chat top bar
3. Wait for the AI to generate the summary
4. A new chat opens with the compressed summary

### What Gets Preserved

The summarization process:
- Maintains chronological flow of the discussion
- Keeps technical details, code snippets, and file references
- Preserves key decisions and conclusions
- Aims for 30-40% of the original conversation length

### What Gets Filtered

The following message types are excluded from summarization:
- Tool call results (file reads, searches)
- File edit blocks
- Thinking/reasoning blocks

## How It Works

```
┌─────────────────────────────────────────────────────────────┐
│                    CHAT SUMMARIZATION                       │
├─────────────────────────────────────────────────────────────┤
│  1. Original chat messages are collected                    │
│  2. Tool/thinking messages are filtered out                 │
│  3. AI generates a structured summary                       │
│  4. New chat file is created with summary as first message  │
│  5. Original chat remains unchanged                         │
└─────────────────────────────────────────────────────────────┘
```

### File Naming

Compressed chats are saved with a unique suffix:
```
original_chat.json → original_chat_a1b2c.json
```

Both files appear in your chat history, allowing you to switch between the full conversation and the summary.

## Best Practices

1. **Summarize at natural breakpoints**: Compress after completing a major task or topic
2. **Review the summary**: Ensure important details were captured before continuing
3. **Keep originals**: Don't delete original chats until you've verified the summary is sufficient
4. **Use for long sessions**: Most beneficial for conversations with 20+ messages

## When to Use

**Good candidates for summarization:**
- Long debugging sessions with resolved issues
- Feature implementation discussions with final decisions
- Research conversations where conclusions were reached
- Any chat approaching context limits

**Consider keeping full history for:**
- Ongoing work that may need exact message references
- Conversations with important code snippets you'll copy
- Discussions where the reasoning process matters

## Configuration

Chat Summarization uses your current Chat Assistant settings:
- **Provider**: Same as Chat Assistant (Settings → QodeAssist → General)
- **Model**: Same as Chat Assistant
- **Template**: Same as Chat Assistant

No additional configuration is required.

## Troubleshooting

### Compression Button Not Visible
- Ensure you have an active chat with messages
- Check that the chat top bar is visible

### Compression Fails
- Verify your Chat Assistant provider is configured correctly
- Check network connectivity
- Ensure the model supports chat completions

### Summary Missing Details
- The AI aims for 30-40% compression; some details may be condensed
- For critical information, keep the original chat
- Consider summarizing smaller conversation segments

## Related Documentation

- [Agent Roles](agent-roles.md) - Switch between AI personas
- [File Context](file-context.md) - Attach files to chat
- [Project Rules](project-rules.md) - Customize AI behavior
