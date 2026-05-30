# File Context Feature

QodeAssist provides two powerful ways to include source code files in your chat conversations: Attachments and Linked Files. Each serves a distinct purpose and helps provide better context for the AI assistant.

## Attached Files

Attachments are designed for one-time code analysis and specific queries:
- Files are sent as part of the current message
- The content is a snapshot taken at send time: it is stored with the chat
  and stays in the conversation history exactly as sent, even if the file
  changes on disk later
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
- Files are automatically refreshed — every request re-reads them and sends
  the latest content from disk
- The snapshot travels next to your latest message and is never duplicated
  into the conversation history, so linked files do not bloat the chat as it
  grows
- Perfect for:
  - Long-term refactoring discussions
  - Complex architectural changes
  - Multi-file implementations
  - Maintaining context across related questions
- Can be managed using the link icon in the chat interface
- Supports automatic syncing with open editor files (can be enabled in settings)
- Files can be added/removed at any time during the conversation

