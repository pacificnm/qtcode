# Recommended Design Patterns

## Adapter

Use for:

- AI agents
- GitHub backends
- memory providers
- external process execution modes

Why:

- Keeps provider-specific behavior isolated.
- Lets the UI talk to stable interfaces.

## Facade

Use for services exposed to UI panels:

- `GitService`
- `GitHubService`
- `MemoryService`
- `TerminalManager`

Why:

- Prevents widgets from coordinating low-level subsystems directly.

## Repository

Use for SQLite access:

- `ProjectRepository`
- `AgentSessionRepository`
- `SettingsRepository`

Why:

- Keeps SQL localized and testable.

## Qt Model/View

Use for:

- repositories
- changed files
- sessions
- context results
- terminal tabs
- issues and pull requests

Why:

- Keeps views responsive and consistent with Qt idioms.

## Command

Use for user actions:

- approve diff
- reject diff
- open repository
- refresh status
- run build
- attach context

Implementation:

- Define shared actions once in a `KActionCollection` owned by `MainWindow`.
- Reuse the same `QAction` instances from menus, toolbars, and future command palettes.
- Connect actions to existing panel slots or services instead of duplicating workflow logic in widgets.

See [ADR 0010: Use KF6 XmlGui for application actions, menus, and toolbars](../adrs/0010-kf6-xmlgui-action-collections.md).

Why:

- Makes actions reusable from menus, shortcuts, buttons, and command palettes.

## Strategy

Use for:

- context retrieval strategies
- agent execution modes
- Git operation backends

Why:

- Enables different behavior without large conditional blocks.

## Observer Through Qt Signals

Use Qt signals and slots for progress, state changes, and async completions.

Why:

- Fits Qt and keeps UI updates decoupled from worker code.
