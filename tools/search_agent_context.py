#!/usr/bin/python3

import argparse
import sys


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Search saved agent context memory through PostgreSQL and pgvector."
    )
    parser.add_argument(
        "--scope-key",
        required=True,
        help="Project or repository scope, such as a repo path or project id",
    )
    parser.add_argument(
        "--session-id",
        help="Optional session id; includes project-wide context when set",
    )
    parser.add_argument(
        "--limit",
        type=int,
        default=8,
        help="Maximum number of matching snippets to return",
    )
    parser.add_argument("query", nargs="*", help="Search text to embed and query")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    query = " ".join(args.query).strip()

    if not query:
        print(
            'Usage: search_agent_context.py --scope-key /path/to/repo "your query"',
            file=sys.stderr,
        )
        return 1

    try:
        from agent_context import format_search_results, search_agent_context

        rows = search_agent_context(
            query,
            args.scope_key,
            session_id=args.session_id,
            limit=args.limit,
        )
        print(format_search_results(rows))
    except Exception as error:
        print(f"ERROR: agent context search failed: {error}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
