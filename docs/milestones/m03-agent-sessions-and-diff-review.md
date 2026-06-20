# M3: Agent Sessions And Diff Review

## Goal

Provide the first end-to-end AI-assisted workflow.

## User Outcome

The user can start an agent session for a repository, send a prompt, view the response, inspect generated diffs, and approve or reject changes.

## Scope

- `AgentManager`
- `AgentAdapter` interface.
- One built-in CLI adapter, preferably Codex first.
- Agent selector.
- Session list and conversation view.
- Persist sessions and messages.
- Capture response stream and process status.
- Basic diff viewer.
- Approve/reject flow for generated changes.

## Engineering Deliverables

- Agent process wrapper.
- Agent session model.
- Message persistence.
- Diff artifact model.
- UI for agent capabilities and errors.

## Exit Criteria

- Agent session can be created for an active project.
- Prompt and response are saved.
- Agent failure state is visible and recoverable.
- Diff approval workflow cannot silently mutate files without user confirmation.

## Out Of Scope

- Full multi-agent orchestration.
- Dynamic plugins.
- Deep MCP context retrieval.
