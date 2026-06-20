# QTCode Agent Rules

These rules apply to any agent working in this repository.

## 1. Search Memory First

Before any implementation, refactor, or repository-specific answer, search both memory stores:

1. `search_project_memory` for specs, ADRs, milestones, and engineering docs.
2. `search_agent_context` for prior decisions, compaction notes, blockers, and task state for the active workspace scope.

If MCP is available, always use those MCP tools first.
Use `scripts/search-memory` and `scripts/search-agent-context` only when MCP is unavailable in the current environment.

When conversation context is compacted or a long task spans multiple sessions, save durable task notes with `save_agent_context` and recover them with `search_agent_context` for the active repository scope.

Cursor project hooks bootstrap both searches at session start and save compaction context automatically. Agents should still call the MCP tools directly during the session when the task changes.

Preferred query pattern:

- project name and subsystem
- relevant file names
- related ADR or milestone names
- user goal in plain language

If memory search is unavailable, say so and fall back to the docs in this repo.

## 2. Read The Right Source Material

After memory search, read the smallest set of local sources that can settle the task:

- `README.md`
- `PROMPT.md`
- `docs/README.md`
- the relevant milestone spec
- the relevant ADRs
- the relevant engineering or design specs
- the exact files you will change

Do not guess when the answer is in the repo.

## 3. Prefer Existing Patterns

Match the codebase before inventing new structure.

- Reuse local naming, architecture, and folder layout.
- Prefer small, focused changes over broad rewrites.
- Add an abstraction only when it removes real complexity.
- Do not build new systems that duplicate existing ones.

## 4. Protect The Product Shape

QTCode is a native KDE/Linux cockpit, not a full IDE.

- Keep terminal, AI agent, repository, GitHub, and memory workflows first-class.
- Treat editor features as secondary unless the docs explicitly require them.
- Do not introduce Electron, browser UI, or web-runtime dependencies.

## 5. Keep Memory And Context Central

Project memory and agent context memory are part of the workflow, not an afterthought.

- Search project memory and agent context before changing code.
- Save agent context after major decisions, completed steps, or before likely compaction.
- Prefer retrieved context over stale assumptions.
- Use memory results together with ADRs and milestone docs.
- If memory and docs disagree, prefer the docs and call out the mismatch.
- Prefer the wrapper scripts in `scripts/` for memory tooling and other repeated repo operations. Use `/usr/bin/python3` only for stdlib-only scripts; use `.venv/bin/python` for the memory tooling and any script that needs third-party packages.
- When Codex is the active agent, the default memory path is the MCP tools on `qtcode-memory`: `search_project_memory`, `search_agent_context`, and `save_agent_context`.

## 6. Verify Before You Change

Before editing, confirm:

- what files are relevant
- what code already exists
- what behavior must stay stable
- what tests or checks are available

Use `rg`, `rg --files`, `git status`, and targeted file reads. Avoid broad speculation.

## 7. Edit Carefully

- Use `apply_patch` for manual file edits.
- Keep changes scoped to the requested work.
- Do not overwrite unrelated user work.
- Do not revert changes you did not make unless explicitly asked.
- Default to ASCII unless the file already uses Unicode.

## 8. Validate The Change

After implementation, run the narrowest useful verification:

- syntax checks for scripts
- targeted tests
- build or lint checks for touched areas
- a quick diff review for unexpected churn

If you cannot validate something, say exactly what remained unverified.

## 9. Make Assumptions Visible

When something is ambiguous:

- state the assumption
- choose the least risky path
- keep the change small
- ask the user only when the ambiguity materially changes the solution

## 10. High-Confidence Delivery

No rule guarantees success, but the default should be to raise confidence before acting.

- Start with memory search.
- Cross-check against docs and existing code.
- Prefer stable interfaces and explicit data flow.
- Keep dependencies simple.
- Add tests around the riskiest path.
- Leave the repo easier to reason about than you found it.
