# File Context Feature

QodeAssist provides two powerful ways to include source code files in your chat conversations: Attachments and Linked Files. Each serves a distinct purpose and helps provide better context for the AI assistant.

## Attached Files

Attachments are designed for one-time code analysis and specific queries:
- Files are included only in the current message
- Content is discarded after the message is processed
- Ideal for:
  - Getting specific feedback on code changes
  - Code review requests
  - Analyzing isolated code segments
  - Quick implementation questions
- Files can be attached using the paperclip icon in the chat interface
- Multiple files can be attached to a single message

## Linked Files

Linked files provide persistent context throughout the conversation:

- Files remain accessible for the entire chat session
- Content is included in every message exchange
- Files are automatically refreshed - always using latest content from disk
- Perfect for:
  - Long-term refactoring discussions
  - Complex architectural changes
  - Multi-file implementations
  - Maintaining context across related questions
- Can be managed using the link icon in the chat interface
- Supports automatic syncing with open editor files (can be enabled in settings)
- Files can be added/removed at any time during the conversation

