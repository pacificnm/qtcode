# Cursor-Style Agent Chat Specification

ADR: [ADR 0014: Build a Cursor-style native agent chat renderer](../adrs/0014-cursor-style-agent-chat-renderer.md)

Related docs:

- [UI layout spec](../design/ui-layout-spec.md)
- [Interaction spec](../design/interaction-spec.md)
- [Agent plugin architecture](../engineering/plugin-agent-architecture.md)

## Purpose

Define the implementation target for QTCode's AI Chat tab: a polished Cursor app chat-inspired transcript and composer that remains native Qt/KDE and is robust under real streaming agent output.

This spec rejects two failure modes:

- A fragile rich UI that clips, disappears, or truncates when content changes.
- A plain boring transcript that ignores the Cursor-style chat experience already chosen for QTCode.

The goal is a rich, reliable, native chat surface.

## User Outcomes

- The user can read long agent responses without clipped or disappearing content.
- The user can follow an agent turn as a cohesive unit: prompt, tool activity, assistant prose, and completion summary.
- The user can scroll through a long active response while the originating prompt remains visible as a sticky overlay.
- The user can distinguish shell, file, search, and generic activity at a glance.
- The user can keep working during streaming without the transcript jumping unless they are already pinned to the bottom.
- The chat feels close to Cursor's agent chat patterns while fitting KDE/Breeze styling.

## Visual Target

The AI Chat transcript follows these rules:

- **Turn-based transcript:** one user prompt starts one turn; assistant prose and tool activity attach to that turn until the next user prompt.
- **User prompt bubble:** right-aligned, palette-backed, subtle border, enough width for real prompts, no truncation.
- **Assistant prose:** full-width content block, no bubble background, readable markdown-lite formatting.
- **Code blocks:** separate monospace blocks with reliable wrapping/sizing and no nested scrollbars in the first implementation.
- **Tool cards:** shell, file, search, and generic activity display as typed cards with compact header and optional monospace detail.
- **Activity groups:** consecutive activity messages group into collapsible step sections.
- **Turn collapse:** while a request is running, completed prior turns collapse to summary rows and dim; the active turn remains expanded.
- **Turn footer:** completed expanded turns show a quiet summary such as `Worked · 3 steps · 1 reply`.
- **Sticky prompt:** the active turn's prompt pins at the top of the transcript viewport when the prompt has scrolled above view.
- **Composer stack:** prompt input spans full width; model/mode controls and Send/Stop stay below it.

## Non-Goals

- No Electron, Chromium, web view, Monaco, or browser-runtime dependency.
- No pixel-perfect Cursor clone.
- No generic plain log transcript as the primary UI.
- No nested scroll areas inside individual message blocks.
- No full markdown engine requirement in the first pass.
- No virtualization in the first pass unless performance measurements require it.

## Component Responsibilities

### `ConversationView`

Owns the full transcript viewport.

Responsibilities:

- Host exactly one outer scroll surface for the transcript.
- Maintain scroll-bottom pinning state.
- Preserve user scroll position when the user has scrolled up.
- Place and size the sticky prompt overlay relative to the transcript viewport.
- Show empty state when the active session has no messages.
- Route message updates to the relevant turn widget.

Must not:

- Force child message widths and heights directly.
- Call ad hoc content `adjustSize()` during streaming.
- Rebuild stable prior turns for every streamed token.

### `ConversationTurnWidget`

Owns one conversation turn.

Responsibilities:

- Compose one `UserPromptBubble`, zero or more activity groups/cards, zero or more assistant blocks, and the footer.
- Own collapsed/expanded state for the turn.
- Apply dimming for prior turns while a request is running.
- Expose the user prompt bubble geometry for sticky prompt decisions.

Must not:

- Render all rich content in one generic label.
- Mutate child geometry after layout to compensate for incorrect size hints.

### `UserPromptBubble`

Owns prompt presentation.

Responsibilities:

- Right-align the prompt bubble within the turn.
- Wrap text to available bubble width.
- Report a stable size hint and height-for-width behavior.
- Support selectable text.
- Use palette colors compatible with Breeze light and dark themes.

