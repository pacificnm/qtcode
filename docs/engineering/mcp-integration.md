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
  -> Context results are ranked and grouped
  -> User can inspect or remove context
  -> AgentRequest includes selected context
```

## Result Model

Each context result should include:

- source type
- source URI or ID
- title
- excerpt
- score
- metadata
- retrieval timestamp

## Configuration

MCP server configuration is stored in SQLite:

- name
- endpoint
- transport
- enabled state
- provider-specific JSON config

Secrets should use KDE Wallet where possible.

## Failure Modes

- MCP server unavailable.
- Query timeout.
- Invalid response schema.
- No results.
- Project not indexed.

Each failure should be visible in the context UI and should not prevent direct agent usage.
