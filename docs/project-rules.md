# Project Rules Configuration

QodeAssist supports project-specific rules to customize AI behavior for your codebase. Create a `.qodeassist/rules/` directory in your project root.

## Quick Start

```bash
mkdir -p .qodeassist/rules/{common,completion,chat,quickrefactor}
```

## Directory Structure

```
.qodeassist/
└── rules/
    ├── common/           # Applied to all contexts
    ├── completion/       # Code completion only
    ├── chat/            # Chat assistant only
    └── quickrefactor/   # Quick refactor only
```

All `.md` files in each directory are automatically loaded and added to the system prompt.

## Example

Create `.qodeassist/rules/common/general.md`:

```markdown
# Project Guidelines
- Use snake_case for private members
- Prefix interfaces with 'I'
- Always document public APIs
- Prefer Qt containers over STL
```

