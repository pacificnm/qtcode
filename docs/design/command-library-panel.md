# Command Library Panel Design

Milestone: [M7: QTCommands](../milestones/m07-qtcommands.md)

ADR: [ADR 0013: Command Library is a persistent repo-native right panel](../adrs/0013-command-library-panel.md)

Specification: [QTCommands specification](../specs/qtcommands-spec.md)

## Product Intent

The Command Library panel makes repo-native commands visible, reviewable, and easy to execute
without turning QTCode into a general-purpose editor macro system.

The panel exists to keep the active repository's reusable workflows close to the rest of the AI
cockpit, while preserving a strict separation between repository source of truth and UI state.

## Layout

The panel is a dedicated right-side panel controlled from the activity bar.

- It opens from a right-side Action icon labeled for the Command Library.
- It remains manually open or closed.
- It does not auto-hide.
- It should always make the current repository name or path visible.

The panel should be reusable enough for future repository-native agent tools without forcing each
tool to invent its own sidebar.

## Main Surfaces

### Header

- Title: `Command Library`
- Repository label: current repo name or path
- Close button

### Navigation

- Commands
- Templates
- Rules
- Examples
- Validation

### Content Areas

- Command list
- Command detail view
- Command run view
- Template list view
- Rules list view
- Examples list view
- Validation view

## Commands Section

The Commands section is the entry point for the feature.

### List Behavior

- Show all `.yaml` and `.yml` files in `.qtcode/commands`.
- Display title, name, category, and description.
- Show source path and any status markers.
- Keep the active repository context visible.

### Actions

- Run
- Edit
- Duplicate
- Delete
- Explain
- New Command
- Initialize `.qtcode`

### Empty States

- If `.qtcode/` is missing, show the initialize action instead of an empty command list.
- If `.qtcode/commands` exists but contains no commands, explain how to add the first one.

## Command Detail View

The detail view is the review surface for one command.

### Detail Content

- metadata
- inputs
- linked template
- linked rules
- linked examples
- validation settings
- YAML source reference

### Run Flow

1. Load the selected command YAML.
2. Validate command metadata.
3. Render the template with user inputs.
4. Present preview and diff.
5. Show validation results.
6. Allow apply, copy, or discard.

The UI must not overwrite existing files without explicit user approval.

## Templates Section

- List files from `.qtcode/templates`.
- Allow viewing and editing template files.
- Show which commands reference each template.
- Surface placeholder variables clearly.

Template support should remain simple: plain text replacement for `{{variable}}` placeholders.

## Rules Section

- List files from `.qtcode/rules`.
- Allow viewing and editing Markdown rule files.
- Provide a way to create new rule documents.
- Show which commands reference each rule file.

Rules are plain project instructions, not executable code.

## Examples Section

- List files from `.qtcode/examples`.
- Allow viewing example implementations.
- Show which commands reference each example.
- Treat examples as known-good reference material.

## Validation Section

- List per-command validation settings.
- Show repo-level validation files.
- Display validation results after preview generation.
- Keep validation extensible so deeper checks can be added later.

Validation results should clearly distinguish:

- errors that block apply
- warnings that need review
- info that documents a decision or limitation

## Service and Model Split

The panel should not parse files directly inside widgets.

Suggested responsibilities:

- `CommandLibraryService` owns repository detection, initialization, listing, loading, saving,
  and deletion.
- `QtCommandTemplateRenderer` handles placeholder replacement and unresolved variable reporting.
- `QtCommandValidator` builds structured diagnostics.
- `CommandLibraryPanel` coordinates the sections and selection state.

## MCP and Agent Hooks

The panel should expose design hooks for future MCP integrations without making MCP a hard runtime
dependency.

- Search Memory can recall when a command should be used.
- Context MCP can provide current task and file context.
- The repo files remain the canonical source of truth.

## Visual Tone

The panel should feel like a native repository tool, not a temporary prompt helper.

- Dense but readable.
- Compact metadata.
- Explicit provenance.
- Strong distinction between repo content and generated preview.

