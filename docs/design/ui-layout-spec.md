# UI Layout Spec

## Layout

Primary layout:

```text
+------+----------+----------------------+--------------+---+
| Repo | Agent    | AI Chat Panel        | Right Panel  |Act|
|      | Sessions |                      | (one view)   |Bar|
|      | (toggle) +----------------------+              |   |
|      |          | Terminal Panel       |              |   |
|      |          | (main)               |              |   |
+------+----------+----------------------+--------------+---+
```

The shell exposes repository on the left, a toggleable agent sessions column, AI chat plus terminal in the main column, and one shared right column. The activity bar toggles the agent sessions column and which right-panel view is active.

Agent sessions column:

- Agent selector
- Session list
- New session control

Main column:

- Conversation transcript
- Prompt composer and request controls
- Terminal tabs beneath the chat surface

Right-panel views:

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

## Agent Sessions Panel

Purpose:

- Choose the active agent.
- Switch between saved conversations for the active project.

Expected sections:

- Agent selector.
- Session list.
- New session control.

## Main Column

Purpose:

- Run the primary AI chat workflow and terminal sessions.

Expected surfaces:

- **AI Chat panel** — conversation transcript, prompt composer, status indicators, and request controls.
- **Terminal panel** — shell tabs and project-aware command output beneath the chat panel.

## Right Panel

Purpose:

- Show one secondary workflow surface at a time without crowding the main chat column.

Expected surfaces:

- **Retrieved Context** — context result viewer and attach/detach controls.
- **Generated Changes** — diff review area and approve/reject controls.
- **MCP Servers** — MCP server configuration and memory tooling.
- **Activity bar** — right-edge icon buttons that toggle agent sessions, switch the active right-panel view, or hide the right column.

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

Panel sizes, agent sessions visibility, active right-panel selection, and right-column visibility should be persisted per user.
