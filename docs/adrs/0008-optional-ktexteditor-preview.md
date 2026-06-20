# ADR 0008: Treat KTextEditor As Optional Preview And Review Infrastructure

## Status

Accepted

## Context

QTCode should not become a traditional editor. Some code display is still useful for diffs, read-only browsing, and reviewing agent-generated changes.

## Decision

Keep editor functionality optional and secondary. Use KTextEditor only for preview, read-only browsing, and focused review surfaces when needed.

## Consequences

Positive:

- Preserves the terminal/agent product focus.
- Leverages KDE-native editor infrastructure when useful.
- Avoids building a text editor from scratch.

Tradeoffs:

- Deep editing workflows remain outside the app at first.
- KTextEditor integration should not dominate early milestones.

## Follow-Ups

- Start with diff review before general file browsing.
- Add external editor handoff for full editing.
