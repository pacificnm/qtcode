# KTextEditor Workspace Specification

ADR: [ADR 0011: Add KTextEditor workspace tabs for focused file editing](../adrs/0011-ktexteditor-workspace-tabs.md)

Related docs:

- [UI layout spec](../design/ui-layout-spec.md)
- [Class responsibilities](../engineering/class-responsibilities.md)
- [Toolchain requirements](../toolchain-requirements.md)

## Purpose

Add a focused native editing workflow to QTCode without changing the product into a full IDE. Users should be able to browse the active repository's folder tree, open files into main-window tabs, edit and save file contents with KTextEditor, create or delete files and folders, and switch back to AI chat without losing context.

## User Outcomes

- The user can switch the left column between repository context and a folder tree.
- The user can open a file from the folder tree into a tab in the main work area.
- The user can keep AI chat open as a permanent tab while working in file tabs.
- The user can create, rename, and delete files and folders under the active repository root.
- The user gets save prompts for dirty editor tabs before closing files, switching projects, or quitting.

## Non-Goals

- No Electron, browser UI, Monaco, or VS Code compatibility layer.
- No language server, debugger, extension host, or project-wide refactoring system.
- No multi-root workspace model in the first slice.
- No binary-file editing.
- No automatic agent edits through the editor surface; agent mutations continue through existing artifact/diff flows.

## Layout

Target shell:

```text
+----------------------+--------------------------------+
| Left Project Space   | Main Work Tabs                 |
|                      | [AI Chat*] [file.cpp] [...]    |
| Repository Context   |                                |
| or                   | Active tab content             |
| Folder Tree          |                                |
|                      +--------------------------------+
|                      | Terminal Panel                 |
+----------------------+--------------------------------+
| Status bar: repository, editor, agent, memory, and terminal state |
+-------------------------------------------------------------------+
```

`AI Chat*` is permanent and non-closable. File tabs are closable.

The existing right activity panel may stay unchanged for agent sessions, retrieved context, generated changes, and MCP servers.

## Left Project Space

The left column should continue to be the source of project context. Replace the single always-visible `RepositoryPanel` content with a shared project-navigation container:

- `Repository` view: existing local repositories, changed files, commits, GitHub issues, pull requests, and detail attach flows.
- `Files` view: active repository folder tree rooted at the active project path.

The selector can be a compact tab bar, segmented control, or stacked view controlled by actions. It must reuse the same left-column width and persisted splitter settings.

## Folder Tree Behavior

The folder tree must:

- Track the active project from `ProjectManager`.
- Root itself at the active repository path.
- Hide or de-emphasize entries outside the active repository root.
- Open regular text files on activation.
- Show clear empty/error states when no project is active or the path is unavailable.
- Refresh when filesystem operations complete.

Expected operations:

- New file.
- New folder.
- Rename.
- Delete.
- Reveal/open containing folder can be deferred unless it already fits local patterns.

All operations must validate that resolved paths remain inside the active repository root.

## Main Work Tabs

Introduce a main work-area tab widget or wrapper component that owns:

- One permanent AI chat tab containing `AgentPanel::conversationPanel()`.
- Zero or more editor tabs, each mapped to one canonical local file path.

Required tab behavior:

- Opening an already-open file focuses its existing tab.
- Opening a new file creates a KTextEditor document and view.
- Dirty tabs show a modified indicator.
- Closing a dirty file asks whether to save, discard, or cancel.
- The AI chat tab cannot be closed by tab close buttons, keyboard close commands, or project-switch cleanup.
- On active project change, dirty file tabs prompt before closing or re-rooting the workspace.

## Editor Behavior

Use KTextEditor for file contents:

- Create one `KTextEditor::Document` per open file tab.
- Create a `KTextEditor::View` for the active tab's widget.
- Use KTextEditor's normal load/save APIs.
- Surface load/save failures through `StatusService` and a local dialog where user action is needed.
- Preserve KTextEditor undo/redo behavior within each document.
- Avoid custom text-buffer implementations.

Binary or unsupported files:

- Detect obvious binary files before loading when practical.
- Refuse to open unsupported content in the editor tab and show a concise status/dialog message.

## Actions And Menus

Add `KActionCollection` actions where they support shared menu, toolbar, shortcut, or future command-palette wiring:

- New File.
- New Folder.
- Save.
- Save As only if KTextEditor integration makes it straightforward; otherwise defer.
- Close File Tab.
- Rename.
- Delete.
- Show Repository View.
- Show Files View.

Panel buttons and context-menu items should reuse the same slots/actions when practical.

## Services And Ownership

Keep the UI thin:

- `ProjectManager` remains the active-project source.
- `StatusService` remains the user-facing status path.
- `MainWindow` owns top-level placement and shared actions.
- A new editor/workspace component should own open tab state.
- A file-operation helper or service should own path validation and filesystem mutations if those operations become more than trivial widget code.

Suggested classes:

- `ProjectNavigationPanel`: contains repository view and file tree view for the left column.
- `FileTreePanel`: displays active repository filesystem and emits file/folder operation requests.
- `WorkspaceTabs`: owns permanent chat tab and editor tabs.
- `EditorTab`: wraps a KTextEditor document/view pair for one file.
- `FileOperationService`: optional helper for create, rename, delete, and project-root path validation.

Names may change during implementation, but the ownership boundaries should remain.

## Dependency Updates

The implementation must update:

- CMake dependency discovery for KTextEditor.
- `QtCodeExternalDependencies` link targets.
- `scripts/check-toolchain`.
- `docs/toolchain-requirements.md`.
- `docs/engineering/beta-setup-guide.md`.
- `docs/engineering/packaging-notes.md`.

Ubuntu/Debian package naming should be verified during implementation. The likely development package is `libkf6texteditor-dev`.

## Acceptance Criteria

- A Files view appears in the same left panel space as repository context.
- The AI chat appears as a permanent, non-closable main work-area tab.
- Opening a text file from the folder tree opens or focuses a main work-area editor tab.
- Editing and saving a file changes the file on disk.
- Dirty editor tabs prompt before close, project switch, and application quit.
- Creating, renaming, and deleting files/folders works only under the active repository root.
- Destructive deletes require confirmation.
- Existing repository, GitHub context attach, right activity panel, terminal tabs, and status bar behavior continue to work.
- Build, targeted tests, and smoke tests pass.

## Issue Plan

- [Issue 001: Add KTextEditor dependency and toolchain documentation](../issues/ktexteditor-workspace/001-ktexteditor-dependency.md)
- [Issue 002: Introduce main workspace tabs with permanent AI chat](../issues/ktexteditor-workspace/002-workspace-tabs.md)
- [Issue 003: Add shared repository/files left project space](../issues/ktexteditor-workspace/003-project-navigation-file-tree.md)
- [Issue 004: Implement KTextEditor file tabs and save lifecycle](../issues/ktexteditor-workspace/004-editor-tabs-save-lifecycle.md)
- [Issue 005: Implement project-root-scoped file and folder operations](../issues/ktexteditor-workspace/005-file-folder-operations.md)
- [Issue 006: Add tests, smoke coverage, and documentation alignment](../issues/ktexteditor-workspace/006-tests-and-docs.md)
