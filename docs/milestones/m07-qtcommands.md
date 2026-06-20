# M7: QTCommands

## Goal

Add a repo-native reusable AI command system so QTCode can discover, execute, validate, and suggest project-specific commands that live in the repository.

## User Outcome

When a developer or AI agent needs to repeat a known project pattern, QTCode can load the approved command from `.qtcode/`, render the correct template with project-specific inputs, preview the result, and validate the generated change before it is applied.

## Scope

- Repository-native command discovery under `.qtcode/commands`.
- Command metadata, templates, examples, rules, and memory notes stored in Git.
- Command execution from the UI, CLI-style entry points, and agent workflows.
- Template rendering for reusable project patterns.
- Validation before generated code is applied.
- Command suggestion when repeated patterns are detected.
- Integration with Search Memory MCP and Context MCP.
- Agent-facing command discovery and execution interfaces.

## Engineering Deliverables

- `.qtcode/commands/*.yaml` command definitions.
- `.qtcode/templates/*.tpl` reusable templates.
- `.qtcode/examples/*` command examples.
- `.qtcode/rules/*.md` project rules and style guidance.
- `.qtcode/memory/*.md` command usage notes for memory indexing.
- Command discovery, listing, search, execution, validation, and suggestion flows in QTCode.
- Command Library panel, detail view, run form, preview surface, and validation results view.
- MCP-aware command selection and guidance.

## Exit Criteria

- QTCode can list commands from the active repository.
- QTCode can search commands by id, name, tags, summary, and vocabulary.
- QTCode can render a command with required inputs and project context.
- QTCode can preview generated output before applying it.
- QTCode can validate command schema and output against repo rules.
- QTCode can suggest a relevant command when repeated patterns are detected.
- Command files remain reviewable in pull requests and portable across machines.
- Command discovery works without dependency on a single AI provider.

## Out Of Scope

- Building a general-purpose macro system for arbitrary editor actions.
- Replacing GitHub Copilot, Cursor, Codex, or other provider-specific tooling.
- Syncing commands to a cloud registry.
- Allowing unreviewed command execution from outside the repository.
- Creating a new AI provider or agent runtime.
