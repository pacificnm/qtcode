# UI Layout Spec

## Layout

Primary layout:

```text
+----------------------+----------------------+--------------+---+
| Project Navigation   | Main Work Tabs       | Right Panel  |Act|
| [Repository|Files]   | [AI Chat*] [...]     | (one view)   |Bar|
|                      +----------------------+              |   |
|                      | Terminal Panel       |              |   |
|                      | (main)               |              |   |
+----------------------+----------------------+--------------+---+
| Status bar: agent progress, repository refresh, errors, and other operational feedback |
+----------------------------------------------------------------------------------------+
```

The shell exposes exactly three content columns: project navigation on the left, workspace tabs plus terminal in the main column, and one shared right column. The activity bar toggles which right-panel view is active. A compact status bar spans the full window width below all columns.

Right-panel views:

- Agent Sessions
- Retrieved Context
- MCP Servers
- Command Library

Only one right-panel view is visible at a time. Workspace tabs and terminal remain in the main column at all times.

## Left Project Space

Purpose:

- Switch between repository context and project file tree in one left column.
- Select active project and show repository state.
- Provide context for agents.
- Surface GitHub issues and pull requests.

Implementation:

- `ProjectNavigationPanel` hosts a compact tab bar with **Repository** and **Files** views.
- **Repository** view: existing `RepositoryPanel` (local repositories, changed files, GitHub issues and pull requests, detail attach flows). Add repository and refresh status remain in the **File** menu; refresh also runs automatically on project selection and startup.
- **Files** view: `FileTreePanel` rooted at the active project path via `QFileSystemModel`. Activating a text file requests a workspace tab through `WorkspaceTabs::requestOpenFile`. Context menu and **File** menu actions support project-root-scoped new file, new folder, rename, and delete through `FileOperationService`.

Expected repository sections:

- Local repositories.
- GitHub repositories.
- Branches and tags.
- Changed files.
- Issues.
- Pull requests.
- Repository search.

## Main Column

Purpose:

- Run the primary AI chat workflow and terminal sessions in a tabbed work area above the terminal splitter.

Expected surfaces:

- **Workspace tabs** — main work-area tab control owned by `WorkspaceTabs`. The permanent **AI Chat** tab hosts `AgentPanel::conversationPanel()` and cannot be closed. File tabs use KTextEditor via `EditorTab` when opened from the folder tree.
- **Terminal panel** — shell tabs and project-aware command output beneath the workspace tabs.

### Terminal panel

Implementation: `TerminalPanel` hosts a closable `QTabWidget` of QTermWidget instances managed by `TerminalManager`.

Opening behavior:

- On startup, QTCode restores persisted terminal sessions from SQLite when they exist; otherwise it creates one tab when an active project is already selected.
- No terminal tab is created before a repository is active unless the user explicitly requests **File > New Terminal Tab** or clicks the panel **+** button.
- Switching the active repository updates persisted session cwd/title from `.qtcode/config.yaml` (`project.path`, `project.displayName`) and applies the new directory to open tabs without typing a visible `cd` command.
- Collapse/expand is controlled from the panel header; collapsed state and the height to restore on expand persist in SQLite.

User actions:

- **+** or **File > New Terminal Tab** — add another shell tab for the active project.
- Close tab — removes the widget and deletes the SQLite session row.
- Right-click — Copy, Paste, Paste Selection, Clear.

See [terminal integration](../engineering/terminal-integration.md) for cwd resolution, profile storage, and current limitations.

### Editor non-goals

The workspace editing slice is intentionally narrow. QTCode is still a developer cockpit, not a full IDE.

- No Electron, browser UI, Monaco, or VS Code compatibility layer.
- No language server, debugger, extension host, or project-wide refactoring system.
- No multi-root workspace model in the first slice.
- No binary-file editing in workspace tabs.
- No automatic agent edits through the editor surface; review agent file changes through git and the repository changed-files list.

