# Ignoring Files

QodeAssist supports the ability to ignore files in context using a `.qodeassistignore` file. This allows you to exclude specific files from the context during code completion and in the chat assistant, which is especially useful for large projects.

## How to Use .qodeassistignore

- Create a `.qodeassistignore` file in the root directory of your project near CMakeLists.txt or pro.
- Add patterns for files and directories that should be excluded from the context.
- QodeAssist will automatically detect this file and apply the exclusion rules.

## .qodeassistignore File Format

The file format is similar to `.gitignore`:
- Each pattern is written on a separate line
- Empty lines are ignored
- Lines starting with `#` are considered comments
- Standard wildcards work the same as in .gitignore
- To negate a pattern, use `!` at the beginning of the line

## Example

```
# Ignore all files in the build directory
/build
*.tmp
# Ignore a specific file
src/generated/autogen.cpp
```

