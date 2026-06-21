# ADR 0011: Add KTextEditor Workspace Tabs For Focused File Editing

## Status

Accepted

## Context

ADR 0008 kept KTextEditor limited to optional preview and review infrastructure so QTCode would not drift into a full IDE. Daily dogfooding now needs a narrower editing path inside the cockpit: browse the active repository tree, open files, make small edits, create or delete files and folders, and keep the AI chat immediately reachable.

The requested product shape is still not a full IDE. The repository and file tree share the left panel space. File contents open as tabs in the main work area. The AI agent chat is also a main work-area tab, but it cannot be closed. Terminal workflows remain first-class below the main work area.

## Decision

Use KDE's **KTextEditor** component for focused, native file editing in QTCode.

The implementation must introduce a tabbed main work area:

- A permanent, non-closable **AI Chat** tab hosts the existing conversation panel.
- File tabs host KTextEditor views for files opened from the folder tree.
- File tabs may be closed, reordered if Qt's tab widget supports it cleanly, and marked modified when their document is dirty.
- The terminal panel remains available beneath the tabbed work area through the existing vertical splitter.

The left repository column becomes a shared project-navigation space:

- One view keeps the existing repository, Git, GitHub issue, and pull request context.
- One view shows a folder tree rooted at the active project's repository path.
- The two views share the same left-column width and layout persistence.

File and folder operations are intentionally modest:

- Open files from the folder tree into main work-area tabs.
- Edit and save text files through KTextEditor.
- Create files and folders under the active repository root.
- Rename and delete files and folders under the active repository root.
- Prevent path traversal and operations outside the active repository root.
- Prefer safe deletion through KDE/Qt facilities when available, with clear user confirmation for destructive fallback deletion.

QTCode must not add language-server management, debugger workflows, project-wide symbol search, build-system introspection, editor extensions, or IDE-style refactoring as part of this decision.

## Consequences

Positive:

- Provides a native KDE editing path without building an editor from scratch.
- Keeps AI chat, file contents, repository context, and terminal workflows in one cockpit.
- Makes small agent-assisted edits easier to inspect and adjust before running commands.
- Reuses KTextEditor syntax highlighting, undo/redo, modified state, and document save semantics.

Tradeoffs:

- KTextEditor becomes a required dependency for this feature slice.
- MainWindow layout must change from a chat-over-terminal splitter to workspace-tabs-over-terminal.
- RepositoryPanel needs a clearer boundary between repository context and file navigation.
- File mutation workflows need careful guardrails to avoid accidental edits outside the project root.

## Implementation Notes

- Add a `WorkspaceTabs` or similarly named UI component to own the permanent chat tab and file editor tabs.
- Keep `AgentPanel` as the source of agent workflow widgets; move only its conversation panel into the permanent tab.
- Add a `FileTreePanel` or similar widget for the active repository filesystem tree.
- Use Qt item models such as `QFileSystemModel` where practical, filtered to the active repository root.
- Add an editor-facing service only if needed to centralize open-document state, path validation, and save/close prompts.
- Keep filesystem mutations project-root-scoped and status-bar backed through `StatusService`.
- Update CMake, toolchain checks, packaging notes, and beta setup docs for the KTextEditor dependency.

## Follow-Ups

- Update the UI layout spec after the implementation settles the exact widget names.
- ~~Decide whether generated-change review should reuse KTextEditor tabs or stay in `DiffReviewView`.~~ Resolved (2026-06): the Generated Changes / `DiffReviewView` panel was removed. Agent file changes are reviewed through git status, the repository changed-files list, and KTextEditor workspace tabs.
- Consider adding external-editor handoff later for workflows that exceed the cockpit's editing scope.

## Related Documents

- [ADR 0002: Keep QTCode terminal-first and agent-first](0002-terminal-and-agent-first-product-shape.md)
- [ADR 0008: Treat KTextEditor as optional preview and review infrastructure](0008-optional-ktexteditor-preview.md)
- [ADR 0010: Use KF6 XmlGui for application actions, menus, and toolbars](0010-kf6-xmlgui-action-collections.md)
- [KTextEditor workspace specification](../specs/ktexteditor-workspace-spec.md)
