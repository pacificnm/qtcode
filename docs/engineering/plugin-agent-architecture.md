# Agent Plugin Architecture

## Goals

- Support multiple agents without coupling the UI to any provider.
- Support CLI, API, and MCP-enabled agents.
- Let future agents be added with isolated adapter work.
- Preserve common session, context, and diff review flows.

## Core Interfaces

### `AgentAdapter`

Responsibilities:

- Return identity and display metadata.
- Report capabilities.
- Validate configuration.
- Start a request.
- Stream output events.
- Cancel in-flight work.
- Return generated artifacts, including diffs when available.

Capabilities should be explicit:

- `StreamingText`
- `FileMutation`
- `DiffOutput`
- `McpAware`
- `RequiresTerminal`
- `SupportsNonInteractiveMode`
- `SupportsProjectConfig`

### `AgentManager`

Responsibilities:

- Register built-in adapters at startup.
- Restore persisted agent sessions from SQLite.
- Detect external CLIs asynchronously after startup.
- Store agent availability.
- Track active session per project.
- Create sessions.
- Dispatch requests.

Default agent selection is not stored globally in SQLite. When the agent selector has no prior value for the active project, `AgentPanel` reads `agent.defaultAgentKey` or top-level `defaultAgentKey` from `.qtcode/config.yaml`. Per-project session history and the last active session id remain in SQLite.
- Normalize errors.

### `AgentSession`

Responsibilities:

- Track conversation state.
- Persist messages.
- Attach context retrievals.
- Track generated artifacts.
- Preserve agent and project metadata.

## Adapter Types

### CLI Adapters

Use child processes or terminal-launched commands. Prefer structured/non-interactive modes when the tool supports them.

Examples:

- Codex CLI
- Claude CLI
- aider
- Cursor CLI if available

### API Adapters

Use network clients and structured request/response handling. API adapters should be added after the local cockpit workflow is stable.

### MCP-Enabled Adapters

MCP-aware agents can receive tool or context configuration directly where supported. If an agent is not MCP-aware, QTCode still injects retrieved context into the normalized prompt.

## Request Flow

```text
AgentPanel
  -> AgentManager
  -> ContextManager
  -> MemoryService
  -> AgentRequest
  -> AgentAdapter
  -> AgentEvent stream
  -> AgentSessionRepository
  -> DiffReviewView
```

## Error Handling

Adapters should map provider-specific failures to common states:

- missing executable
- authentication required
- invalid configuration
- process failed
- request canceled
- unsupported capability
- rate limited
- unknown error

## Future Dynamic Plugins

Start with built-in adapters. Later, support dynamic plugins with metadata:

- adapter key
- display name
- executable or library path
- supported capabilities
- configuration schema
- version compatibility
