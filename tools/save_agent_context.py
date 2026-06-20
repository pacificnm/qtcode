#!/usr/bin/python3

import argparse
import sys


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Save agent context memory for long-term retrieval after compaction."
    )
    parser.add_argument(
        "--scope-key",
        required=True,
        help="Project or repository scope, such as a repo path or project id",
    )
    parser.add_argument(
        "--session-id",
        help="Optional agent session id for provenance",
    )
    parser.add_argument(
        "--context-type",
        default="note",
        choices=["note", "decision", "summary", "compaction", "task_state"],
        help="Kind of context being stored",
    )
    parser.add_argument(
        "--title",
        help="Optional short label for the saved context",
    )
    parser.add_argument(
        "content",
        nargs="*",
        help="Context text to embed and store",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    content = " ".join(args.content).strip()

    if not content and not sys.stdin.isatty():
        content = sys.stdin.read().strip()

    if not content:
        print(
            'Usage: save_agent_context.py --scope-key /path/to/repo "context text"',
            file=sys.stderr,
        )
        return 1

    try:
        from agent_context import save_agent_context

        message = save_agent_context(
            content,
            args.scope_key,
            session_id=args.session_id,
            context_type=args.context_type,
            title=args.title,
        )
    except Exception as error:
        print(f"ERROR: agent context save failed: {error}", file=sys.stderr)
        return 1

    print(message)
    return 0


if __name__ == "__main__":
    sys.exit(main())
