# GitHub Repo Conventions

This document defines how QTCode issues, milestones, labels, and pull requests should be used.

## Milestones

- Every implementation issue should belong to exactly one milestone.
- Milestones should map to the planning docs in `docs/milestones/`.
- Milestone names should stay stable once created.
- Milestone scope should be narrow enough to finish in a coherent pass.

## Labels

Required labels for implementation work:

- one `type:*` label
- one or more `component:*` labels
- a `priority:*` label when the issue is actionable

Recommended label meanings:

- `type:task` for bounded implementation work
- `type:feature` for user-facing capability requests
- `type:bug` for defects and regressions
- `type:planning` for architecture or roadmap work
- `priority:p1` for the current critical path
- `priority:p2` for important but non-blocking follow-up work

## Issue Rules

- Write the user outcome first.
- Include acceptance criteria on every non-trivial issue.
- Link the relevant doc, ADR, or milestone source.
- Keep issues small enough to finish in one meaningful pass.
- Add reproduction steps for bugs.
- Add expected behavior for bugs and tasks.

## Milestone Assignment

- Assign the milestone after the issue is created if the issue template does not do it automatically.
- If an issue crosses milestones, split it.
- If an issue is too vague to fit a milestone, refine the issue before starting work.

## Pull Requests

- Keep PRs focused on one issue or one small cluster of related issues.
- Reference the milestone and issue number in the PR description.
- Include verification notes.
- Mention any follow-up work explicitly.

## Repository Hygiene

- Update docs when behavior changes.
- Keep labels, docs, and issues aligned.
- Prefer a small backlog with clear acceptance criteria over a large pile of vague tickets.
