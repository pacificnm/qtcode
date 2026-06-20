# ADR 0002: Keep QTCode Terminal-First And Agent-First

## Status

Accepted

## Context

QTCode is for developers who spend most of their time working through AI agents, terminals, Git, and GitHub. The editor is optional and secondary.

## Decision

Make terminal tabs, AI sessions, repository context, memory retrieval, and GitHub work the primary surfaces. Treat editor features as optional preview, diff, and browsing tools.

## Consequences

Positive:

- Product stays distinct from VS Code and JetBrains IDEs.
- Early engineering effort goes into the user's real workflow.
- UI complexity stays focused.

Tradeoffs:

- Users expecting a traditional IDE may find the app incomplete.
- Some workflows need external editors until optional KTextEditor support exists.

## Follow-Ups

- Keep milestone acceptance criteria tied to agent/terminal workflows.
- Reject feature work that implies a full editor unless it supports agent review or context browsing.
