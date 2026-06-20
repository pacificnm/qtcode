# UI Layout Spec

## Layout

Primary layout:

```text
+----------------------+-----------------------------+
| Repository Panel     | AI Agent Panel              |
|                      |                             |
| Local repositories   | Agent selector              |
| GitHub repositories  | Session list                |
| Branches/tags        | Conversation                |
| Changed files        | Context results             |
| Issues/PRs           | Diff review                 |
+----------------------+-----------------------------+
| Terminal Panel                                     |
| Tabs, shell output, build/test/agent commands      |
+----------------------------------------------------+
```

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

## Agent Panel

Purpose:

- Choose agent.
- Manage conversations.
- Show retrieved context.
- Show responses and generated artifacts.
- Approve or reject changes.

Expected sections:

- Agent selector.
- Session list.
- Prompt composer.
- Conversation transcript.
- Context result viewer.
- Diff review area.
- Status and error indicators.

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

Panel sizes should be persisted per user.
