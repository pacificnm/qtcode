# QTCommands Specification

Milestone: [M7: QTCommands](../milestones/m07-qtcommands.md)

ADR: [ADR 0009: QTCommands must be repo-native](../adrs/0009-qtcommands-repo-native.md)

Panel ADR: [ADR 0013: Command Library is a persistent repo-native right panel](../adrs/0013-command-library-panel.md)

Design: [QTCommands design](../design/qtcommands-design.md)

Feature: [Command Library feature](../features/command-library.md)

Panel design: [Command Library panel design](../design/command-library-panel.md)

## Purpose

QTCommands defines a repo-native command system for reusable AI and developer workflows in QTCode.
Commands live inside `.qtcode/` so they can be versioned, reviewed, validated, and reused by any
agent or developer working in the repository.

## Design Principles

- Repo-native first.
- Provider-neutral.
- Reviewable in Git.
- Portable across machines and agents.
- Explicit about inputs, templates, examples, and validation.
- Prefer project conventions over ad hoc generation.

## Repository Layout

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
  memory/
    command-usage.md
```

### Directory Rules

- `commands/` contains executable command definitions in YAML.
- `templates/` contains text templates rendered by commands.
- `rules/` contains project conventions that commands should follow.
- `examples/` contains representative outputs and reference implementations.
- `validation/` contains repo-level validation settings and shared checks.
- `memory/` contains notes intended for long-term memory indexing and agent recall.
- All paths are repository-relative and committed to Git.

## Initialization

When the user initializes Command Library for a repository, QTCode should:

1. Detect whether the active repository has `.qtcode/`.
2. Create the directory tree if it is missing.
3. Create the default scaffold files, including `.qtcode/validation/validation-rules.yaml`.
4. Leave existing files untouched.
5. Mark the repository as Command Library ready in the UI.

Initialization should not move commands into editor-local settings or any external registry.

## Command File Format

The canonical command definition lives in `.qtcode/commands/*.yaml`.

### Required Fields

- `name`: Stable command identifier and lookup key, usually kebab-case.
- `title`: Human-readable display name.
- `description`: One-sentence description of what the command does.
- `category`: Broad grouping such as `ui`, `api`, `docs`, or `tests`.
- `version`: Schema or command version.
- `inputs`: Declared input variables for the command.
- `template`: Repository-relative template path.
- `rules`: Repository-relative rule file paths.
- `examples`: Repository-relative example file paths.
- `validation`: Validation settings for the command.

### Recommended Fields

- `tags`: Search keywords and project vocabulary.
- `status`: `draft`, `stable`, or `deprecated`.
- `aliases`: Alternate command names or lookup keys.
- `memoryHints`: Phrases that help agents and memory retrieval find the command.
- `defaultOutput`: Suggested output path for generated changes.
- `outputs`: Multiple generated artifacts when a command creates more than one file.

### Canonical YAML Shape

```yaml
name: create-delete-button
title: Create Delete Button
description: Creates a project-standard delete button with confirmation behavior.
category: ui
version: 1

inputs:
  - name: entityName
    label: Entity Name
    type: string
    required: true
    example: Artist

  - name: endpoint
    label: Delete Endpoint
    type: string
    required: true
    example: endpoints.artist.delete

template: ../templates/delete-button.tsx.tpl

rules:
  - ../rules/project-patterns.md
  - ../rules/import-order.md
  - ../rules/naming-rules.md

examples:
  - ../examples/artist-delete-button.tsx

validation:
  requireJsdoc: true
  preserveFunctionNames: true
  enforceImportOrder: true
  requireConfiguredEndpoint: true
```

### Compatibility Note

Older documentation may refer to `id`. The new canonical identifier is `name`. If both appear during
migration, `name` should be treated as the primary key.

## Input Schema

Each item in `inputs` declares one value needed to render the command.

### Input Fields

- `name`: Template and validation key.
- `label`: UI label for the form.
- `type`: `string`, `boolean`, `number`, `enum`, `path`, `identifier`, `list`, or `json`.
- `required`: Whether the input must be supplied before render.
- `example`: Helpful sample value.
- `defaultValue`: Fallback value when the user does not provide one.

### Input Rules

- Required inputs must be validated before rendering.
- Defaults may come from project context, but the final render must be explicit.
- Missing required values should produce a diagnostic before apply.

## Templates

Templates are repository-relative text files stored under `.qtcode/templates/`.

### Template Rules

- Templates must be deterministic.
- Templates must not depend on an AI provider.
- Templates may reference declared inputs and supported context values only.
- Missing required placeholders must fail validation.

### Placeholder Format

- `{{entityName}}`
- `{{endpoint}}`
- `{{componentName}}`

QTCode should report unresolved placeholders in preview and validation output.

## Rules

Rules are Markdown documents stored under `.qtcode/rules/`.

Rules should describe:

- naming conventions
- import ordering
- endpoint usage
- component structure
- JSDoc expectations
- route patterns
- folder and file placement
- architecture constraints

Referenced rule files must exist before the command is considered valid.

## Examples

Examples are repository-relative reference implementations stored under `.qtcode/examples/`.

Examples should:

- show a valid output shape
- help agents infer project vocabulary
- support command explanation
- make validation intent concrete

Referenced example files must exist before the command is considered valid.

## Validation

Validation protects project conventions and prevents accidental drift.

### Validation Sources

- per-command validation settings in the command YAML
- repo-level shared rules in `.qtcode/validation/validation-rules.yaml`
- rendered output checks
- file existence checks for templates, rules, and examples

### Minimum Validation Checks

- invalid YAML
- missing required inputs
- missing template file
- missing rule file
- missing example file
- unresolved template variables
- empty generated output

### Diagnostic Shape

Validation diagnostics should be structured so the UI can render them consistently.

- `severity`: `info`, `warning`, or `error`
- `code`: stable diagnostic identifier
- `message`: human-readable explanation
- `filePath`: optional related file path

Errors block apply. Warnings remain visible. Info messages are advisory.

## Command Discovery

QTCode must discover commands from the active repository.

### Discovery Rules

- Scan `.qtcode/commands/*.yaml` and `.qtcode/commands/*.yml`.
- Prefer repository-local commands over any fallback source.
- Expose `name`, `title`, `description`, `category`, `tags`, and `status` in the index when present.
- Load templates, rules, and examples lazily.

### Search Fields

Commands should be searchable by:

- name
- title
- description
- category
- tags
- aliases
- memoryHints
- referenced rules and examples

## Execution

Execution means loading a command, resolving inputs, rendering templates, validating output, and
producing a preview before any change is applied.

### Execution Flow

1. Resolve the command by `name` or alias.
2. Load and parse the YAML definition.
3. Resolve input values from the UI, CLI, agent context, or defaults.
4. Load the template and referenced files.
5. Render output deterministically.
6. Run validation checks.
7. Show preview and diff.
8. Apply, copy, or discard only after user approval.

### Execution Sources

Commands may be launched from:

- Command Library UI actions
- CLI-style entry points
- agent tool calls
- suggested command prompts from context analysis

## Validation Results

Validation results should return:

- `severity`: `info`, `warning`, or `error`
- `code`: stable diagnostic code
- `message`: human-readable explanation
- `filePath`: optional file or file reference

Recommended codes:

- `invalid-yaml`
- `missing-required-input`
- `missing-template-file`
- `missing-rule-file`
- `missing-example-file`
- `unresolved-template-variable`
- `empty-generated-output`

## Command Library UI Contract

The UI contract for the Command Library is documented in
[Command Library panel design](../design/command-library-panel.md). In short:

- the right-side panel is manually opened and closed
- the current repository must be visible in the header
- commands, templates, rules, examples, and validation each have their own section
- preview must happen before apply
- direct overwrite without approval is not allowed

## Agent Integration

Agents must treat `.qtcode/commands` as the authoritative source for reusable project patterns.

### Agent Rules

- Prefer an existing command when the task matches a known project pattern.
- Do not invent new architecture when a command covers the use case.
- Use command examples and rules to preserve project vocabulary.
- Run validation before asking the user to approve changes.

## MCP Integration

### Search Memory MCP

Search Memory MCP should be used to retrieve long-term notes about:

- when to use a command
- when not to use a command
- project-specific wording and naming
- historical decisions around repeated workflows

### Context MCP

Context MCP should provide the current:

- repository
- file
- task
- architecture state
- selected diff or target files

### Source Of Truth

`.qtcode/commands` is the source of truth for executable project patterns. MCP provides retrieval
and explanation, not the canonical command definition.

## Compatibility And Portability

- Commands must work from any machine with the repository checked out.
- Commands must not require a specific editor plugin to be useful.
- Commands must not depend on a single model vendor or agent provider.
- Commands should degrade gracefully if MCP services are unavailable.

