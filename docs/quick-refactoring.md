# Quick Refactoring Feature

## Setup

Since this is actually a small chat with redirected output, the main settings of the provider, model and template are taken from the chat settings.

## Using

The request to model consist of instructions to model, selection code and cursor position.

The default instruction is: "Refactor the code to improve its quality and maintainability." and sending if text field is empty.

Also there buttons to quick call instractions:
- Repeat latest instruction, will activate after sending first request in QtCreator session
- Improve current selection code
- Suggestion alternative variant of selection code
- Other instructions[TBD]

