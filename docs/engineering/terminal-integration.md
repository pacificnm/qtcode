# Terminal Integration Architecture

## Goals

- Make terminal workflows first-class.
- Support multiple tabs.
- Make terminals project-aware.
- Run Git, GitHub CLI, agents, build tools, and tests.
- Persist useful terminal metadata.

## Current State (M6)

Implemented today:

- QTermWidget-backed shell tabs in `TerminalPanel` beneath the workspace tabs.
- One or more terminal tabs per launch, restored from SQLite when metadata exists.
- Automatic first tab when an active project is present and no sessions were restored.
- Project-aware working directory and tab title derived from the active repository.
- Per-repository overrides for display name and working directory path in `.qtcode/config.yaml`.
- Global and per-project terminal profiles in SQLite (`TerminalProfileStore`), including shell path and working-directory mode.
- Konsole-like default color scheme (`WhiteOnBlack` with fallbacks).
- Custom context menu (Copy, Paste, Paste Selection, Clear); Konsole URL/open-link actions are not exposed.
- Collapsible terminal panel with persisted height and collapse state.
- Unit coverage for working-directory resolution in `tests/unit/terminal/TerminalManagerWorkingDirectoryTest.cpp`.

Not implemented yet:

- Command injection from build profiles or other features (`Run Build Command` flow is planned only).
- Per-project color scheme override.
- True process persistence across relaunch (sessions restore metadata and start fresh shells).
- Terminal profile editing in the Settings dialog UI (profiles are stored in SQLite but not yet exposed in **File > Settings**).

## QTermWidget Usage

Each visible terminal tab owns a QTermWidget instance. `TerminalManager` owns logical session state and creates widgets through a controlled factory.

`TerminalManager::configureWidget()` applies the default Konsole-like appearance to every new and restored tab. The preferred bundled scheme is `WhiteOnBlack` (white foreground on a black background). If that scheme is missing from the QTermWidget install, QTCode falls back to `Linux`, then `GreenOnBlack`, and finally the QTermWidget built-in default while logging a warning.

## How Terminals Open

Terminals are created only through `TerminalManager::createTerminal()` or `TerminalManager::restoreTerminal()`. `TerminalPanel` owns the tab UI and decides when to call each path.

### Startup

```text
ApplicationController initializes TerminalManager
  -> TerminalManager::restoreState() loads global profile + SQLite sessions
MainWindow constructs TerminalPanel
  -> TerminalPanel::restoreOrCreateInitialTabs()
       if persisted sessions exist:
         restore each session as a tab (title suffix "(restored)")
       else if ProjectManager has an active project:
         create one tab for the active project
       else:
         show empty tab bar (no tab until a project is active)
  -> if active project already set:
       onActiveProjectChanged() syncs session metadata and cwd
```

QTCode does not create a `$HOME` tab before a repository is active. The first shell appears only after project restore/selection or when the user explicitly requests a new tab while a project is active.

### New tab

The user can open another shell tab from:

- the **+** button in the terminal panel header
- **File > New Terminal Tab** (`file_new_terminal_tab`)

Both call `TerminalPanel::addTerminalTab()`, which creates a session for the current active project id (may be empty when no project is selected).

### Active project change

```text
ProjectManager emits activeProjectChanged
  -> TerminalPanel::onActiveProjectChanged()
       TerminalManager::syncSessionsToActiveProject()
         re-resolves cwd + title from repo config / SQLite project record
         updates persisted session rows when values changed
       if no tabs exist:
         create one tab for the active project
       else:
         syncTabsFromSessions()
           update tab titles
           apply cwd to each live widget when needed
```

### Settings save

When **File > Settings** saves repository terminal fields (`project.displayName`, `project.path`), `MainWindow` calls `TerminalManager::syncSessionsToActiveProject()` for the active project so tab titles and working directories pick up the new values without restarting the app.

## Working Directory Resolution

