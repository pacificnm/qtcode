# Issue 006: Add Tests, Smoke Coverage, And Documentation Alignment

GitHub: https://github.com/pacificnm/qtcode/issues/116

Labels: `type:task`, `component:tests`, `component:docs`, `priority:p2`

Milestone: M6 Beta Hardening And Packaging, or the next active editor-workspace milestone if created.

Source:

- [ADR 0011](../../adrs/0011-ktexteditor-workspace-tabs.md)
- [KTextEditor workspace spec](../../specs/ktexteditor-workspace-spec.md)

## Task

Add focused verification for the KTextEditor workspace and align older docs that still describe KTextEditor as preview-only.

## Acceptance Criteria

- [x] Add unit coverage for project-root path validation and file/folder operation edge cases.
- [x] Add UI or smoke coverage for permanent AI chat tab, opening a file tab, dirty close prompt, and save.
- [x] Existing smoke tests still pass.
- [x] Update `docs/design/ui-layout-spec.md` to describe workspace tabs and the shared repository/files left project space.
- [x] Update roadmap, risk analysis, class responsibilities, beta setup, and packaging docs where they still conflict with ADR 0011.
- [x] Document any remaining editor non-goals so the feature does not expand into a full IDE scope.
