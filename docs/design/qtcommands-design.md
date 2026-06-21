# QTCommands Design

Milestone: [M7: QTCommands](../milestones/m07-qtcommands.md)

ADR: [ADR 0009: QTCommands must be repo-native](../adrs/0009-qtcommands-repo-native.md)

Specification: [QTCommands specification](../specs/qtcommands-spec.md)

Detailed panel design: [Command Library panel](command-library-panel.md)

## Product Intent

QTCommands makes QTCode a better long-lived AI cockpit by storing reusable project patterns in the repository itself. When context is reset, the command system preserves the project rules, vocabulary, examples, and templates that agents need to generate consistent code.

## User Experience Goals

- Make reusable commands easy to find.
- Make command execution feel explicit and reviewable.
- Make generated output predictable before it is applied.
- Make command suggestions feel grounded in the active project.
- Keep the system useful to humans and agents equally.

## Information Architecture

### Main Surfaces

- Command Library panel
- Command detail view
- Command run form
- Preview and diff view
- Validation results view
- Agent-facing command interface

### Supporting Surfaces

- Search Memory results when commands are relevant
- Context summary showing repository and task state
- Repository tree entry for `.qtcode/`
- Command history and prior runs

## Command Library Panel

The Command Library panel is a dedicated browser for `.qtcode/commands`.

### Panel Content

- Search box
- Filters by category, status, and tags
- Command list with name, summary, and status
- Source path and last modified metadata
- Quick actions for explain, run, validate, and edit

### Panel Behavior

- Search should prioritize exact id and alias matches.
- Filters should update results immediately.
- Commands with warnings or deprecated status should be visually distinct.
- The panel should surface project vocabulary, such as `Blocks` instead of `Widgets`, when it exists in the repository rules.

## Command Detail View

The detail view shows one command and everything needed to trust it.

### Detail Sections

- Command identity: id, name, summary, status, version.
- Inputs: fields, types, defaults, and validation hints.
- Template: template path and output targets.
- Examples: reference files and expected patterns.
- Rules: linked project rules and anchor references.
- Validation: required checks and result history.
- Usage: where the command was used and why it was suggested.

### Detail Behavior

- Show a plain-language explanation first.
- Keep the YAML source accessible for review.
- Link to the template, example, and rule files.
- Indicate whether the command is stable, draft, or deprecated.

## Command Run Form

The run form is generated from the command schema.

### Form Behavior

- Show required inputs first.
- Populate defaults and context-derived values when possible.
- Provide inline hints for project-specific vocabulary and endpoint choices.
- Let the user override inferred values before execution.
- Show a short explanation of how each value affects the generated result.

### Context-Aware Input Generation

The form may prefill values from:

- current file name
- selected symbol
- repository conventions
- current task text
- memory search results
- project settings

## Preview And Diff

Commands that generate or modify code must present a preview before changes are applied.

### Preview Requirements

- Show the rendered output before apply.
- Show the diff against the target file when the command patches existing code.
- Highlight any files that will be created, updated, or removed.
- Make it obvious when validation failed and why.

### Diff UX

- Keep generated text and repository changes side by side when practical.
- Group file-level changes by output path.
- Mark risky or destructive operations clearly.
- Allow the user to accept, reject, or edit before apply.

## Validation Results

Validation results are a first-class part of the workflow.

### Result Presentation

- Errors should block apply.
- Warnings should remain visible even if the user proceeds.
- Validation output should map back to a command, template, input, or project rule.
- The UI should show remediation text whenever possible.

### Validation Examples

- preserve existing function names
- include JSDoc
- use project import ordering
- use configured endpoints
- follow established component patterns
- do not invent new architecture when a command exists

## Agent-Facing Command Interface

QTCode should expose commands to agents through a neutral interface that does not assume one provider.

### Agent Responsibilities

- Discover commands from the active repository.
- Prefer commands over free-form generation when a match exists.
- Pass context, inputs, and selected files into the command runtime.
- Request preview and validation before suggesting apply.

### Agent Interaction Model

Agents should receive:

- command metadata
- input schema
- project rules
- example references
- validation requirements
- rendered result summary

This keeps the agent grounded in the repo rather than in generic patterns.

## Support For Codex, Cursor, GitHub Copilot, And Future Agents

QTCommands should not rely on one provider's proprietary runtime.

### Shared Expectations

- Commands are stored in Git.
- Commands are rendered by QTCode, not by the provider.
- Agents can be built around the same command source.
- Provider adapters should only handle how an agent receives a command, not what the command means.

### Practical Implications

- Codex can read and suggest the repo-native command.
- Cursor can surface the command definition in the project tree or agent panel.
- GitHub Copilot can be guided by the same repository files and rule documents.
- Future agents can consume the same `.qtcode/commands` schema without migration work.

## MCP Integration Design

### Search Memory MCP

Search Memory MCP should help QTCode learn:

- when a command should be used
- which commands are preferred for a task
- what project vocabulary applies to a pattern
- historical examples of command usage

### Context MCP

Context MCP should provide:

- active repository identity
- current file and selection
- task text or user prompt
- nearby architectural context
- selected diff or issue context

### Combined Flow

1. The user asks for work in plain language.
2. QTCode queries Search Memory MCP for command usage history.
3. QTCode queries Context MCP for the current project and task state.
4. QTCode resolves the best matching command from `.qtcode/commands`.
5. QTCode presents the command, inputs, preview, and validation before apply.

## Repository Integration

The command system should appear in the repository as a normal, reviewable set of files.

### Repository UX

- `.qtcode/` should be visible in the project tree.
- Command files should open like other text files.
- Changes to commands should be included in diffs and pull requests.
- Command edits should be validated before commit whenever possible.

## Future UI Extensions

- A command history panel with successful and failed runs.
- Command dependency graphing for shared templates and rules.
- Suggested command creation when repeated patterns appear in sessions.
- Repository-level command dashboards for larger codebases.

## Example Workflow

1. User asks: "Add delete support for ArtPiece."
2. QTCode queries memory and context.
3. QTCode identifies `create-delete-button` as the approved project pattern.
4. QTCode loads `.qtcode/commands/create-delete-button.yaml`.
5. QTCode renders the template with the correct inputs.
6. QTCode shows the preview and diff.
7. QTCode validates the output.
8. The user approves or edits the generated code.

This flow keeps the agent anchored to the repo's approved patterns instead of re-deriving them from scratch.