Terminal cwd is resolved in layers. The effective directory is always applied through the PTY (`QTermWidget::setWorkingDirectory()` before `startShellProgram()`), not by typing a visible `cd` command into the shell.

### Project-backed sessions

For sessions tied to an active project, `TerminalManager::resolveProjectWorkingDirectory()`:

1. Loads the `ProjectRecord` from SQLite.
2. Reads `.qtcode/config.yaml` via `RepoConfigLoader`.
3. Resolves path with `effectiveProjectPath()`:
   - use `project.path` from repo config when set (absolute or relative to repository root)
   - otherwise use the SQLite project `rootPath`
4. Canonicalizes the directory and validates that it exists.

Tab title uses `effectiveProjectDisplayName()`:

- use `project.displayName` from repo config when set
- otherwise use the SQLite project name

These repo fields are editable in **File > Settings > Repository** and documented in [settings spec](../specs/settings-spec.md).

### Profile working-directory mode

Each session stores a profile snapshot (`profileJson`) captured at creation time. `TerminalManager::resolveSessionWorkingDirectory()` re-resolves cwd on restore and project sync:

| Mode | Resolved cwd |
| --- | --- |
| `ProjectRoot` (default) | `effectiveProjectPath()` for the session's project |
| `Home` | user home directory |
| `CustomPath` | configured absolute directory on the profile |

Global and per-project profiles merge through `TerminalManager::effectiveProfile()`. Project shell overrides replace the global shell when set.

### Applying cwd to a live tab

`TerminalManager::applySessionToWidget()` compares the widget's tracked `qtcode_working_directory` property with the target session cwd:

- if they match, no work is done
- otherwise `restartShellInDirectory()` sets the PTY directory, sends `SIGHUP` to the running shell when one exists, and starts a fresh shell program

This avoids Konsole's `changeDir()` helper, which injects visible `cd /path` text into the scrollback.

## Session Metadata

SQLite stores one row per tab through `TerminalSessionRepository`:

- title
- project ID
- working directory
- shell path
- profile snapshot JSON
- creation and update timestamps

MVP persistence restores metadata and creates fresh shell processes. True process persistence can be researched later.

## Terminal Launch Flow

```text
TerminalPanel requests a session
  -> TerminalManager::buildSessionForProject()
       effectiveProfile(projectId)
       resolve shell path
       resolve working directory + title
  -> TerminalManager::configureWidget()
       apply color scheme + context menu
       setShellProgram + setWorkingDirectory
       startShellProgram()
       record qtcode_working_directory on widget
  -> persist TerminalSession row
  -> TerminalPanel adds QTermWidget as tab
```

## Command Launch Flow

Planned for later milestones:

```text
Feature requests command
  -> TerminalManager selects or creates terminal
  -> command is inserted or executed
  -> terminal receives focus
```

For commands that should not be interactive, use `ProcessRunner` instead of the terminal.

## Context Menu

Each terminal widget uses a QTCode-installed context menu:

- Copy (enabled when text is selected; refreshed at menu open so right-click selection works)
- Paste
- Paste Selection
- Clear

Konsole/QTermWidget default actions that open URLs or external locations are not shown.

## Profiles

Project terminal profiles should eventually include:

- shell path
- environment variables
- startup command
- title pattern
- working directory behavior
- color scheme override

Today:

- shell path and working-directory mode persist in SQLite through `TerminalProfileStore`
- the global default color scheme is applied centrally in `TerminalManager::configureWidget()` and is not persisted per project
- profile fields are not yet editable through the Settings dialog

## Safety

Commands that mutate repositories should be transparent. The terminal is allowed to show exact commands before execution when the action is high risk.

## Related Docs

- [UI layout spec](../design/ui-layout-spec.md) — terminal panel placement and user actions
- [Settings spec](../specs/settings-spec.md) — repository `project.displayName` and `project.path`
- [Communication flows](communication-flows.md) — startup and project-switch sequences
- [ADR 0007: QTermWidget terminal integration](../adrs/0007-qtermwidget-terminal-integration.md)
