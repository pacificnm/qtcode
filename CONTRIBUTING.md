# Contributing To QTCode

Thanks for helping build QTCode.

## Before You Start

- Read `README.md`.
- Read `PROMPT.md`.
- Read `AGENTS.md`.
- Read the relevant docs under `docs/`.
- Search project memory first using `/usr/bin/python3 tools/search_memory.py ...` or the MCP `search_project_memory` tool.

## What We Care About

QTCode is a native KDE/Linux developer cockpit, not a full IDE. Contributions should improve:

- repository context
- terminal workflows
- AI agent workflows
- Git and GitHub work
- project memory and retrieval
- startup speed and low memory usage

## Local Workflow

1. Make the smallest change that solves the problem.
2. Use `apply_patch` for manual file edits.
3. Keep code and docs aligned.
4. Run the narrowest useful verification.
5. Leave the tree cleaner than you found it.

## Tooling

- Use `/usr/bin/python3` for the repository helper scripts in `tools/`.
- Prefer `rg` and `rg --files` for searches.
- Avoid introducing new dependencies unless they clearly improve the project.

## Pull Requests

- Describe the user-visible change.
- Link the relevant milestone or issue when available.
- Mention any tradeoffs or follow-up work.
- Include verification notes.

## Style

- Keep changes scoped.
- Match the existing architecture.
- Prefer small, readable commits.
- Don’t overbuild the editor side of the app.

## Need Help?

Open an issue or draft a discussion with the relevant docs, command output, and what you were trying to do.
