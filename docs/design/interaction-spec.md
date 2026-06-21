# Interaction Spec

## Active Project Selection

When a user selects a repository:

- Repository panel marks it active.
- Git status refreshes.
- Terminal default working directory changes.
- Agent preference loads.
- GitHub remote resolution runs if available.
- Context retrieval scope updates.

## Agent Prompt Flow

1. User selects active repository.
2. User selects agent.
3. User writes prompt.
4. QTCode retrieves relevant context when enabled.
5. QTCode retrieves relevant context when enabled.
6. User can open the Retrieved Context panel from the activity bar to inspect attached context.
7. User sends request.
8. Response streams into conversation.
9. Diffs or artifacts appear in the Generated Changes panel.
10. User approves or rejects generated changes.

## Terminal Flow

1. User opens a terminal tab.
2. QTCode starts the configured shell in the active project root.
3. User runs commands directly or launches commands from UI actions.
4. Terminal tab metadata is persisted.
5. On restart, QTCode restores terminal tabs as fresh sessions with saved metadata.

## GitHub Context Flow

1. QTCode detects GitHub remote.
2. QTCode checks `gh` availability and authentication.
3. Issues and pull requests load as repository context.
4. User selects an issue or pull request.
5. Selected content can be attached to an agent request.

## Diff Approval Flow

1. Agent produces change artifact.
2. QTCode shows diff before applying or accepting changes.
3. User approves or rejects.
4. Approved changes update repository state.
5. Rejected changes remain recorded in the session but are not applied.

## Keyboard And Actions

Expected early shortcuts:

- open command palette or quick action search
- add repository
- focus repository panel
- focus agent prompt
- focus terminal
- new terminal tab
- refresh repository status

Shortcuts should follow KDE conventions where possible.

## Error States

External dependency errors should be actionable:

- agent CLI missing
- `gh` missing
- GitHub authentication missing
- MCP server unavailable
- repository path invalid
- terminal shell missing

Each state should explain the missing capability and keep unrelated features usable.
