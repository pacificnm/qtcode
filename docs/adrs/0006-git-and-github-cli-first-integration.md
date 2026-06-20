# ADR 0006: Use libgit2, Git CLI, And GitHub CLI First

## Status

Accepted

## Context

QTCode needs local Git state and GitHub integration. Local Git reads benefit from library access, while many mutating operations are safest when delegated to the user's existing tools and authentication.

## Decision

Use libgit2 for fast local repository inspection where practical. Use the Git CLI for operations that should match user expectations. Use GitHub CLI (`gh`) for GitHub authentication and MVP GitHub workflows. Consider GitHub REST API later.

## Consequences

Positive:

- Reuses user Git and GitHub authentication.
- Avoids early OAuth implementation.
- Keeps behavior close to command-line workflows.

Tradeoffs:

- CLI output and exit states need robust parsing.
- `gh` must be installed and authenticated for GitHub features.
- Some advanced GitHub UI may require REST API later.

## Follow-Ups

- Add capability detection for `git` and `gh`.
- Prefer JSON output from `gh` whenever available.
- Route destructive Git actions through explicit confirmation flows.
