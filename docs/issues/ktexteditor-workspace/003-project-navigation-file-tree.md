# Issue 003: Add Shared Repository/Files Left Project Space

GitHub: https://github.com/pacificnm/qtcode/issues/113

Labels: `type:task`, `component:ui`, `component:repository`, `priority:p1`

Milestone: M6 Beta Hardening And Packaging, or the next active editor-workspace milestone if created.

Source:

- [ADR 0011](../../adrs/0011-ktexteditor-workspace-tabs.md)
- [KTextEditor workspace spec](../../specs/ktexteditor-workspace-spec.md)

## Task

Let the left column switch between the existing repository context and a new folder tree rooted at the active repository path.

## Acceptance Criteria

- [x] The existing `RepositoryPanel` behavior remains available in the left column.
- [x] A `Files` view appears in the same left-column space, not in a new permanent column.
- [x] The folder tree follows `ProjectManager` active-project changes.
- [x] Activating a text file emits a request to open that file in the main workspace tabs.
- [x] Empty, missing-path, and no-active-project states are handled clearly.
- [x] Repository GitHub issue and pull request context attachment still works from the repository view.
