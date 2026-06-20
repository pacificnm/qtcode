"""Save and search agent context memory in PostgreSQL/pgvector."""

from __future__ import annotations

import hashlib
import json
from typing import Any

from memory_common import database_url, embed_text, vector_literal

CONTEXT_TYPES = {
    "note",
    "decision",
    "summary",
    "compaction",
    "task_state",
}


def content_hash(scope_key: str, session_id: str | None, content: str) -> str:
    session_part = session_id or ""
    return hashlib.sha256(f"{scope_key}:{session_part}:{content}".encode()).hexdigest()


def save_agent_context(
    content: str,
    scope_key: str,
    *,
    session_id: str | None = None,
    context_type: str = "note",
    title: str | None = None,
    metadata: dict[str, Any] | None = None,
) -> str:
    """Persist one agent context chunk and return a status message."""
    import psycopg

    normalized_content = content.strip()
    if not normalized_content:
        raise ValueError("Content must not be empty.")
    if not scope_key.strip():
        raise ValueError("scope_key must not be empty.")

    normalized_type = context_type.strip().lower() or "note"
    if normalized_type not in CONTEXT_TYPES:
        allowed = ", ".join(sorted(CONTEXT_TYPES))
        raise ValueError(f"context_type must be one of: {allowed}")

    digest = content_hash(scope_key.strip(), session_id, normalized_content)
    embedding = vector_literal(embed_text(normalized_content))
    metadata_json = json.dumps(metadata or {})

    with psycopg.connect(database_url()) as conn:
        result = conn.execute(
            """
            INSERT INTO agent_context_memory
              (scope_key, session_id, context_type, title, content, content_hash, embedding, metadata_json)
            VALUES
              (%s, %s, %s, %s, %s, %s, %s::vector, %s::jsonb)
            ON CONFLICT (content_hash) DO NOTHING
            RETURNING id
            """,
            (
                scope_key.strip(),
                session_id,
                normalized_type,
                title,
                normalized_content,
                digest,
                embedding,
                metadata_json,
            ),
        ).fetchone()
        conn.commit()

    if result is None:
        return "Agent context already stored for this scope and content."

    return f"Saved agent context entry {result[0]} ({normalized_type})."


def search_agent_context(
    query: str,
    scope_key: str,
    *,
    session_id: str | None = None,
    limit: int = 8,
) -> list[tuple[str, str | None, str, str, str]]:
    """Search agent context memory and return matching rows."""
    import psycopg

    normalized_query = query.strip()
    if not normalized_query:
        raise ValueError("Query must not be empty.")
    if not scope_key.strip():
        raise ValueError("scope_key must not be empty.")

    embedding = vector_literal(embed_text(normalized_query))

    sql = """
        SELECT context_type, title, content, session_id, created_at::text
        FROM agent_context_memory
        WHERE scope_key = %s
    """
    params: list[Any] = [scope_key.strip()]

    if session_id:
        sql += " AND (session_id = %s OR session_id IS NULL)"
        params.append(session_id)

    sql += """
        ORDER BY embedding <=> %s::vector
        LIMIT %s
    """
    params.extend([embedding, limit])

    with psycopg.connect(database_url()) as conn:
        return conn.execute(sql, params).fetchall()


def format_search_results(rows: list[tuple[str, str | None, str, str, str]]) -> str:
    if not rows:
        return "No matching agent context found."

    output = []
    for context_type, title, content, session_id, created_at in rows:
        heading = title or context_type
        session_label = session_id or "project-wide"
        output.append(
            f"--- {heading} ({context_type}, {session_label}, {created_at}) ---\n{content[:2000]}"
        )

    return "\n\n".join(output)
