# File Context Feature

QodeAssist lets you include source code files in your chat conversations as attachments.

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
- Files can also be added by dragging them onto the chat panel or with `@` mentions

For persistent, conversation-long context, enable tools (or use an ACP agent): the
model reads the files it needs from the project itself, always seeing the latest
content from disk.
