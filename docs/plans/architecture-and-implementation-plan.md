# QTCode Architecture And Implementation Plan

## Vision

QTCode is a fast native KDE/Linux application for developers whose primary workflows are AI agents, terminals, GitHub, project context, memory retrieval, build/test execution, and Git operations.

It should feel closer to a developer cockpit than a general IDE. The terminal and AI agent panel are first-class surfaces. File editing is optional and should stay lightweight.

## Product Principles

- Native Qt/KDE application, no Electron or Chromium dependency.
- Terminal-first and AI-first workflows.
- Context-first architecture: repository, memory, and GitHub state should enrich agent sessions.
- Pluggable AI agents with no tight dependency on one provider.
- Fast startup and low idle memory.
- Editor features stay secondary: focused workspace editing, diff review, and lightweight browsing rather than a full IDE.
- Prefer proven local tools: QTermWidget, libgit2, Git CLI, GitHub CLI, SQLite, KDE Frameworks.

## High-Level Architecture

```text
QTCodeApp
  |
  +-- MainWindow
  |     +-- RepositoryPanel
  |     +-- AgentPanel
  |     +-- TerminalPanel
  |     +-- OptionalPreviewOrDiffPanel
  |
  +-- ApplicationCore
        +-- ProjectManager
        +-- AgentManager
        +-- ContextManager
        +-- TerminalManager
        +-- GitService
        +-- GitHubService
        +-- MemoryService
        +-- SettingsService
        +-- StorageService
```

The UI layer presents state and user actions. Application services own workflow orchestration. Integration layers wrap external systems. Storage uses SQLite for local app state and metadata.

## Module Boundaries

- `app`: process startup, command-line handling, KDE application metadata.
- `ui`: widgets, models, actions, menus, panels, dialogs.
- `core`: application services and workflow orchestration.
- `agents`: common agent interface and provider adapters.
- `terminal`: QTermWidget integration and terminal session management.
- `git`: repository state, branch data, status, commits, diffs.
- `github`: GitHub repository, issue, and pull request workflows.
- `memory`: MCP and RAG integration.
- `storage`: SQLite schema, migrations, repositories.
- `settings`: user and project configuration.

## Complete Directory Structure

The recommended source layout is captured in `engineering/project-structure.md`.

## CMake Organization

The build should use a root CMake project with libraries per subsystem and one GUI executable. Keep compile units grouped by responsibility, use Qt automatic MOC/UIC/RCC, and avoid circular dependencies between feature libraries.

Detailed CMake guidance is in `engineering/cmake-organization.md`.

## Major Classes

Core classes:

- `ApplicationController`
- `MainWindow`
- `ProjectManager`
- `RepositoryModel`
- `AgentManager`
- `AgentAdapter`
- `AgentSession`
- `ContextManager`
- `MemoryService`
- `TerminalManager`
- `GitService`
- `GitHubService`
- `StorageService`
- `SettingsService`

Detailed responsibilities are in `engineering/class-responsibilities.md`.

## UI Panel Architecture

```text
+----------------------+-----------------------------+
| RepositoryPanel      | AgentPanel                  |
|                      |                             |
| Projects             | Agent selector              |
| Local repos          | Sessions                    |
| GitHub repos         | Conversation                |
| Branches/tags        | Retrieved context           |
| Changes/commits      | Diffs and approvals         |
| Issues/PRs           |                             |
+----------------------+-----------------------------+
| TerminalPanel                                      |
| Multiple tabs, build output, agent execution       |
+----------------------------------------------------+
```

Detailed UI specs are in `design/ui-layout-spec.md` and `design/interaction-spec.md`.

## Database Design

SQLite stores local app state only:

- settings
- project list
- repositories
- recent workspaces
- agent configurations
- sessions
- chat messages
- context retrieval metadata
- terminal session metadata
- MCP server configurations

The full schema proposal is in `engineering/database-schema.md`.

## Agent Plugin Architecture

Agents are exposed through a stable `AgentAdapter` interface. CLI agents use process wrappers. API agents use network clients. MCP-enabled agents can receive context from `MemoryService`.

Agent adapters should be loadable from built-in modules first, with future support for dynamic plugins.

Detailed agent design is in `engineering/plugin-agent-architecture.md`.

## MCP And Memory Architecture

QTCode should call the existing Python MCP/RAG system instead of reimplementing memory storage. `MemoryService` provides a common query and result interface. `ContextManager` decides what repository and user intent should be sent to memory search before an agent request.

