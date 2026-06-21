# ADR 0009: QTCommands Must Be Repo-Native

## Status

Accepted

## Context

QTCode needs a reusable command system for project-specific AI and developer workflows. The goal is to preserve established patterns such as destructive action handling, component layouts, API handlers, documentation blocks, and test scaffolds even after context resets.

The command system must be portable across machines and agents, reviewable in pull requests, and independent of a single AI provider. It also needs to support repo-specific vocabulary, validation, examples, and long-term discoverability through MCP tooling.

## Alternatives Considered

### Editor-Local Snippets

Store commands in an editor-specific snippet format.

Pros:

- Easy to author in a familiar editor UI.
- Fast for one developer on one machine.

Cons:

- Not shared with the repository.
- Hard to review in pull requests.
- Not portable across editors or agents.
- Weak fit for project-specific rules and validation.

### Global User Snippets

Store commands in the user profile or editor settings.

Pros:

- Portable across repositories on the same machine.
- Good for personal productivity shortcuts.

Cons:

- Wrong ownership boundary for project conventions.
- Not versioned with the codebase.
- Invisible to other developers and CI review.
- Cannot safely capture project-specific vocabulary or architecture rules.

### AI Prompt-Only Instructions

Keep reusable patterns in system prompts or prompt fragments.

Pros:

- Simple to start.
- Works across different AI tools.

Cons:

- Ephemeral and hard to audit.
- Not executable as a first-class project artifact.
- Easy to lose during context resets.
- No stable place for examples, validation, or file templates.

### MCP-Only Memory

Store the reusable patterns only in memory/search infrastructure.

Pros:

- Discoverable by agents.
- Useful for historical guidance and retrieval.

Cons:

- Memory is not the source of truth for executable project patterns.
- Retrieval can miss, rank poorly, or fail.
- Not as reviewable as committed repository files.
- Does not naturally package templates, rules, and examples together.

### Repo-Native Command System

Store commands under `.qtcode/` in the repository.

Pros:

- Versioned with the project.
- Reviewable in pull requests.
- Portable across machines, shells, editors, and agents.
- Can bundle templates, examples, rules, and validation in one place.
- Supports a stable project vocabulary and local conventions.
- Gives MCP tools a clear source of truth to index and explain.

Cons:

- Requires a schema and renderer that QTCode must maintain.
- Needs validation to prevent stale or bad command definitions.
- Adds a new project artifact type that contributors must learn.

## Decision

QTCode will use a repo-native `.qtcode/` command system as the canonical location for reusable project commands.

Command definitions, templates, examples, rules, and memory notes will live in Git and be consumed by QTCode, MCP tooling, and supported agents.

## Consequences

Positive:

- Reusable patterns survive context resets.
- The team can review command changes alongside code changes.
- Agents can be instructed to prefer established project commands over invention.
- Validation can enforce project rules before generated code is applied.
- Commands can evolve with the repository instead of with a single editor or provider.

Tradeoffs:

- QTCode must implement command discovery, rendering, and validation.
- The schema should remain intentionally small to avoid duplicating a full workflow engine.
- Teams must keep command files, examples, and rules synchronized.

## Follow-Ups

- Define the command schema and validation rules in `docs/specs/qtcommands-spec.md`.
- Add the Command Library UI in `docs/design/command-library-panel.md`.
- Track delivery in `docs/milestones/m07-qtcommands.md`.
