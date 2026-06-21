# MCP And Memory Integration Architecture

## Goals

- Reuse the existing Python MCP server, PostgreSQL, pgvector, RAG layer, indexed docs, and vector search.
- Make memory available to every agent.
- Keep memory failures non-fatal.
- Let users inspect retrieved context.

## Components

### `MemoryService`

Provides normalized methods:

- `searchProjectMemory(project, query, options)`
- `searchAgentContext(project, query, options)`
- `saveAgentContext(project, session, content, options)`
- `searchDocumentation(project, query, options)`
- `getArchitectureDecisions(project, query)`
- `getCodingStandards(project, query)`
- `healthCheck(serverConfig)`

### `McpClient`

Owns MCP transport details and protocol mapping.

### `ContextManager`

Combines context sources:

- user prompt
- active repository metadata
- Git status
- selected issue or pull request
- selected files or diffs
- project memory search results
- saved agent context search results
- project settings

## Agent Context Flow

```text
User prompt
  -> ContextManager builds retrieval query
  -> MemoryService queries MCP/RAG
  -> Context results are deduplicated by source key
  -> ContextResultsView shows list; user attaches/detaches for next prompt
  -> Manual SourceFile entries may be added from tree, changed files, or editor tabs
  -> AgentRequest includes attached context excerpts only
  -> Retrieval metadata persisted for session audit
```

## Result Model

Each context result includes:

- source type (`ContextSourceType`: project memory, agent context, documentation, architecture decision, coding standard, or source file)
- source URI or ID (project-relative path for manual files)
- title
- excerpt (merged when multiple chunks share a source)
- score
- metadata
- retrieval timestamp

Dedupe and file helpers live in `memory/ContextResult.cpp`:

- `contextResultSourceKey()` — stable key for attachment persistence
- `dedupeContextResultsBySource()` — one row per source; keeps highest score and merges excerpts
- `contextResultFromFilePath()` — manual file attachment with 64 KiB cap and binary rejection
- `parseProjectMemoryToolOutput()` — MCP tool output to `ContextResult` list

See [retrieved context spec](../specs/retrieved-context-spec.md) for UI behavior.

## Configuration

MCP server configuration is stored in SQLite:

- name
- endpoint
- transport
- enabled state
- provider-specific JSON config

Secrets should use KDE Wallet where possible.

MCP server secret values are stored in the KDE Wallet folder `QTCode` under keys
`mcp/<server-id>/<env-var-name>`. The MCP server settings panel lets users enter
secret values without persisting them in SQLite. When a wallet entry is missing,
QTCode falls back to `~/.openAi/key` for `OPENAI_API_KEY` and the default local
PostgreSQL socket URL for `DATABASE_URL`.

## Failure Modes

- MCP server unavailable.
- Query timeout.
- Invalid response schema.
- No results.
- Project not indexed.

Each failure should be visible in the context UI and should not prevent direct agent usage.
