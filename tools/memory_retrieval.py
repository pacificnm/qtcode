"""Shared retrieval helpers for project and agent context memory."""

from __future__ import annotations

from agent_context import format_search_results, search_agent_context
from memory_common import database_url, embed_text_literal


def search_project_memory(query: str, limit: int = 6) -> str:
    """Return formatted project-memory search results."""
    import psycopg

    normalized_query = query.strip()
    if not normalized_query:
        return "No project memory query provided."

    embedding = embed_text_literal(normalized_query)

    with psycopg.connect(database_url()) as conn:
        rows = conn.execute(
            """
            SELECT source_path, content
            FROM project_memory
            ORDER BY embedding <=> %s::vector
            LIMIT %s
            """,
            (embedding, limit),
        ).fetchall()

    if not rows:
        return "No matching project memory found."

    output = []
    for source_path, content in rows:
        output.append(f"--- {source_path} ---\n{content[:2000]}")

    return "\n\n".join(output)


def search_agent_context_memory(
    query: str,
    scope_key: str,
    *,
    session_id: str | None = None,
    limit: int = 6,
) -> str:
    """Return formatted agent-context search results."""
    rows = search_agent_context(
        query,
        scope_key,
        session_id=session_id,
        limit=limit,
    )
    return format_search_results(rows)
