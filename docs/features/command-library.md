# Command Library Feature

## Overview

The Command Library is QTCode's repo-native command system for reusable project workflows.
It lets developers and AI agents define commands, templates, rules, examples, and validation
inside the active repository under `.qtcode/`.

The feature exists so repeated patterns become durable project artifacts instead of one-off prompts.
That keeps command behavior visible in Git, portable across machines, and recoverable after context
resets.

## Why It Exists

- Reusable project patterns should live with the repository, not in editor-local settings.
- Generated changes should follow repository conventions instead of generic AI output.
- Humans need to review command definitions the same way they review code.
- Agents need a stable source of truth that does not depend on one provider or session.

## Repo-Native Storage

Command Library data belongs in the active repository under `.qtcode/`.
The repository should remain the source of truth even when MCP memory or context services are
unavailable.

Expected scaffold:

```text
.qtcode/
  commands/
    create-delete-button.yaml
    create-card-layout.yaml
    add-crud-route.yaml
  templates/
    delete-button.tsx.tpl
    card-layout.tsx.tpl
    crud-route.ts.tpl
  rules/
    project-patterns.md
    import-order.md
    naming-rules.md
  examples/
    artist-delete-button.tsx
    art-piece-card.tsx
  validation/
    validation-rules.yaml
```

`memory/` notes may also be added later for long-term recall, but they do not replace the
repository files above.

## UI Shape

The feature appears as a persistent right-side panel in QTCode.

- Open it from the right-side Action icon.
- Keep it manually open or closed; it must not auto-hide.
- Make the current repository obvious in the header.
- Let the user switch between Commands, Templates, Rules, Examples, and Validation.

The header should show:

- `Command Library`
- the current repo name or path
- a close button

## Core Sections

### Commands

Commands are reusable actions that agents can run when a task matches a known project pattern.
Each command lives in `.qtcode/commands/*.yaml` and can be run, edited, duplicated, explained,
validated, or deleted from the panel.

The command view should surface:

- title
- name
- category
- description
- linked template
- linked rules
- linked examples
- validation settings

### Templates

Templates are code snippets with variables such as `{{entityName}}`, `{{endpoint}}`, and
`{{componentName}}`.

Templates should be editable as plain text files and should show which commands reference them.

### Rules

Rules are project-specific instructions for agents and validators.
They cover import order, naming, JSDoc, API patterns, route patterns, and architecture constraints.

### Examples

Examples are known-good implementations that help agents understand the expected output shape.
They should be visible and readable from the panel, and each example should show its referencing
commands.

### Validation

Validation is the gate between rendered output and applying a generated change.
The panel should expose both per-command validation settings and the repo-level validation file.

Minimum validation outcomes:

- missing required inputs
- missing template file
- missing rule file
- missing example file
- unresolved template variables
- empty generated output
- invalid YAML

## Initialization Flow

When the user initializes Command Library for a repository, QTCode should:

1. Detect whether the active repository already has `.qtcode/`.
2. Create the directory tree if it is missing.
3. Create the expected starter files or empty placeholders.
4. Leave existing files untouched.
5. Confirm that the repository now has a repo-native Command Library source of truth.

## Agent Workflow

1. The agent checks the repository and available context.
2. The agent lists commands from `.qtcode/commands`.
3. The agent selects the best matching command.
4. QTCode renders the command template with explicit inputs.
5. QTCode shows preview, diff, and validation results.
6. The user approves, edits, copies, or discards the output.

## MCP Plan

- Search Memory should remember when a command is preferred for a repeated task.
- Context MCP should provide the current repository, file, selection, and task state.
- `.qtcode/commands` remains the source of truth for executable project patterns.
- MCP adds discovery and guidance, not command ownership.

## Example Command

The canonical example for this feature is a delete-button command that takes an entity name and a
configured endpoint, renders a repo-approved template, and validates the result before apply.

