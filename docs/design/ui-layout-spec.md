# UI Layout Spec

## Layout

Primary layout:

```text
+----------------------+------------------+--------------+--------------+---+
| Repository Panel     | Agent Panel      | Secondary    | MCP Panel    |Act|
| (full height)        |                  | panel column | (full height)|Bar|
|                      +------------------+              |              |   |
|                      | Terminal Panel   |              |              |   |
|                      | (middle only)    |              |              |   |
+----------------------+------------------+--------------+--------------+---+
```

The right activity bar toggles the AI Agent, Retrieved Context, and Generated Changes panels. Context and Generated Changes share the secondary panel column between the middle workspace and MCP column.

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

## Agent Panels

Purpose:

- Choose agent.
- Manage conversations.
- Show retrieved context.
- Show responses and generated artifacts.
- Approve or reject changes.

Expected surfaces:

- **AI Agent panel** — agent selector, session list, prompt composer, conversation transcript, and status indicators in the middle column above the terminal.
- **Retrieved Context panel** — context result viewer and attach/detach controls in the secondary column.
- **Generated Changes panel** — diff review area and approve/reject controls in the secondary column.
- **Activity bar** — right-edge icon buttons that toggle each panel open or closed.

## Terminal Panel

Purpose:

- Run shells and tools in project context.
- Keep build/test output visible.
- Run agent CLIs where interactive terminal behavior is needed.

Expected sections:

- Tab bar.
- Shell terminal.
- New tab control.
- Profile selector where useful.

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

Panel sizes and activity-bar panel visibility should be persisted per user.