See [KTextEditor workspace spec](../specs/ktexteditor-workspace-spec.md) for the full non-goals list.

### Prompt composer

The AI Chat composer is a two-row stack below the conversation transcript:

1. **Prompt input** — multi-line `QPlainTextEdit` spanning the full composer width, with a minimum height suitable for longer prompts.
2. **Control row** — model selector and execution-mode selector on the left; Send/Stop on the right.

Interaction:

- **Enter** sends when the prompt is non-empty and sending is allowed; **Shift+Enter** inserts a newline.
- While a request is running, the Send control becomes Stop/Cancel for the active session.
- After project memory retrieval completes, the prompt dispatches automatically with all retrieved results attached by default. Users can attach or detach excerpts in **Retrieved Context** to control what is included in the **next** prompt. Attachment choices persist across subsequent retrievals by source key.

Request options:

- Model and execution mode are session-scoped. Selections refresh when the active agent session changes and persist per session in app settings.
- **Cursor** sessions load models from the Cursor CLI (`cursor agent models`) and expose execution modes Agent, Plan, Debug, Multitask, and Ask. Cursor requests pass `--model` and `--mode plan|ask` when applicable.
- **Codex** sessions expose a static model list (Default/config, GPT-5.5, Codex 5.3, Codex 5.2, o3) and execution modes Agent, Plan, Debug, and Ask. Codex requests pass `-m` for model selection and use read-only sandbox for Plan/Ask modes.
- Debug and Multitask appear in the UI for Cursor parity; the Cursor CLI currently accepts only `plan` and `ask` as explicit modes, so those two run in default agent mode until CLI support expands.

## Right Panel

Purpose:

- Show one secondary workflow surface at a time without crowding the main chat column.

Expected surfaces:

- **Agent Sessions** — agent selector, session list, and new session control. The selector preselects the repository default agent from `.qtcode/config.yaml` when opening a project that has no prior selector value; session restore and auto-create behavior are described in [settings spec](../specs/settings-spec.md).
- **Retrieved Context** — deduplicated context result list with checkboxes and attach/detach controls. Shows automatic memory hits plus manual `[source_file]` entries from **Add to Context** actions in the repository changed-files list, folder tree, and editor tab bar. Clicking a file-backed row opens the file in a workspace tab. No inline excerpt preview. See [retrieved context spec](../specs/retrieved-context-spec.md).
- **MCP Servers** — MCP server configuration and memory tooling.
- **Command Library** — repo-native commands, templates, rules, examples, and validation from the active repository. Opened from the right-side action icon and kept manually visible until the user closes it. See [Command Library feature](../features/command-library.md).
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

## Panel Layout Persistence

QTCode separates **configured column widths** from **session collapse state**.

| What | Where it lives | When it applies |
| --- | --- | --- |
| Left column width | KDE config (`leftPanelWidth`, default 240) | Launch, **File > Settings** save, **View > Reset Panel Layout** |
| Right column width | KDE config (`rightPanelWidth`, default 320) | Launch, **File > Settings** save, **View > Reset Panel Layout** |
| Active right panel | SQLite `app.panel_layout` | Launch and normal shutdown restore |
| Right column collapsed | SQLite `app.panel_layout` | Launch and normal shutdown restore |
| Terminal collapsed / restore height | SQLite `app.panel_layout` | Launch and normal shutdown restore |
| Runtime splitter drags | Not persisted | Session only; next launch uses configured widths |

Implementation rules (do not regress these without an explicit design change):

- Apply configured column widths only at launch, settings save, and reset panel layout.
- Persist collapse and selection state to SQLite on normal shutdown; do not save splitter sizes or window geometry.
- Use rubber-band splitter resize (`setOpaqueResize(false)`) on both horizontal and vertical splitters.
- Collapse the right column only through the activity-bar toggle, not by dragging the splitter to zero.
- Default the active right panel to **Agent Sessions** on first run and for unknown legacy values.

See [settings spec](../specs/settings-spec.md) for the full persistence model and schema details.
