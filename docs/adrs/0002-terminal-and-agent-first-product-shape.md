# ADR 0002: Keep QTCode Terminal-First And Agent-First

## Status

Accepted

## Context

QTCode is for developers who spend most of their time working through AI agents, terminals, Git, and GitHub. The editor is optional and secondary.

## Decision

Make terminal tabs, AI sessions, repository context, memory retrieval, and GitHub work the primary surfaces. Treat rich IDE features as out of scope while keeping focused file editing, diff review, and browsing tools secondary.

## Consequences

Positive:

- Product stays distinct from VS Code and JetBrains IDEs.
- Early engineering effort goes into the user's real workflow.
- UI complexity stays focused.

Tradeoffs:

- Users expecting a traditional IDE may find the app incomplete.
- Some workflows may still need external editors for work that exceeds the focused workspace editing slice in [ADR 0011](0011-ktexteditor-workspace-tabs.md).

## Follow-Ups

- Keep milestone acceptance criteria tied to agent/terminal workflows.
- Reject feature work that implies a full editor unless it supports agent review or context browsing.
