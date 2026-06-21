# Issue 004: Implement KTextEditor File Tabs And Save Lifecycle

GitHub: https://github.com/pacificnm/qtcode/issues/114

Labels: `type:task`, `component:ui`, `component:editor`, `priority:p1`

Milestone: M6 Beta Hardening And Packaging, or the next active editor-workspace milestone if created.

Source:

- [ADR 0011](../../adrs/0011-ktexteditor-workspace-tabs.md)
- [KTextEditor workspace spec](../../specs/ktexteditor-workspace-spec.md)

## Task

Open files from the folder tree into KTextEditor-backed tabs and implement save, dirty-state, and close prompts.

## Acceptance Criteria

- Opening a text file creates a KTextEditor document/view in a closable main work-area tab.
- Opening the same file again focuses the existing tab instead of duplicating it.
- Dirty documents mark their tab as modified.
- Save writes the document to disk and clears the dirty indicator.
- Closing a dirty tab prompts to save, discard, or cancel.
- Switching projects and quitting prompt for dirty editor tabs before closing.
- Load and save failures are surfaced through `StatusService` and a user-actionable dialog when needed.
