# Issue 001: Add KTextEditor Dependency And Toolchain Documentation

Labels: `type:task`, `component:build`, `component:docs`, `priority:p1`

Milestone: M6 Beta Hardening And Packaging, or the next active editor-workspace milestone if created.

Source:

- [ADR 0011](../../adrs/0011-ktexteditor-workspace-tabs.md)
- [KTextEditor workspace spec](../../specs/ktexteditor-workspace-spec.md)

## Task

Add KDE KTextEditor as a required dependency for the workspace editing feature and update all build, toolchain, and packaging docs.

## Acceptance Criteria

- CMake discovers KTextEditor and links the relevant target into the UI/app target.
- Missing KTextEditor produces a clear configure-time dependency error.
- `scripts/check-toolchain` checks for the dependency.
- `docs/toolchain-requirements.md` lists the required package and verification behavior.
- Beta setup and packaging notes mention the runtime/development package names.
- Existing build and test scripts still run after dependency detection is added.
