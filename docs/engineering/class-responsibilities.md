# Major Classes And Responsibilities

## Application Classes

### `QtCodeApplication`

Owns process-level application setup, metadata, localization hooks, and startup handoff.

### `ApplicationController`

Wires services together, initializes storage, loads settings, restores last state, and coordinates shutdown.

### `MainWindow`

Owns top-level layout, `KActionCollection`-backed actions, menus, toolbar, dock/panel arrangement, and major navigation state. Menu and toolbar actions delegate workflow behavior to panels and services.

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
