#!/usr/bin/python3

from pathlib import Path

try:
    import psycopg
    from mcp.server.fastmcp import FastMCP
except ModuleNotFoundError as error:
    raise SystemExit(
        "Missing Python dependency for QTCode memory MCP. "
        "Run it with /usr/bin/python3 or install the project Python dependencies."
    ) from error

from memory_common import database_url, vector_literal


mcp = FastMCP("qtcode-memory")


@mcp.tool()
def search_project_memory(query: str, limit: int = 8) -> str:
    """Search QTCode project memory for specs, decisions, issues, and build notes."""
    from openai import OpenAI
    from memory_common import load_openai_api_key

    api_key = load_openai_api_key()
    if not api_key:
        raise RuntimeError(
            f"Missing OpenAI API key. Set OPENAI_API_KEY or create {Path.home() / '.openAi' / 'key'}."
        )

    client = OpenAI(api_key=api_key)
    embedding = client.embeddings.create(
        model="text-embedding-3-small",
        input=query,
    ).data[0].embedding

    with psycopg.connect(database_url()) as conn:
        rows = conn.execute(
            """
            SELECT source_path, content
            FROM project_memory
            ORDER BY embedding <=> %s::vector
            LIMIT %s
            """,
            (vector_literal(embedding), limit),
        ).fetchall()

    if not rows:
        return "No matching project memory found."

    output = []
    for source_path, content in rows:
        output.append(f"--- {source_path} ---\n{content[:2000]}")

    return "\n\n".join(output)


if __name__ == "__main__":
    mcp.run()
