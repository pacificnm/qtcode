# Issue 002: Introduce Main Workspace Tabs With Permanent AI Chat

GitHub: https://github.com/pacificnm/qtcode/issues/112

Labels: `type:task`, `component:ui`, `component:agents`, `priority:p1`

Milestone: M6 Beta Hardening And Packaging, or the next active editor-workspace milestone if created.

Source:

- [ADR 0011](../../adrs/0011-ktexteditor-workspace-tabs.md)
- [KTextEditor workspace spec](../../specs/ktexteditor-workspace-spec.md)

## Task

Replace the current chat-over-terminal main-column arrangement with workspace tabs above the existing terminal panel. Move the AI conversation widget into a permanent, non-closable tab.

## Acceptance Criteria

- [x] `MainWindow` shows a main work-area tab control above the terminal splitter.
- [x] The existing AI conversation panel appears in an `AI Chat` tab.
- [x] The `AI Chat` tab cannot be closed through the tab UI or close-tab actions.
- [x] The terminal panel remains below the main work area and keeps existing tab behavior.
- [x] Existing agent session, context attachment, generated changes, right activity panel, and status bar connections continue to work.
- [x] Layout persistence remains stable for the left, main, right, and terminal splitter sizes.
