# ADR 0003: Use Agent Adapters Behind A Stable Interface

## Status

Accepted

## Context

QTCode must support Codex, Cursor, GitHub Copilot, Claude, aider, future local LLMs, and custom agents. These tools may expose CLIs, APIs, MCP endpoints, or mixed execution models.

## Decision

Create an `AgentAdapter` interface and route all agent execution through `AgentManager`. Built-in adapters can ship first. Dynamic plugins can be added later without changing the UI workflow.

## Consequences

Positive:

- UI and session storage are independent of specific agents.
- New agents can be added with isolated implementation work.
- CLI, API, and MCP agents can share common session concepts.

Tradeoffs:

- Lowest-common-denominator interfaces can hide provider-specific features.
- CLI output parsing and process lifecycle handling need careful adapter-specific code.

## Follow-Ups

- Define adapter capabilities explicitly.
- Store per-agent configuration in SQLite.
- Add conformance tests for each adapter.
