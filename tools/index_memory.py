#!/usr/bin/python3

import hashlib
import sys
from pathlib import Path

from memory_common import database_url, embed_text_literal


TEXT_EXTENSIONS = {".md", ".markdown", ".yml", ".yaml"}
EXCLUDED_DIR_NAMES = {
    ".git",
    ".venv",
    "__pycache__",
    "build",
    "dist",
    "node_modules",
}
EXCLUDED_FILE_NAMES = {
    "LICENSE",
}


def chunks(text, size=1800, overlap=200):
    i = 0
    while i < len(text):
        yield text[i : i + size]
        i += size - overlap


def embed(text: str) -> str:
    return embed_text_literal(text)


def collect_documents(project_root: Path) -> list[Path]:
    """Discover repo docs, governance files, and related text artifacts."""
    documents: list[Path] = []

    for path in project_root.rglob("*"):
        if not path.is_file():
            continue

        if any(part in EXCLUDED_DIR_NAMES for part in path.parts):
            continue

        if path.name in EXCLUDED_FILE_NAMES:
            continue

        if path.suffix.lower() not in TEXT_EXTENSIONS and path.name not in {
            "AGENTS.md",
            "CONTRIBUTING.md",
            "PROMPT.md",
            "README.md",
            "SECURITY.md",
        }:
            continue

        documents.append(path)

    return sorted(documents)


def main() -> int:
    import psycopg

    project_root = Path(__file__).resolve().parents[1]
    documents = collect_documents(project_root)

    with psycopg.connect(database_url()) as conn:
        for file in documents:
            text = file.read_text(errors="ignore")
            source_path = file.relative_to(project_root).as_posix()

            for chunk in chunks(text):
                content_hash = hashlib.sha256(
                    f"{source_path}:{chunk}".encode()
                ).hexdigest()
                vector = embed(chunk)

                conn.execute(
                    """
                    INSERT INTO project_memory
                      (source_path, content, content_hash, embedding)
                    VALUES
                      (%s, %s, %s, %s::vector)
                    ON CONFLICT (content_hash) DO NOTHING
                    """,
                    (source_path, chunk, content_hash, vector),
                )

        conn.commit()

    print(f"Memory indexing complete. Indexed {len(documents)} documents.")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as error:
        print(f"ERROR: memory indexing failed: {error}", file=sys.stderr)
        sys.exit(1)
