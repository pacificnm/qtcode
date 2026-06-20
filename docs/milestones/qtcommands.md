# QTCommands Milestone

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

## Non-Goals

- Building a general-purpose macro system for arbitrary editor actions.
- Replacing GitHub Copilot, Cursor, Codex, or other provider-specific tooling.
- Syncing commands to a cloud registry.
- Allowing unreviewed command execution from outside the repository.
- Creating a new AI provider or agent runtime.

## Deliverables

- `.qtcode/commands/*.yaml` command definitions.
- `.qtcode/templates/*.tpl` reusable templates.
- `.qtcode/examples/*` command examples.
- `.qtcode/rules/*.md` project rules and style guidance.
- `.qtcode/memory/*.md` command usage notes for memory indexing.
- Command discovery, listing, search, execution, validation, and suggestion flows in QTCode.
- MCP-aware command selection and guidance.

## Phases

### Phase 1: Command Discovery

- Scan the repository for `.qtcode/commands/*.yaml`.
- Build an in-memory command index with name, summary, tags, status, and path metadata.
- Expose list and search operations in the UI and agent flows.
- Resolve command references to templates, examples, and rules.

### Phase 2: Command Execution

- Parse command inputs from UI forms, CLI-style invocations, or agent requests.
- Resolve defaults, required values, and project context bindings.
- Load the command template and render it with the resolved inputs.
- Support read-only preview before changes are applied.

### Phase 3: Template Rendering

- Support reusable placeholders for project vocabulary, filenames, endpoints, component names, and function names.
- Render one command into one or more output artifacts.
- Keep the template engine deterministic and provider-neutral.

### Phase 4: Validation

- Validate command schema before execution.
- Validate rendered output against command-specific and project-specific rules.
- Check that existing function names, import ordering, JSDoc, and configured endpoints are preserved where required.
- Block or warn on output that violates approved project conventions.

### Phase 5: Agent Integration

- Suggest commands when the user request matches an approved repo pattern.
- Let agents query commands by intent, tags, entity type, or task type.
- Use command metadata in prompt construction so the agent prefers established patterns over invention.

### Phase 6: MCP Integration

- Use Search Memory MCP to find long-term memory about when a command should be used.
- Use Context MCP to attach the current repository, file, task, and architecture context.
- Keep `.qtcode/commands` as the source of truth for executable patterns.
- Let MCP tools surface command metadata and usage examples.

## Acceptance Criteria

- QTCode can list commands from the active repository.
- QTCode can search commands by id, name, tags, summary, and vocabulary.
- QTCode can render a command with required inputs and project context.
- QTCode can preview generated output before applying it.
- QTCode can validate command schema and output against repo rules.
- QTCode can suggest a relevant command when repeated patterns are detected.
- Command files remain reviewable in pull requests and portable across machines.
- Command discovery works without dependency on a single AI provider.

## Risks

- Overlapping commands could create ambiguity unless metadata and tags are disciplined.
- Template drift can occur if examples and rules are not updated with the command.
- Validation must stay strict enough to protect conventions without blocking legitimate evolution.
- Agent suggestion quality depends on both memory retrieval and context quality.
- The command schema should avoid becoming a second, unmaintainable programming language.

