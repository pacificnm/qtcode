# ADR 0007: Use QTermWidget For Terminal Tabs

## Status

Accepted

## Context

The terminal is a primary workflow component. It must run shells, Git, GitHub CLI, agent CLIs, build tools, and test tools in project-aware tabs.

## Decision

Use QTermWidget for embedded terminals. Manage tabs, working directories, shell selection, and session metadata through `TerminalManager`.

## Consequences

Positive:

- Native Qt integration.
- Mature terminal emulation for Linux desktop workflows.
- Natural fit for agent and build command execution.

Tradeoffs:

- Persistent terminal restoration may be limited to metadata unless process persistence is added.
- Terminal lifecycle and child process cleanup require careful handling.

## Follow-Ups

- Define what "persistent sessions" means for MVP.
- Store terminal metadata in SQLite.
- Add per-project terminal profiles.
