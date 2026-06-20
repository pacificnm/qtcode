# QTCode Roadmap

## MVP

The MVP proves that QTCode can be a native terminal-first AI cockpit without becoming a full IDE.

Core outcomes:

- Launch quickly as a native Qt/KDE application.
- Open and remember local repositories.
- Show repository status, branch, and changed files.
- Provide project-aware QTermWidget terminal tabs.
- Run one initial AI CLI adapter through a persistent session model.
- Retrieve or stub project context before agent calls.
- Save agent sessions and messages to SQLite.
- Show generated diffs and allow user approval or rejection.
- Surface GitHub repository, issue, and pull request summaries through `gh`.

## Phase 2

Phase 2 expands from proof of workflow to daily usability.

Core outcomes:

- Add additional agent adapters.
- Integrate the existing MCP/RAG memory system fully.
- Let users inspect retrieved context before sending it to agents.
- Restore terminal sessions and command metadata.
- Add commit history, tags, branch actions, and diff navigation.
- Add GitHub issue and pull request actions through `gh`.
- Add build/test command profiles per project.
- Add optional read-only KTextEditor preview.

## Phase 3

Phase 3 focuses on scale, automation, and extensibility.

Core outcomes:

- Dynamic plugin loading for third-party agents.
- Local LLM support.
- Background repository indexing.
- Advanced context ranking and memory attribution.
- GitHub REST API sync where `gh` is not enough.
- Cross-repository memory and workspace-level context.
- More capable review workflows for agent-generated changes.

## Non-Goals Across All Phases

- No Electron dependency.
- No Monaco clone.
- No VS Code extension compatibility layer.
- No full-featured editor until the terminal/agent cockpit is excellent.
- No hard dependency on one AI provider.
