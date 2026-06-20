#!/usr/bin/python3

from pathlib import Path

try:
    from mcp.server.fastmcp import FastMCP
except ModuleNotFoundError as error:
    raise SystemExit(
        "Missing Python dependency for QTCode memory MCP. "
        "Run it with /usr/bin/python3 or install the project Python dependencies."
    ) from error

from agent_context import (
    format_search_results,
    save_agent_context as persist_agent_context,
    search_agent_context as query_agent_context,
)
from memory_common import database_url, embed_text_literal, load_openai_api_key


mcp = FastMCP("qtcode-memory")


@mcp.tool()
def search_project_memory(query: str, limit: int = 8) -> str:
    """Search QTCode project memory for specs, decisions, issues, and build notes."""
    import psycopg

    if not load_openai_api_key():
        raise RuntimeError(
            f"Missing OpenAI API key. Set OPENAI_API_KEY or create {Path.home() / '.openAi' / 'key'}."
        )

    embedding = embed_text_literal(query)

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


@mcp.tool()
def save_agent_context(
    content: str,
    scope_key: str,
    session_id: str | None = None,
    context_type: str = "note",
    title: str | None = None,
) -> str:
    """Save agent context for long-term retrieval after conversation compaction."""
    return persist_agent_context(
        content,
        scope_key,
        session_id=session_id,
        context_type=context_type,
        title=title,
    )


@mcp.tool()
def search_agent_context(
    query: str,
    scope_key: str,
    session_id: str | None = None,
    limit: int = 8,
) -> str:
    """Search saved agent context for decisions, summaries, and prior task state."""
    rows = query_agent_context(
        query,
        scope_key,
        session_id=session_id,
        limit=limit,
    )
    return format_search_results(rows)


if __name__ == "__main__":
    mcp.run()
