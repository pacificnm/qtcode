#!/usr/bin/env python3
"""Cursor hooks for automatic QTCode memory search and context persistence."""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

from agent_context import save_agent_context
from memory_retrieval import search_agent_context_memory, search_project_memory


PROJECT_MEMORY_QUERY = (
    "QTCode architecture milestones ADRs implementation tasks engineering specs"
)
AGENT_CONTEXT_QUERY = (
    "recent decisions task progress blockers next steps compaction summary"
)


def load_hook_input() -> dict:
    if sys.stdin.isatty():
        return {}
    return json.load(sys.stdin)


def scope_key_from_input(data: dict) -> str:
    roots = data.get("workspace_roots") or []
    if roots:
        return str(roots[0])
    return str(ROOT)


def extract_transcript_text(transcript_path: str | None, max_chars: int = 12000) -> str:
    if not transcript_path:
        return ""

    path = Path(transcript_path)
    if not path.is_file():
        return ""

    parts: list[str] = []
    for raw_line in path.read_text(encoding="utf-8", errors="ignore").splitlines():
        raw_line = raw_line.strip()
        if not raw_line:
            continue

        try:
            event = json.loads(raw_line)
        except json.JSONDecodeError:
            continue

        role = event.get("role")
        message = event.get("message") or {}
        content = message.get("content")
        if not isinstance(content, list):
            continue

        for block in content:
            if not isinstance(block, dict) or block.get("type") != "text":
                continue

            text = str(block.get("text", "")).strip()
            if not text:
                continue

            text = re.sub(r"</?user_query>", "", text).strip()
            if role == "user":
                parts.append(f"User: {text}")
            elif role == "assistant":
                parts.append(f"Assistant: {text}")

    if not parts:
        return ""

    combined = "\n\n".join(parts)
    if len(combined) <= max_chars:
        return combined

    return combined[-max_chars:]


def session_start(data: dict) -> dict:
    scope_key = scope_key_from_input(data)

    sections = [
        (
            "Project memory",
            lambda: search_project_memory(PROJECT_MEMORY_QUERY, limit=6),
        ),
        (
            "Agent context memory",
            lambda: search_agent_context_memory(
                AGENT_CONTEXT_QUERY,
                scope_key,
                session_id=data.get("session_id"),
                limit=6,
            ),
        ),
    ]

    context_blocks = [
        "# QTCode memory bootstrap",
        "",
        "Use MCP tools `search_project_memory`, `search_agent_context`, and "
        "`save_agent_context` throughout the session.",
        f"Active scope_key: {scope_key}",
        "",
    ]

    for title, fetch in sections:
        try:
            body = fetch()
        except Exception as error:
            body = f"Unavailable: {error}"

        context_blocks.extend([f"## {title}", "", body, ""])

    return {"additional_context": "\n".join(context_blocks).strip()}


def pre_compact(data: dict) -> dict:
    scope_key = scope_key_from_input(data)
    transcript_text = extract_transcript_text(data.get("transcript_path"))

    if not transcript_text:
        return {}

    title = f"Compaction ({data.get('trigger', 'auto')})"
    try:
        save_agent_context(
            transcript_text,
            scope_key,
            session_id=data.get("session_id"),
            context_type="compaction",
            title=title,
            metadata={
                "trigger": data.get("trigger"),
                "message_count": data.get("message_count"),
                "messages_to_compact": data.get("messages_to_compact"),
            },
        )
    except Exception as error:
        return {"user_message": f"Could not save agent context before compaction: {error}"}

    return {
        "user_message": "Saved conversation context to agent memory before compaction."
    }


def stop(data: dict) -> dict:
    if data.get("status") != "completed":
        return {}

    scope_key = scope_key_from_input(data)
    transcript_text = extract_transcript_text(data.get("transcript_path"), max_chars=8000)
    if not transcript_text:
        return {}

    try:
        save_agent_context(
            transcript_text,
            scope_key,
            session_id=data.get("session_id"),
            context_type="summary",
            title="Session summary",
        )
    except Exception:
        return {}

    return {}


def main() -> int:
    command = sys.argv[1] if len(sys.argv) > 1 else ""
    data = load_hook_input()

    if command == "session-start":
        print(json.dumps(session_start(data)))
        return 0

    if command == "pre-compact":
        print(json.dumps(pre_compact(data)))
        return 0

    if command == "stop":
        print(json.dumps(stop(data)))
        return 0

    print(json.dumps({}))
    return 1


if __name__ == "__main__":
    sys.exit(main())
