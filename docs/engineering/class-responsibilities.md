# Major Classes And Responsibilities

## Application Classes

### `QtCodeApplication`

Owns process-level application setup, metadata, localization hooks, and startup handoff.

### `AppConfigService`

Loads and saves the KDE config file that contains system startup preferences before SQLite is opened. Current fields include restore-last-project behavior, start-maximized behavior, default left and right column widths, and the system fallback repository help entry path (`repoHelpPath`).

### `RepoConfigLoader`

Reads `.qtcode/config.yaml` from an active project root and returns repository-specific preference overrides. Uses a minimal YAML line parser; missing or unreadable files yield an empty config so callers can fall back to system defaults.

### `RepoConfigWriter`

Writes merged repository preferences to `.qtcode/config.yaml` for the active project. Used by the Settings dialog **Repository** group and preserves both help and agent fields when saving. Uses atomic writes through `QSaveFile`.

### `ApplicationController`

Wires services together, loads app config before storage, initializes storage, registers agent adapters, restores agent sessions and project state, schedules background CLI capability detection, and coordinates shutdown.

### `MainWindow`

Owns top-level layout, `KActionCollection`-backed actions, menus, toolbar, dock/panel arrangement, and major navigation state. Menu and toolbar actions delegate workflow behavior to panels and services. The File menu includes a modal Settings dialog with **Global** and **Repository** groups: global startup preferences and column widths are saved through `AppConfigService`; active-repository default agent and help entry overrides are saved through `RepoConfigWriter`. The Help menu includes **Repo Help**, which loads the effective repository help entry for the active project. The left column uses `ProjectNavigationPanel` for repository and file-tree views. The main column uses `WorkspaceTabs` for the permanent AI chat tab and file tabs above the terminal splitter. Applies configured column widths from `AppConfigService` on launch, after settings save, and when resetting panel layout; persists collapse/selection state to SQLite on shutdown but does not persist runtime splitter sizes.

### `ProjectNavigationPanel`

Hosts the left-column **Repository** and **Files** views in a compact tab bar. Column width comes from the configured default in `AppConfigService`, not from persisted splitter state.

### `FileTreePanel`

Displays the active project's filesystem tree, emits `fileOpenRequested` when the user activates an openable text file, and provides project-root-scoped create, rename, and delete operations through `FileOperationService`.

### `FileOperationService`

Validates paths against the active repository root and performs create, rename, and delete filesystem mutations for the Files view.

### `WorkspaceTabs`

Owns the main work-area tab widget. Hosts the permanent, non-closable AI chat tab and closable KTextEditor-backed file tabs opened from the folder tree. Resolves the effective repository help entry path by merging `AppConfigService` defaults with `.qtcode/config.yaml` overrides through `RepoConfigLoader`. Handles deduplicated open requests, dirty-tab close prompts, and closing editor tabs on project switch.

### `EditorTab`

Wraps one `KTextEditor::Document` and `KTextEditor::View` pair for a single local file path. Surfaces load/save failures through `StatusService` and prompts before closing dirty documents.

## Project And Repository Classes

### `ProjectManager`

Tracks known projects, active project, recent repositories, and project-scoped settings.

### `RepositoryListModel`

Qt model for local and GitHub repository lists.

### `GitService`

Provides repository status, current branch, tags, commit summaries, diffs, and safe Git operation entry points.

## Agent Classes

### `AgentManager`

Discovers available agents, registers built-in adapters at startup, restores persisted sessions from SQLite, tracks the active session per project, creates sessions, and dispatches requests.

### `AgentPanel`

Hosts the AI chat conversation surface, prompt composer, and right-panel agent/session controls. On project switch or settings save, refreshes the agent selector using the repository default from `.qtcode/config.yaml` when no prior selection exists, restores the last active session for the project, and creates a default session when needed.

### `AgentAdapter`

Common interface implemented by all agents. Exposes identity, capabilities, configuration requirements, request execution, cancellation, and output events.

### `AgentSession`

Represents one conversation or task thread with an agent. Includes project scope, selected agent, messages, context attachments, generated artifacts, and status.

### `AgentRequest`

Normalized input sent to an adapter: user prompt, project metadata, selected context, working directory, and execution mode.

## Context And Memory Classes

### `ContextManager`

Coordinates context collection from repository state, GitHub state, and project memory before an agent request. Dedupes memory results with `memory::dedupeContextResultsBySource()` and builds excerpt strings for dispatch.

### `ContextResult`

Normalized memory/file context record and helpers: source-type labels, dedupe-by-source, MCP output parsing, and manual file loading (`contextResultFromFilePath`).

### `ContextResultsView`

Right-panel list UI for retrieved context. Shows deduplicated results with checkboxes and attach/detach controls, preserves attachment by source key across retrievals, supports **Add to Context** file entries, opens file-backed rows in workspace tabs, and exposes `attachedResults()` / `attachedContextExcerpts()` to `AgentPanel`.

### `MemoryService`

Queries MCP/RAG providers and returns normalized context results.

### `McpClient`

Owns the protocol-specific communication with the configured MCP server.

## Terminal Classes

### `TerminalManager`

Creates terminal tabs, resolves project working directories from SQLite and `.qtcode/config.yaml`, starts QTermWidget shells, persists terminal metadata, syncs sessions on project switch, and handles shell profiles.

### `TerminalPanel`

Hosts terminal tabs, restores or creates initial sessions on startup, wires project-change and session-change signals, and exposes new-tab and collapse controls.

### `TerminalSession`

Stores one terminal tab's logical metadata, including project, shell, working directory, title, and optional command profile.

## GitHub Classes

### `GitHubService`

Coordinates GitHub workflows for repositories, issues, and pull requests.

### `GhCliClient`

Executes `gh` commands, requests JSON output, parses results, and maps errors to user-facing states.

## Storage Classes

### `StorageService`

Owns database path, connection lifecycle, transactions, and migration execution.

### `MigrationRunner`

Applies schema migrations in order and records migration state.

### Repository Classes

Each storage repository owns SQL for one aggregate: projects, settings, sessions, terminals, MCP configs, GitHub cache.
