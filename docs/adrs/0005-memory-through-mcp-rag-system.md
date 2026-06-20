# ADR 0005: Integrate Existing Memory Through MCP And RAG Services

## Status

Accepted

## Context

The user already has a Python MCP server, PostgreSQL, pgvector, a RAG layer, indexed documentation, and vector search. QTCode should reuse this instead of building a parallel memory system.

## Decision

Implement a `MemoryService` that talks to the existing MCP/RAG system. `ContextManager` uses this service before agent execution to retrieve relevant project memory, documentation, architecture decisions, coding standards, and search results.

## Consequences

Positive:

- Avoids duplicating existing memory infrastructure.
- Makes memory reusable across all agents.
- Keeps QTCode focused on orchestration and UI.

Tradeoffs:

- QTCode depends on the availability and contract of an external local service.
- Error states must be visible and non-blocking.

## Follow-Ups

- Define a stable request/response contract for memory search.
- Cache lightweight retrieval metadata in SQLite.
- Provide health checks and configuration UI for MCP servers.
