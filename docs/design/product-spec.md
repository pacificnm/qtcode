# Product Spec

## Product Summary

QTCode is a native KDE/Linux developer cockpit for AI-assisted coding, terminal workflows, GitHub, Git operations, and project memory. It is designed for developers who spend more time with AI agents and shells than with traditional code editors.

## Primary Users

- KDE/Linux developers.
- Developers using AI coding agents daily.
- Developers who prefer terminal workflows.
- Developers managing multiple local and GitHub repositories.
- Developers with existing project memory and RAG infrastructure.

## Jobs To Be Done

- Open a repository and immediately see useful project state.
- Run terminal commands in the correct project context.
- Ask an AI agent to work on a project with relevant memory attached.
- Inspect what context was retrieved before or during an agent session.
- Review agent file changes through git status and workspace tabs.
- Track related GitHub issues and pull requests.
- Save conversations and return to them later.
- Reuse approved repo-native commands for repeatable AI and developer workflows.

## Product Non-Goals

- Do not become a full IDE.
- Do not clone VS Code, JetBrains, Monaco, or Electron workflows.
- Do not replace the user's shell.
- Do not replace the existing PostgreSQL/pgvector RAG system.
- Do not require a single AI vendor.

## MVP Features

- Native application shell.
- Repository panel.
- AI agent panel.
- Terminal panel.
- SQLite local state.
- Local repository list.
- Basic Git status.
- QTermWidget terminal tabs.
- One CLI-based agent adapter.
- Agent session persistence.
- Basic context retrieval architecture and attach/detach controls.
- Repository-native review of agent file changes.
- GitHub CLI status and summary integration.

## Success Criteria

- App starts quickly.
- Common flows do not block the UI.
- The user can move from repository context to terminal or agent work with little friction.
- Agent sessions are recoverable.
- Generated changes are reviewable through git and the repository changed-files list.
- Missing external tools are understandable, not mysterious.