Detailed memory design is in `engineering/mcp-integration.md`.

## Git And GitHub Architecture

Git should use libgit2 for fast local repository reads where practical and the Git CLI for operations where CLI behavior is expected or safer.

GitHub MVP should use `gh` because it preserves the user's existing authentication and avoids early OAuth complexity. REST API integration can come later when richer background sync is needed.

Detailed integration design is in `engineering/git-github-integration.md`.

## Terminal Architecture

QTermWidget powers terminal tabs. `TerminalManager` owns session creation, tab metadata, project working directories, shell selection, and command launch requests from other features.

Detailed terminal design is in `engineering/terminal-integration.md`.

## Recommended Design Patterns

- Adapter for AI agents, GitHub backends, and memory providers.
- Facade for subsystem services used by UI panels.
- Repository pattern for SQLite access.
- Model/view for Qt UI data.
- Command pattern for user-triggered operations.
- Observer/signals for workflow progress.
- Strategy for agent execution modes and context retrieval.

Detailed pattern guidance is in `engineering/design-patterns.md`.

## MVP Feature Set

- Native Qt/KDE application shell.
- Three primary panels: repository, AI agent, terminal.
- SQLite-backed settings and project list.
- Add/open local repositories.
- Show current branch and changed files.
- QTermWidget terminal tabs per project.
- Built-in CLI adapter for one initial agent, preferably Codex.
- Agent session persistence.
- Context retrieval pipeline stub with MCP configuration.
- Basic diff display and approval/reject workflow.
- GitHub CLI authentication detection.
- Read GitHub repositories, issues, and pull requests through `gh`.

## Phase 2 Feature Set

- Multiple agent adapters: Codex, Claude, aider, Copilot, Cursor where feasible.
- Full MCP memory search integration.
- Rich context preview and attach/detach controls.
- Commit history and branch/tag management.
- Pull request review workflow.
- Build/test command profiles.
- Terminal session restoration.
- Focused KTextEditor workspace tabs for small in-cockpit file edits.
- More complete diff viewer and patch application flow.

## Future Roadmap

- Dynamic third-party agent plugins.
- Local LLM adapters.
- Background repository indexing.
- Advanced RAG result ranking.
- GitHub REST API sync for richer state.
- Project dashboards.
- Cross-repository memory.
- Optional editor mode if demand justifies it.

See `plans/roadmap.md` for milestone ordering.

## Risk Summary

- Scope creep toward full IDE behavior.
- Agent CLIs have inconsistent capabilities and output formats.
- Terminal process handling can become fragile.
- MCP/RAG contract may change independently.
- GitHub CLI output and authentication states need robust error handling.
- KDE/Qt dependency packaging can vary across distributions.

See `engineering/risk-analysis.md` for mitigation details.

## Development Milestones

1. M0: Planning and architecture baseline.
2. M1: Native app shell and layout.
3. M2: Repository, Git, and terminal foundation.
4. M3: Agent sessions and diff review.
5. M4: MCP memory and context pipeline.
6. M5: GitHub workflows.
7. M6: Beta hardening and packaging.

Milestone specs are in `milestones/`.

## First 20 Implementation Tasks

The prioritized task list is in `engineering/implementation-tasks.md`.

## Suggested Class Diagram

```text
MainWindow
  -> RepositoryPanel
  -> AgentPanel
  -> TerminalPanel

RepositoryPanel -> ProjectManager -> GitService
AgentPanel -> AgentManager -> AgentAdapter
AgentPanel -> ContextManager -> MemoryService
TerminalPanel -> TerminalManager -> QTermWidget
GitHubPanel/RepositoryPanel -> GitHubService -> GhCliClient

StorageService <- ProjectManager
StorageService <- AgentManager
StorageService <- TerminalManager
StorageService <- SettingsService
```

## Suggested Communication Flow

Agent request:

```text
User prompt
  -> AgentPanel
  -> ContextManager
  -> MemoryService
  -> AgentManager
  -> Selected AgentAdapter
  -> Agent response stream
  -> Diff review
  -> User approval
  -> GitService or TerminalManager
```

Repository selection:

```text
User selects repository
  -> ProjectManager marks active project
  -> GitService refreshes status
  -> GitHubService refreshes issue/PR summaries
  -> TerminalManager switches default working directory
  -> AgentPanel refreshes agent selector from .qtcode/config.yaml default
  -> AgentManager restores last active session or creates one
  -> ContextManager scopes future retrieval
```
