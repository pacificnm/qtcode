# ADR 0014: Build A Cursor-Style Native Agent Chat Renderer

## Status

Accepted

## Context

QTCode is an AI-first developer cockpit. The AI Chat tab is a primary workflow surface, not a plain log view. The UI layout spec already defines a Cursor app chat-inspired transcript: turn-based messages, right-aligned user prompt bubbles, full-width assistant prose, typed tool cards, collapsible activity groups, sticky active prompt, and streaming-first updates.

The current implementation is fragile. It uses a `QScrollArea` with hand-built turn widgets and rich `QLabel` blocks whose widths and heights are adjusted manually during resize and streaming. This has caused truncation, disappearing content, inconsistent wrapping, and layout behavior that is hard for agents to fix safely. Falling back to a dull plain transcript would violate the product direction; the correct fix is to keep the Cursor-style experience and make its native Qt rendering foundation deterministic.

## Decision

Implement the AI Chat transcript as a **Cursor-style native Qt/KDE chat renderer** with explicit widget responsibilities and robust sizing contracts.

The renderer must preserve the visual and interaction target:

- Turn-based transcript grouped by user prompt.
- Right-aligned user prompt bubble.
- Full-width assistant prose without a bubble background.
- Typed tool cards for shell, file, search, and generic activity.
- Collapsible step groups for consecutive activity messages.
- Completed-turn collapse and dimming while a request is running.
- Sticky active prompt overlay while scrolling through long responses.
- Streaming updates that keep the scroll pinned only when the user is already at the bottom.
- Native KDE palette styling, not a Chromium/Electron surface.

The renderer must also stop relying on fragile layout tricks:

- Do not implement message bodies as generic rich `QLabel` instances whose widths and heights are mutated after layout.
- Do not call ad hoc `adjustSize()` on the scroll content during the streaming path.
- Do not rely on browser-only CSS features in Qt rich text for core wrapping and sizing.
- Do not add nested scroll areas for individual message blocks.

Instead, introduce dedicated rendering widgets with stable size hints:

- `ConversationView` owns transcript scroll state, bottom-pinning, empty state, and sticky prompt placement.
- `ConversationTurnWidget` owns one turn's composition and collapse state.
- `UserPromptBubble` owns prompt bubble layout and sizing.
- `AssistantMessageBlock` owns assistant prose rendering and sizing.
- `CodeBlock` owns code/fenced-block rendering, monospace wrapping, copy affordances when added, and sizing.
- `ToolActivityCard` owns shell/file/search/generic activity display.
- `ActivityGroup` owns collapsible grouped tool activity.

Names may change during implementation, but the ownership boundaries must remain.

## Consequences

Positive:

- Preserves the intended Cursor-inspired AI workflow instead of regressing to a plain transcript.
- Makes the most important QTCode surface reliable during real agent output.
- Gives agents and humans smaller components to reason about when fixing display issues.
- Enables targeted tests for each rendering primitive and for the full transcript.

Tradeoffs:

- Requires refactoring the current chat widgets instead of patching one-off truncation bugs.
- More UI components are needed than a simple flat text view.
- Screenshot or visual smoke coverage becomes important because correctness is visual, not only structural.

## Implementation Notes

- Keep the transcript native Qt widgets. Do not introduce Electron, Chromium, Monaco, web views, or a browser runtime.
- Prefer `QTextDocument`-backed custom widgets, read-only `QTextEdit`/`QTextBrowser` configured without internal scrollbars, or explicit custom painting where needed for reliable sizing.
- Each message-rendering widget must own its `sizeHint()` and height-for-width behavior rather than having a parent force widths and heights after the fact.
- Code blocks should be rendered as separate blocks from assistant prose so long code and prose do not fight one rich-text label.
- Sticky prompt geometry should be tied to the transcript viewport and content margins, not the outer tab width.
- Streaming updates should update only the active turn/message where possible, without rebuilding stable prior turns.
- Tests must include narrow widths, long unbroken paths, long code blocks, streaming updates, sticky prompt behavior, and the full AI Chat tab inside `WorkspaceTabs`.

## Follow-Ups

- Create implementation issues from the Cursor-style chat spec.
- Add screenshot or pixel-based smoke coverage for the AI Chat tab.
- Update the UI layout spec only when the implementation settles final class names.

## Related Documents

- [UI layout spec: Conversation transcript](../design/ui-layout-spec.md#conversation-transcript)
- [Cursor-style agent chat specification](../specs/cursor-style-chat-spec.md)
- [ADR 0002: Keep QTCode terminal-first and agent-first](0002-terminal-and-agent-first-product-shape.md)
