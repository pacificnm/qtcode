# UI Layout Spec

## Layout

Primary layout:

```text
+----------------------+----------------------+--------------+---+
| Repository Panel     | AI Chat Panel        | Right Panel  |Act|
| (left)               |                      | (one view)   |Bar|
|                      +----------------------+              |   |
|                      | Terminal Panel       |              |   |
|                      | (main)               |              |   |
+----------------------+----------------------+--------------+---+
```

The shell exposes exactly three content columns: repository on the left, AI chat plus terminal in the main column, and one shared right column. The activity bar toggles which right-panel view is active.

Right-panel views:

- Agent Sessions
- Retrieved Context
- Generated Changes
- MCP Servers

Only one right-panel view is visible at a time. AI chat and terminal remain in the main column at all times.

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

- Run the primary AI chat workflow and terminal sessions.

Expected surfaces:

- **AI Chat panel** — conversation transcript, multi-line prompt composer with inline send control, status indicators, and request controls.
- **Terminal panel** — shell tabs and project-aware command output beneath the chat panel.

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
