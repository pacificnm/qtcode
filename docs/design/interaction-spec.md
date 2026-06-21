# Interaction Spec

## Active Project Selection

When a user selects a repository:

- Repository panel marks it active.
- Git status refreshes.
- Terminal default working directory changes.
- Agent panel restores the last active session for that project or creates one with the selected agent.
- Agent selector preselects the repository default from `.qtcode/config.yaml` when no prior selector value exists for that project.
- GitHub remote resolution runs if available.
- Context retrieval scope updates.

## Agent Prompt Flow

1. User selects active repository.
2. User selects agent.
3. User writes prompt.
4. User may attach manual file context with **Add to Context** from the repository changed-files list, folder tree, or an editor tab.
5. User sends the prompt.
6. QTCode retrieves relevant project memory asynchronously when enabled.
7. Retrieved results appear in the **Retrieved Context** panel (deduplicated by source). All results attach by default unless the user previously detached that source.
8. The prompt dispatches with checked excerpts. Users can adjust attach/detach before the next send.
9. Response streams into the conversation.
10. User reviews agent file changes through git status and the repository changed-files list (open in workspace tabs as needed).

See [retrieved context spec](../specs/retrieved-context-spec.md) for panel behavior and recent fixes.

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

## GitHub Issue Branch Flow

1. User right-clicks an issue in the repository panel issues list.
2. QTCode resolves whether an issue branch already exists from GitHub-linked branches and local/remote names matching `{number}-*`.
3. When no branch exists and both Git and authenticated `gh` are available, **Create Branch** opens `CreateIssueBranchDialog` with a suggested `{number}-{slug}` name and a base-branch picker defaulting to `main` or `master`.
4. Create runs `gh issue develop`, fetches the branch from `origin`, and offers **Change Branch** in the dialog to check it out immediately.
5. When a branch already exists, **Change Branch** checks it out through `GitService` and refreshes repository status.
6. **Add to Context** and **Copy Link** remain available regardless of branch state.

## Agent Change Review Flow

1. Agent modifies files through its own tooling.
2. QTCode refreshes repository git status.
3. User inspects changed files in the repository panel and opens them in workspace tabs.
4. User validates changes with git diff, terminal commands, or external review workflows.

QTCode does not provide a dedicated in-app diff approval panel. Session history retains conversation context; file review is repository-native.

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
