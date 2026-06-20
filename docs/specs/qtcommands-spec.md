# QTCommands Specification

## Purpose

QTCommands defines a repo-native command system for reusable AI and developer workflows in QTCode. Commands live in `.qtcode/` so they can be versioned, reviewed, validated, and reused by any agent or developer working in the repository.

## Design Principles

- Repo-native first.
- Provider-neutral.
- Reviewable in Git.
- Portable across machines and agents.
- Explicit about inputs, templates, examples, and validation.
- Prefer project conventions over ad hoc generation.

## Directory Schema

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
  examples/
    artist-delete-button.tsx
    art-piece-card.tsx
  rules/
    project-patterns.md
    import-order.md
    naming-rules.md
  memory/
    command-usage.md
```

### Directory Rules

- `commands/` contains executable command definitions in YAML.
- `templates/` contains text templates rendered by commands.
- `examples/` contains representative outputs and reference implementations.
- `rules/` contains project conventions that commands should follow.
- `memory/` contains notes intended for long-term memory indexing and agent recall.
- All paths are repository-relative and committed to Git.

## Command File Format

Command definitions use YAML.

### Required Fields

- `id`: Stable command identifier, usually kebab-case.
- `name`: Human-readable command name.
- `summary`: One-sentence description of the command.
- `template`: Path to the primary template or template bundle.
- `inputs`: Declared input variables for the command.

### Recommended Fields

- `description`: Longer explanation of the command and when to use it.
- `category`: Broad grouping such as `ui`, `api`, `docs`, or `tests`.
- `tags`: Search keywords and project vocabulary.
- `status`: `draft`, `stable`, or `deprecated`.
- `version`: Integer or semantic version for schema evolution.
- `examples`: One or more repository-relative example files.
- `rules`: One or more repository-relative rule documents.
- `validation`: Command-specific validation configuration.
- `memoryHints`: Phrases that should help MCP and agents match the command.
- `owner`: Optional team or subsystem owner.

### Optional Fields

- `aliases`: Alternate ids or names that should resolve to the same command.
- `descriptionLong`: Extended guidance for the command detail view.
- `output`: Default output path or output pattern.
- `outputs`: Multiple generated artifacts when a command creates more than one file.
- `appliesTo`: File types, folders, or component types the command is intended for.
- `ui`: UI hints for form ordering or field grouping.
- `agentHints`: Guidance for agent selection and prompt injection.
- `deprecatedBy`: Reference to a replacement command when the command is retired.

## Canonical YAML Shape

```yaml
id: create-delete-button
name: Create Delete Button
summary: Generate a delete button that follows project conventions.
description: Generate the repo-approved delete control for a model or entity.
category: ui
status: stable
version: 1
tags:
  - ui
  - destructive-action
  - button
aliases:
  - add-delete-button
inputs:
  entity:
    type: string
    required: true
    description: PascalCase entity name, such as `ArtPiece`.
  endpoint:
    type: string
    required: true
    description: Configured endpoint reference, such as `endpoints.artist.delete`.
  componentName:
    type: string
    required: false
    defaultFrom: entity
template: templates/delete-button.tsx.tpl
examples:
  - examples/artist-delete-button.tsx
rules:
  - rules/project-patterns.md#destructive-actions
  - rules/import-order.md
validation:
  requiredChecks:
    - preserve-function-names
    - include-jsdoc
    - use-project-import-order
    - use-configured-endpoints
    - follow-established-component-patterns
    - do-not-invent-new-architecture
memoryHints:
  - destructive action
  - delete support
  - delete button
outputs:
  - path: src/components/{{entity}}DeleteButton.tsx
    template: templates/delete-button.tsx.tpl
