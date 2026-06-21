# ADR 0008: Treat KTextEditor As Optional Preview And Review Infrastructure

## Status

Accepted (partially superseded by [ADR 0011](0011-ktexteditor-workspace-tabs.md) for workspace file editing)

## Context

QTCode should not become a traditional editor. Some code display is still useful for diffs, read-only browsing, and reviewing agent-generated changes.

ADR 0011 added focused KTextEditor workspace tabs for small in-cockpit edits. This ADR still applies to diff review and any read-only preview surfaces that are not part of the workspace tab model.

## Decision

Keep general editor functionality secondary. Use KTextEditor for preview, read-only browsing, focused review surfaces, and the ADR 0011 workspace editing slice when needed.

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
