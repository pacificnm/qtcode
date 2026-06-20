# ADR 0004: Use SQLite For Local Application State

## Status

Accepted

## Context

QTCode needs durable local state for settings, repositories, sessions, chat history, terminal metadata, agent configuration, and MCP server configuration. It does not need a local server database for this state.

## Decision

Use SQLite as the local application database. Use migrations from the first implementation milestone.

## Consequences

Positive:

- Simple deployment and backup.
- Strong fit for local desktop application state.
- Good Qt support through SQL abstractions or direct SQLite wrappers.

Tradeoffs:

- Not suited for heavy vector search, which remains in the existing PostgreSQL/pgvector system.
- Migration discipline is required from the start.

## Follow-Ups

- Create `schema_migrations`.
- Keep project memory embeddings outside SQLite.
- Store secrets through KDE Wallet where possible instead of plaintext SQLite.
