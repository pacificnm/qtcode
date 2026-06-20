# QTCode

QTCode is a planned native KDE/Linux developer cockpit for AI-assisted development, terminal workflows, GitHub work, Git operations, and project memory.

It is intentionally not another VS Code, JetBrains IDE, Monaco shell, or Electron application. The goal is a fast, lightweight Qt desktop app for developers who primarily work through AI coding agents and terminals, with traditional editing treated as optional and secondary.

## Vision

QTCode should become the central workspace for:

- AI coding agents
- local and GitHub repositories
- terminal-first build, test, and agent workflows
- Git status, diffs, branches, and history
- GitHub issues and pull requests
- project memory, MCP tools, RAG, and vector search
- saved agent sessions and reusable project context

The product shape is simple: repository context on the left, AI agent work in the main panel, and terminals across the bottom.

```text
+----------------------+-----------------------------+
| Repository Panel     | AI Agent Panel              |
|                      |                             |
| Local repositories   | Chat sessions               |
| GitHub repositories  | Agent responses             |
| Branches/files       | Context and memory results  |
| Issues/PRs           | Diffs and approvals         |
+----------------------+-----------------------------+
| Terminal Panel                                     |
| Shell tabs, build output, agent execution          |
+----------------------------------------------------+
```

## Principles

- Terminal-first
- AI-first
- GitHub-first
- Context-first
- KDE-native
- Fast startup
- Low memory usage
- No Electron dependency
- No web-runtime dependency
- No full IDE rebuild

## Planned Technology

- C++20 or newer
- Qt 6
- CMake
- KDE Frameworks
- QTermWidget
- SQLite
- libgit2 and Git CLI
- GitHub CLI first, GitHub REST API later if needed
- Pluggable AI agent adapters
- MCP integration with an existing Python RAG service, PostgreSQL, pgvector, and vector search

## Documentation

Start here:

- [Documentation index](docs/README.md)
- [Toolchain requirements](docs/toolchain-requirements.md)
- [GitHub repo conventions](docs/github-repo-conventions.md)
- [Architecture and implementation plan](docs/plans/architecture-and-implementation-plan.md)
- [Roadmap](docs/plans/roadmap.md)
- [Product spec](docs/design/product-spec.md)
- [UI layout spec](docs/design/ui-layout-spec.md)
- [Interaction spec](docs/design/interaction-spec.md)
- [Engineering architecture](docs/engineering/architecture.md)
- [First implementation tasks](docs/engineering/implementation-tasks.md)

### Milestones

- [M0: Planning and architecture baseline](docs/milestones/m00-planning.md)
- [M1: Native app shell](docs/milestones/m01-native-app-shell.md)
- [M2: Repository, Git, and terminal foundation](docs/milestones/m02-repository-git-terminal-foundation.md)
- [M3: Agent sessions and diff review](docs/milestones/m03-agent-sessions-and-diff-review.md)
- [M4: MCP memory and context pipeline](docs/milestones/m04-mcp-memory-context-pipeline.md)
- [M5: GitHub workflows](docs/milestones/m05-github-workflows.md)
- [M6: Beta hardening and packaging](docs/milestones/m06-beta-hardening-and-packaging.md)

### Engineering Specs

- [Project structure](docs/engineering/project-structure.md)
- [CMake organization](docs/engineering/cmake-organization.md)
- [Class responsibilities](docs/engineering/class-responsibilities.md)
- [Database schema](docs/engineering/database-schema.md)
- [Agent plugin architecture](docs/engineering/plugin-agent-architecture.md)
- [MCP integration](docs/engineering/mcp-integration.md)
- [Git and GitHub integration](docs/engineering/git-github-integration.md)
- [Terminal integration](docs/engineering/terminal-integration.md)
- [Communication flows](docs/engineering/communication-flows.md)
- [Design patterns](docs/engineering/design-patterns.md)
- [Risk analysis](docs/engineering/risk-analysis.md)

### Project Files

- [License](LICENSE)
- [Contributing guide](CONTRIBUTING.md)
- [Security policy](SECURITY.md)
- [Agent rules](AGENTS.md)
- [.github issue templates](.github/ISSUE_TEMPLATE/config.yml)

### Architecture Decision Records

- [ADR 0001: Build QTCode as a native Qt/KDE desktop application](docs/adrs/0001-native-qt-kde-desktop.md)
- [ADR 0002: Keep QTCode terminal-first and agent-first](docs/adrs/0002-terminal-and-agent-first-product-shape.md)
- [ADR 0003: Use agent adapters behind a stable interface](docs/adrs/0003-agent-adapter-architecture.md)
- [ADR 0004: Use SQLite for local application state](docs/adrs/0004-sqlite-for-local-application-state.md)
- [ADR 0005: Integrate existing memory through MCP and RAG services](docs/adrs/0005-memory-through-mcp-rag-system.md)
- [ADR 0006: Use libgit2, Git CLI, and GitHub CLI first](docs/adrs/0006-git-and-github-cli-first-integration.md)
- [ADR 0007: Use QTermWidget for terminal tabs](docs/adrs/0007-qtermwidget-terminal-integration.md)
- [ADR 0008: Treat KTextEditor as optional preview and review infrastructure](docs/adrs/0008-optional-ktexteditor-preview.md)

## Current Status

QTCode is in the planning and architecture phase. The repository currently contains project documentation, milestone specs, ADRs, engineering specs, and design specs. Application code has not been generated yet.
