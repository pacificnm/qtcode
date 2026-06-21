# Issue 005: Implement Project-Root-Scoped File And Folder Operations

GitHub: https://github.com/pacificnm/qtcode/issues/115

Labels: `type:task`, `component:ui`, `component:filesystem`, `priority:p1`

Milestone: M6 Beta Hardening And Packaging, or the next active editor-workspace milestone if created.

Source:

- [ADR 0011](../../adrs/0011-ktexteditor-workspace-tabs.md)
- [KTextEditor workspace spec](../../specs/ktexteditor-workspace-spec.md)

## Task

Add create, rename, and delete operations for files and folders under the active repository root.

## Acceptance Criteria

- Users can create a new file under the selected folder or repository root.
- Users can create a new folder under the selected folder or repository root.
- Users can rename files and folders under the active repository root.
- Users can delete files and folders only after confirmation.
- All operations reject paths that resolve outside the active repository root.
- Open editor tabs update or close safely after rename/delete operations.
- File tree refreshes after each successful operation.
- Errors are shown through status messages and dialogs where user action is required.
