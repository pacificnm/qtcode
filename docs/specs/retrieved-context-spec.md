# Retrieved Context Specification

Related docs:

- [M4 milestone](../milestones/m04-mcp-memory-context-pipeline.md)
- [MCP integration](../engineering/mcp-integration.md)
- [UI layout spec](../design/ui-layout-spec.md)
- [Interaction spec](../design/interaction-spec.md)

## Purpose

The **Retrieved Context** right-panel view lets users inspect, attach, and detach context excerpts before they are included in the next agent prompt. It combines automatic project-memory retrieval with manual file attachments from the repository panel, folder tree, and editor tabs.

## Current State (2026-06-21)

The panel is implemented in `ContextResultsView` and wired through `AgentPanel`. Behavior described here matches the current codebase on `main` plus the in-progress Retrieved Context hardening work (dedupe, attach/detach fixes, manual file context).

### Panel layout

- **Header** — `AgentPanel` shows the section title **Retrieved Context**. The inner view does not repeat the title.
- **Status label** — compact empty/error copy above the list (memory unavailable, no matches, or saved-retrieval audit details).
- **Result list** — one row per deduplicated context source. Each row is checkable and shows `[source_type] title (score)`; detached rows are prefixed with `(-)`.
- **Controls** — **Attach all**, **Detach all**, **Attach selected**, **Detach selected**. Tooltips explain that these choices apply to the **next** agent prompt.

There is no inline excerpt preview pane. Clicking a list row that resolves to a project file opens that file in a KTextEditor workspace tab.

### Source types

`memory::ContextResult` normalizes all excerpts. List labels use `memory::contextSourceTypeLabel()`:

| Type | Label | Origin |
| --- | --- | --- |
| `ProjectMemory` | `project_memory` | MCP/RAG search via `ContextManager` |
| `AgentContext` | `agent_context` | Saved agent context search (reserved) |
| `Documentation` | `documentation` | Documentation search (reserved) |
| `ArchitectureDecision` | `architecture_decision` | ADR retrieval (reserved) |
| `CodingStandard` | `coding_standard` | Standards retrieval (reserved) |
| `SourceFile` | `source_file` | Manual **Add to Context** from UI |

Manual file entries use a project-relative `sourceUri` when the file is under the active repository root.

### Automatic retrieval flow

1. User sends a prompt from the AI Chat composer.
2. `ContextManager::retrieveForAgentRequest()` runs asynchronously.
3. Results are deduplicated with `memory::dedupeContextResultsBySource()` in both `ContextManager` and `ContextResultsView`.
4. `ContextResultsView::setRetrievalOutcome()` rebuilds the list. New memory hits default to **attached** unless the user previously detached that source key.
5. The prompt dispatches immediately with `attachedContextExcerpts()` from the view (checked rows only).
6. `AgentPanel` persists retrieval metadata for the session without reloading the live view (so attach/detach controls stay enabled).

Dedupe rules (`memory::dedupeContextResultsBySource`):

- Key = `sourceUri`, or `title` when `sourceUri` is empty.
- One list row per key; highest score wins.
- Multiple excerpts for the same source are merged with a blank line separator when they are not already contained in the existing excerpt.

### Manual file attachment

**Add to Context** is available from:

- **Repository** view — changed-files list context menu (readable files only; multi-select adds all addable paths).
- **Files** view — folder tree context menu.
- **Editor tabs** — tab bar context menu on file tabs.

Implementation path:

```text
FileTreePanel / RepositoryPanel / WorkspaceTabs
  -> MainWindow
  -> AgentPanel::attachFileContext()
  -> ContextResultsView::addFileContext()
  -> memory::contextResultFromFilePath()
```

Rules:

- Readable regular files only; binary files are rejected.
- Content is capped at 64 KiB; longer files are truncated with `…`.
- Editor tabs pass unsaved buffer text when the document is modified.
- Re-adding the same file refreshes its excerpt and re-attaches it.
- Manual `SourceFile` entries are preserved across subsequent memory retrievals.

### Attach and detach semantics

- Checkboxes and the four attach/detach buttons control inclusion in the **next** prompt only.
- Attachment state is tracked per source key and survives new memory searches.
- GitHub issue/PR attach flows still merge separate excerpts at dispatch time through `AgentPanel`; they do not appear in the Retrieved Context list.
- After dispatch, the status bar reports how many excerpts were sent.

### Session restore and audit

When the user switches agent sessions, `AgentPanel::refreshSavedContextRetrieval()` loads the latest saved retrieval from SQLite into `setPersistedRetrieval()`. This is an audit/history view of what was attached at send time, not a live editing surface for the next prompt. Controls remain enabled so users can adjust attachment before the next send.

### Failure modes

| Condition | UI behavior |
| --- | --- |
| MCP/memory unavailable | Status label explains the error; prompt still sends without memory excerpts. |
| No matching memory | Status label: “No matching project memory found.” |
| Empty list | View disabled until results or manual attachments exist. |
| Unreadable manual file | Status bar error; file is not added. |

Memory failures are non-fatal. Direct agent usage continues.

## Recent Changes

| Date | Change |
| --- | --- |
| 2026-06-20 | Removed redundant in-view title label; panel tab/header is the only title. |
| 2026-06-20 | Removed bottom excerpt preview; clicking a file-backed result opens a KTextEditor tab. |
| 2026-06-21 | Deduplicate memory chunks per source via `dedupeContextResultsBySource`. |
| 2026-06-21 | Fixed disabled attach/detach buttons after persist by not reloading saved retrieval after send. |
| 2026-06-21 | Preserve attach/detach choices across retrievals by source key. |
| 2026-06-21 | Added **Add to Context** from repository changed files, folder tree, and editor tabs (`SourceFile` type). |
| 2026-06-21 | Added unit tests in `tests/unit/memory/ContextResultTest.cpp`. |

## Engineering Components

| Component | Responsibility |
| --- | --- |
| `ContextManager` | Builds retrieval query, calls `MemoryService`, dedupes results, produces excerpts. |
| `ContextResult` / `ContextResult.cpp` | Source types, dedupe, MCP output parsing, file-to-context conversion. |
| `ContextResultsView` | List UI, attach/detach state, manual file add, file-open on click. |
| `AgentPanel` | Retrieval orchestration on send, dispatch with attached excerpts, persist metadata, session restore. |
| `ContextRetrievalRepository` | SQLite storage for retrieval audit records. |

## Verification

- Unit: `tests/unit/memory/ContextResultTest.cpp` (dedupe and file context).
- Manual: send prompt with memory enabled; confirm single row per source, attach/detach before next send, **Add to Context** from tree/changed files/editor tab, click-to-open file tab.

## Out Of Scope

- Inline excerpt editing in the panel.
- Attaching binary files or paths outside the active repository root (except absolute fallback when relative path cannot be computed).
- Replacing the PostgreSQL/pgvector memory backend.
- Showing GitHub issue/PR attachments in the same list (they remain composer-level context).
