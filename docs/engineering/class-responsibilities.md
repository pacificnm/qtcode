# Major Classes And Responsibilities

## Application Classes

### `QtCodeApplication`

Owns process-level application setup, metadata, localization hooks, and startup handoff.

### `AppConfigService`

Loads and saves the KDE config file that contains system startup preferences before SQLite is opened. Current fields include restore-last-project behavior, start-maximized behavior, and the default repository help entry path.

### `RepoConfigLoader`

Reads `.qtcode/config.yaml` from an active project root and returns repository-specific preference overrides. Uses a minimal YAML line parser; missing or unreadable files yield an empty config so callers can fall back to system defaults.

### `ApplicationController`

Wires services together, loads app config before storage, initializes storage, restores last state, and coordinates shutdown.

### `MainWindow`

Owns top-level layout, `KActionCollection`-backed actions, menus, toolbar, dock/panel arrangement, and major navigation state. Menu and toolbar actions delegate workflow behavior to panels and services. The File menu includes a modal Settings dialog that edits KDE config-backed system startup preferences. The Help menu includes **Repo Help**, which loads the effective repository help entry for the active project. The left column uses `ProjectNavigationPanel` for repository and file-tree views. The main column uses `WorkspaceTabs` for the permanent AI chat tab and file tabs above the terminal splitter.

### `ProjectNavigationPanel`

Hosts the left-column **Repository** and **Files** views in a compact tab bar. Reuses persisted left-column width from the root horizontal splitter.

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

Discovers available agents, registers adapters, selects active agent per project, creates sessions, and dispatches requests.

### `AgentAdapter`

Common interface implemented by all agents. Exposes identity, capabilities, configuration requirements, request execution, cancellation, and output events.

### `AgentSession`

Represents one conversation or task thread with an agent. Includes project scope, selected agent, messages, context attachments, generated artifacts, and status.

### `AgentRequest`

Normalized input sent to an adapter: user prompt, project metadata, selected context, working directory, and execution mode.

## Context And Memory Classes

### `ContextManager`

Coordinates context collection from repository state, GitHub state, and project memory before an agent request.

### `MemoryService`

Queries MCP/RAG providers and returns normalized context results.

### `McpClient`

Owns the protocol-specific communication with the configured MCP server.

## Terminal Classes

### `TerminalManager`

Creates terminal tabs, assigns project working directories, launches commands, persists terminal metadata, and handles shell profiles.

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
