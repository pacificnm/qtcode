# UI Layout Spec

## Layout

Primary layout:

```text
+----------------------+----------------------+--------------+---+
| Repository Panel     | Main Work Tabs       | Right Panel  |Act|
| (left)               | [AI Chat*] [...]     | (one view)   |Bar|
|                      +----------------------+              |   |
|                      | Terminal Panel       |              |   |
|                      | (main)               |              |   |
+----------------------+----------------------+--------------+---+
| Status bar: agent progress, repository refresh, errors, and other operational feedback |
+----------------------------------------------------------------------------------------+
```

The shell exposes exactly three content columns: repository on the left, workspace tabs plus terminal in the main column, and one shared right column. The activity bar toggles which right-panel view is active. A compact status bar spans the full window width below all columns.

Right-panel views:

- Agent Sessions
- Retrieved Context
- Generated Changes
- MCP Servers

Only one right-panel view is visible at a time. Workspace tabs and terminal remain in the main column at all times.

## Repository Panel

Purpose:

- Select active project.
- Show repository state.
- Provide context for agents.
- Surface GitHub issues and pull requests.

Expected sections:

- Local repositories.
- GitHub repositories.
- Branches and tags.
- Changed files.
- Commit history.
- Issues.
- Pull requests.
- Repository search.

## Main Column

Purpose:

- Run the primary AI chat workflow and terminal sessions in a tabbed work area above the terminal splitter.

Expected surfaces:

- **Workspace tabs** — main work-area tab control owned by `WorkspaceTabs`. The permanent **AI Chat** tab hosts `AgentPanel::conversationPanel()` and cannot be closed. Future editor tabs open beside it.
- **Terminal panel** — shell tabs and project-aware command output beneath the workspace tabs.

### Prompt composer

- Multi-line input (`QPlainTextEdit`) with a minimum height suitable for longer prompts.
- Send control on the same row as the input, aligned to the bottom-right of the composer.
- **Enter** sends when the prompt is non-empty and sending is allowed; **Shift+Enter** inserts a newline.
- After project memory retrieval completes, the prompt dispatches automatically with all retrieved results attached by default. Users can attach or detach excerpts in **Retrieved Context** to control what is included in later prompts.

## Right Panel

Purpose:

- Show one secondary workflow surface at a time without crowding the main chat column.

Expected surfaces:

- **Agent Sessions** — agent selector, session list, and new session control.
- **Retrieved Context** — context result viewer and attach/detach controls.
- **Generated Changes** — diff review area and approve/reject controls.
- **MCP Servers** — MCP server configuration and memory tooling.
- **Activity bar** — right-edge icon buttons that switch the active right-panel view or hide the right column.

## Status Bar

Purpose:

- Provide one application-wide place for short-lived operational feedback.
- Keep agent, repository, memory, and startup messages visible regardless of which panel has focus.

Expected message categories:

- Agent session progress, completion, cancellation, and failures.
- Memory retrieval and prompt dispatch outcomes.
- Repository refresh started, completed, and failed states.
- Capability and service availability warnings.
- Transient success and informational confirmations.

Implementation notes:

- Panels and services publish status through `StatusService` on `ApplicationController`; widgets do not reach into `MainWindow` directly.
- Panel-specific empty-state copy inside list sections stays local; the global bar is not used for static placeholders.
- Info messages auto-clear after a short delay; errors and in-progress operations remain until replaced or cleared.

## Visual Direction

- Native KDE feel.
- Dense but readable information layout.
- Avoid marketing-style hero areas.
- Prefer predictable panels, splitters, tabs, lists, and toolbars.
- Keep icons and text compact.
- Make empty states useful but quiet.

## Responsive Behavior

Although QTCode is desktop-first, the layout should tolerate:

- small laptop screens
- large monitors
- vertical resizing
- hidden/collapsed panels

Panel sizes, active right-panel selection, and right-column visibility should be persisted per user.
