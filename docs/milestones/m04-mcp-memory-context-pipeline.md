# M4: MCP Memory And Context Pipeline

## Goal

Connect QTCode to the existing project memory and RAG system.

## User Outcome

Before sending a prompt to an agent, the user can see relevant project memory, documentation, architecture decisions, and coding standards retrieved from the existing MCP/RAG system.

## Scope

- MCP server configuration UI.
- MCP health checks.
- `MemoryService`
- `ContextManager`
- Memory search request model.
- Context result viewer.
- Attach/detach retrieved context for agent prompts.
- Save retrieval metadata with agent sessions.

## Engineering Deliverables

- MCP client boundary.
- Context ranking and grouping model.
- Agent request composition flow.
- Error handling for unavailable memory service.

## Exit Criteria

- Memory search runs for active project and user prompt.
- Retrieved context is visible before or alongside agent request.
- Memory service failure does not block basic agent usage.
- Retrieval metadata can be audited later from a saved session.

## Out Of Scope

- Building a new vector database.
- Re-indexing all project content inside QTCode.
