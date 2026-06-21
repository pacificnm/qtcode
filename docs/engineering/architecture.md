# Engineering Architecture

## Layers

```text
UI Layer
  Qt/KDE widgets, models, actions, panels

Application Layer
  workflow coordination, project state, sessions, command routing

Integration Layer
  agent adapters, Git, GitHub CLI, MCP/RAG, terminal processes

Storage Layer
  SQLite repositories, migrations, settings persistence
```

## UI Layer

The UI should stay thin. Widgets present data models, emit user intentions, and render state. They should not parse Git output, compose agent prompts, or talk directly to MCP servers.

Primary widgets:

- `MainWindow`
- `RepositoryPanel`
- `AgentPanel`
- `TerminalPanel`
- `ContextResultsView`
- `DiffReviewView`

## Application Layer

Application services own workflows and cross-feature coordination.

- `ApplicationController`: startup and feature wiring.
- `AppConfigService`: startup-time KDE config file for system preferences that must load before SQLite.
- `RepoConfigLoader`: on-demand read of `.qtcode/config.yaml` repository overrides for the active project.
- `ProjectManager`: active project and repository lifecycle.
- `AgentManager`: agent discovery, selection, and session orchestration.
- `ContextManager`: context gathering before agent calls.
- `TerminalManager`: terminal tab lifecycle.
- `SettingsService`: app and project settings.

## Integration Layer

Integration services wrap external tools and unstable contracts.

- `GitService`: local repository inspection and Git operations.
- `GitHubService`: GitHub workflows through `gh`.
- `MemoryService`: MCP/RAG memory access.
- `AgentAdapter`: provider-specific agent execution.
- `ProcessRunner`: safe wrapper for child process execution where a terminal is not required.

## Storage Layer

SQLite persistence is accessed through explicit repositories.

- `ProjectRepository`
- `AgentSessionRepository`
- `SettingsRepository`
- `TerminalSessionRepository`
- `McpConfigRepository`
- `GitHubCacheRepository`

Do not let arbitrary widgets issue SQL.

## Threading And Responsiveness

- UI work remains on the main thread.
- Git refresh, GitHub calls, MCP calls, and agent process monitoring must not block the UI.
- Use Qt signals for progress, completion, and errors.
- Prefer cancellable jobs for long operations.

## Error Philosophy

Most external dependencies are optional or user-managed. Missing `gh`, missing agents, an offline MCP server, or a broken repository should produce visible degraded states rather than application failure.