```

## Input Variables

Input variables are declared in `inputs`.

### Supported Input Types

- `string`
- `boolean`
- `number`
- `enum`
- `path`
- `identifier`
- `list`
- `json`

### Input Metadata

Each input may define:

- `required`: Whether the user or agent must provide a value.
- `default`: Static fallback value.
- `defaultFrom`: A value derived from another input or project context.
- `description`: UI and agent-facing explanation.
- `pattern`: Optional validation regex.
- `allowedValues`: Explicit values for enum-like inputs.
- `suggestFrom`: Context sources or project data used to suggest a value.

## Templates

Templates are repository-relative text files stored under `.qtcode/templates/`.

### Template Requirements

- Templates must be deterministic.
- Templates must not depend on an AI provider.
- Templates should use project vocabulary and file naming conventions.
- Templates may reference declared inputs and derived values only.

### Template Rendering Model

- `{{inputName}}` inserts a resolved input value.
- `{{project.name}}` and similar context variables may be provided by QTCode.
- Conditional sections may be used for optional fields if the renderer supports them.
- Rendering must fail fast when a required variable is missing.

### Output Modes

- `create`: Write a new file or set of files.
- `replace`: Replace a generated target file in the preview/apply flow.
- `patch`: Generate a diff against an existing file when a command updates existing code.

## Rules

Rules are markdown documents under `.qtcode/rules/` and may include anchors for specific subrules.

### Rule Content

Rules should describe:

- naming conventions
- import ordering
- endpoint usage
- component structure
- JSDoc expectations
- folder and file placement
- project vocabulary
- architectural constraints

### Required Validation Themes

Commands should be able to express validation around:

- preserve existing function names
- include JSDoc
- use project import ordering
- use configured endpoints
- follow established component patterns
- do not invent new architecture when a command exists

## Examples

Examples are repository-relative reference implementations.

### Example Uses

- Show what a valid generated file should look like.
- Help agents infer the expected project pattern.
- Support command detail view explanations.
- Provide training material for command suggestion.

## Discovery

QTCode must discover commands from the active repository.

### Discovery Rules

- Scan `.qtcode/commands/*.yaml` in the active repository.
- Prefer repository-local commands over any global or workspace-level fallback.
- Expose command ids, names, summaries, categories, tags, and statuses in the index.
- Load referenced templates, examples, and rules lazily when needed.

### Search Fields

Commands must be searchable by:

- id
- name
- summary
- tags
- aliases
- category
- memoryHints
- rule references

## Listing

The command list should sort by:

1. exact id match
2. active status
3. tag and summary relevance
4. alphabetical name

The list should show:

- command id
- human-readable name
- summary
- status
- category
- tags
- source path

## Execution

Execution means loading a command, resolving inputs, rendering templates, validating output, and producing a preview or applied change.

### Execution Flow

1. Resolve the command by id or alias.
2. Load the YAML definition.
3. Resolve input values from the UI, CLI, agent context, or defaults.
4. Load the template(s).
5. Render output deterministically.
6. Run validation checks.
7. Show preview and diff.
8. Apply or discard the result after user approval.

### Execution Sources

Commands may be launched from:

- UI command library actions
- `/qtcode` CLI-style entry points
- agent tool calls
- suggested command prompts from context analysis

## Validation

Validation protects repo conventions and prevents accidental drift.

### Validation Types

- Schema validation: required fields, types, and references.
- Reference validation: template, example, and rule paths exist.
- Input validation: required values, enums, patterns, and derived values.
- Output validation: rendered text matches command checks.
- Project validation: import ordering, naming, JSDoc, endpoint references, and architecture constraints.

### Validation Results

Validation should return:

- severity: `info`, `warning`, or `error`
- source: command, template, input, or project rule
- message: human-readable explanation
- location: optional file or line reference
- remediation: optional fix suggestion

### Validation Policy

- Errors block apply.
- Warnings allow preview but require acknowledgement if the command is mutating.
- Info messages are advisory and should still be visible in the UI.

## Creation Workflow

QTCode should help agents create a command when repeated patterns are detected.

### Trigger Conditions

- The same code shape appears across multiple tasks.
- A user or agent repeats the same instructions more than once.
- The pattern depends on project vocabulary or established architecture.
- The command would reduce risk by encoding rules and validation.

### Creation Flow

1. Detect the repeated pattern from session history, repository context, and memory results.
2. Check whether an existing command already covers the task.
3. Draft a new command id, summary, inputs, template, examples, and validation rules.
4. Write the command and related files into `.qtcode/`.
5. Validate the draft against the command schema and repository rules.
6. Present the draft for review in the UI and in the pull request.
7. Store a usage note in `.qtcode/memory/` so Search Memory MCP can learn the new command.

## CLI And Editor Examples

```text
/qtcode list
/qtcode run create-delete-button entity=Artist endpoint=endpoints.artist.delete
/qtcode explain create-delete-button
/qtcode suggest-command
/qtcode validate-command create-delete-button
```

### Example Semantics

- `list` shows discovered commands for the active repository.
- `run` executes a command with explicit inputs.
- `explain` shows why the command exists, what it generates, and what rules it enforces.
- `suggest-command` proposes a command from current context, memory, and repeated patterns.
- `validate-command` checks schema, references, and project constraints without generating code.

## Agent Integration

Agents must treat `.qtcode/commands` as the authoritative source for reusable project patterns.

### Agent Rules

- Prefer an existing command when the current task matches a known project pattern.
- Do not invent new architecture when a command covers the use case.
- Use command examples and rules to preserve project vocabulary.
- Run validation before asking the user to approve code changes.

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

`.qtcode/commands` is the source of truth for executable project patterns. MCP provides retrieval and explanation, not the canonical command definition.

## Compatibility And Portability

- Commands must work from any machine with the repository checked out.
- Commands must not require a specific editor plugin to be useful.
- Commands must not depend on a single model vendor or agent provider.
- Commands should degrade gracefully if MCP services are unavailable.

