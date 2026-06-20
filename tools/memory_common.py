"""Shared helpers for QTCode project-memory tools."""

from __future__ import annotations

import os
from collections.abc import Sequence
from pathlib import Path


DEFAULT_DATABASE_URL = "postgresql:///qtcode_memory?host=/var/run/postgresql"
DEFAULT_OPENAI_KEY_PATH = Path.home() / ".openAi" / "key"
PROJECT_ROOT = Path(__file__).resolve().parents[1]


def load_project_env() -> None:
    """Load repo-local .env values without requiring python-dotenv."""
    env_path = PROJECT_ROOT / ".env"
    if not env_path.is_file():
        return

    for raw_line in env_path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue

        key, value = line.split("=", 1)
        key = key.strip()
        value = value.strip().strip('"').strip("'")

        if key and value and key not in os.environ:
            os.environ[key] = value


def database_url() -> str:
    load_project_env()
    return os.environ.get("DATABASE_URL", DEFAULT_DATABASE_URL)


def load_openai_api_key() -> str | None:
    """Return the OpenAI API key from the environment or the user key file."""
    load_project_env()

    api_key = os.environ.get("OPENAI_API_KEY")
    if api_key:
        return api_key.strip() or None

    if DEFAULT_OPENAI_KEY_PATH.is_file():
        key = DEFAULT_OPENAI_KEY_PATH.read_text(encoding="utf-8").strip()
        if key:
            os.environ.setdefault("OPENAI_API_KEY", key)
            return key

    return None


def vector_literal(values: Sequence[float]) -> str:
    """Return a pgvector text literal without requiring the pgvector Python package."""
    return "[" + ",".join(str(float(value)) for value in values) + "]"


def embed_text(text: str) -> list[float]:
    """Create an OpenAI embedding vector for the given text."""
    from openai import OpenAI

    api_key = load_openai_api_key()
    if not api_key:
        raise RuntimeError(
            f"Missing OpenAI API key. Set OPENAI_API_KEY or create {DEFAULT_OPENAI_KEY_PATH}."
        )

    client = OpenAI(api_key=api_key)
    result = client.embeddings.create(
        model="text-embedding-3-small",
        input=text,
    )
    return result.data[0].embedding


def embed_text_literal(text: str) -> str:
    """Return a pgvector literal for the embedding of the given text."""
    return vector_literal(embed_text(text))


load_project_env()