### `AssistantMessageBlock`

Owns assistant prose.

Responsibilities:

- Render markdown-lite prose reliably.
- Wrap long words/paths without forcing horizontal scrolling.
- Size to full available width and full required height.
- Support selectable text and links where applicable.

### `CodeBlock`

Owns fenced and inline code presentation where separated from prose.

Responsibilities:

- Use monospace font and compact padding.
- Wrap long lines in v1 so the transcript has one outer scroll surface.
- Preserve line breaks.
- Report reliable height for width.
- Later support copy button and language label without changing layout ownership.

### `ToolActivityCard`

Owns a single activity message.

Responsibilities:

- Map activity prefix to card kind.
- Show compact header and optional detail.
- Use monospace detail text.
- Truncate only intentional summaries, never assistant prose.

Activity mapping:

| Prefix | Type | Header |
| --- | --- | --- |
| `Running:`, `Ran:` | Shell | Shell |
| `Read:`, `Write:`, `Edit:`, `Delete:`, `List:` | File | Verb |
| `Grep:`, `Glob:` | Search | Verb |
| Other | Generic | Verb |

### `ActivityGroup`

Owns consecutive activity cards.

Responsibilities:

- Start collapsed for grouped activity unless product rules say otherwise.
- Provide a compact title such as `3 steps` or `2 commands`.
- Expand without changing scroll state unexpectedly.

## Rendering Rules

- Size flows from parent layout to child `sizeHint()` and height-for-width calculations.
- Message body widgets must own their sizing contract.
- Styling should use Qt/KDE palette values and object names, not hard-coded theme colors.
- Qt rich text may be used only for features it supports predictably.
- Browser-specific CSS must not be required for core wrapping or sizing.
- Long unbroken strings must wrap or break within the available width.
- No message content may be clipped vertically.
- Horizontal scrolling is disabled for the transcript in v1.

## Streaming Rules

- A running request updates the active turn incrementally.
- Existing prior turns are not rebuilt unless their content actually changes.
- When the user is at the bottom, streamed content keeps the view pinned to the bottom.
- When the user scrolls up, streamed content must not steal position.
- Sticky prompt visibility updates after layout settles, not before child widgets have valid geometry.
- Collapsing prior turns while running must happen once per state transition, not on every token.

## Composer Rules

- Prompt input remains a multi-line `QPlainTextEdit`.
- Input spans full composer width.
- Model and execution-mode controls live below the input.
- Send becomes Stop/Cancel while the active request is running.
- Selector labels must not steal unbounded horizontal width from the composer row.
- Enter sends; Shift+Enter inserts a newline.

## Test Requirements

Unit and UI tests must cover:

- 320 px, 640 px, and wide transcript widths.
- Long user prompts.
- Long assistant prose.
- Long unbroken file paths and command strings.
- Fenced code blocks of at least 200 lines.
- Streaming append to the active assistant message.
- Activity grouping and expansion.
- Prior-turn collapse/dim while active request is running.
- Sticky prompt visibility and non-overlap.
- AI Chat embedded as the permanent tab in `WorkspaceTabs`.
- Teardown with a loaded conversation.

Visual or screenshot smoke coverage must verify:

- Transcript is nonblank after messages load.
- Last streamed assistant line is visible at bottom when pinned.
- Composer remains visible.
- Sticky prompt does not cover the composer or unrelated content.
- Narrow width does not truncate message bodies.

## Acceptance Criteria

- The AI Chat tab visually follows the Cursor-style transcript rules from this spec.
- Long prose and code blocks do not disappear, clip, or truncate vertically.
- There is only one outer transcript scroll surface.
- Streaming preserves bottom pinning and user scroll position correctly.
- Sticky prompt works without overlapping the composer.
- Tool activity renders as typed cards and grouped collapsible sections.
- The implementation has focused tests for layout, streaming, and visual edge cases.
- The current fragile manual width/height adjustment pattern is removed from chat message rendering.
