#!/usr/bin/python3

import argparse
import sys
from pathlib import Path

from memory_common import database_url, load_openai_api_key, vector_literal


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Search QTCode project memory through PostgreSQL and pgvector."
    )
    parser.add_argument("query", nargs="*", help="Search text to embed and query")
    parser.add_argument(
        "--limit",
        type=int,
        default=8,
        help="Maximum number of matching snippets to return",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    query = " ".join(args.query).strip()

    if not query:
        print(
            'Usage: /usr/bin/python3 tools/search_memory.py "your query"',
            file=sys.stderr,
        )
        return 1

    try:
        import psycopg
        from openai import OpenAI

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
                (vector_literal(embedding), args.limit),
            ).fetchall()
    except Exception as error:
        print(f"ERROR: QTCode memory search failed: {error}", file=sys.stderr)
        return 1

    for source_path, content in rows:
        print(f"\n--- {source_path} ---\n")
        print(content[:2000])

    return 0


if __name__ == "__main__":
    sys.exit(main())
