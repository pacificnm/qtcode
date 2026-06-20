# QTCode Memory MCP Setup

This document describes the local setup for the QTCode project-memory MCP
server in `tools/mcp_memory_server.py`.

The memory tools are development-time helpers only. They index repository
documentation into PostgreSQL with `pgvector`, then expose semantic search
through the MCP tool `search_project_memory`.

## Components

| File | Purpose |
| --- | --- |
| `tools/memory_common.py` | Shared environment loading, default database URL, and pgvector literal formatting. |
| `tools/index_memory.py` | Reads approved project context files and Markdown docs, creates OpenAI embeddings, and inserts chunks into PostgreSQL. |
| `tools/search_memory.py` | Command-line semantic search against the local `project_memory` table. |
| `tools/mcp_memory_server.py` | FastMCP stdio server exposing `search_project_memory(query, limit)`. |
| `.env.example` | Template for `DATABASE_URL` and `OPENAI_API_KEY`. |

## Required Services

- PostgreSQL with the `pgvector` extension available.
- Python 3 with the `openai`, `psycopg` or `psycopg-binary`, and `mcp` packages installed.
- An OpenAI API key in `~/.openAi/key` or in the `OPENAI_API_KEY` environment variable.
- The PostgreSQL client runtime library (`libpq5` on Ubuntu), or `psycopg-binary` if the host library is not visible to the Python runtime.

The default connection string is:

```text
postgresql:///qtcode_memory?host=/var/run/postgresql
```

Set `DATABASE_URL` in `.env` when using a TCP connection, a different user, or a
different database name.

## PostgreSQL Setup

Install PostgreSQL and `pgvector` using the package names for the host OS. On a
Debian-family workstation the packages are typically:

```bash
sudo apt install postgresql postgresql-contrib postgresql-XX-pgvector
```

Replace `XX` with the installed PostgreSQL major version.

Create the database and enable `pgvector`:

```bash
sudo -u postgres createdb qtcode_memory
sudo -u postgres psql qtcode_memory
```

Run this SQL in the `qtcode_memory` database:

```sql
CREATE EXTENSION IF NOT EXISTS vector;

CREATE TABLE IF NOT EXISTS project_memory (
    id bigserial PRIMARY KEY,
    source_path text NOT NULL,
    content text NOT NULL,
    content_hash text NOT NULL UNIQUE,
    embedding vector(1536) NOT NULL,
    created_at timestamptz NOT NULL DEFAULT now()
);

CREATE INDEX IF NOT EXISTS project_memory_embedding_idx
    ON project_memory
    USING hnsw (embedding vector_cosine_ops);

CREATE INDEX IF NOT EXISTS project_memory_source_path_idx
    ON project_memory (source_path);
```

`tools/index_memory.py` uses OpenAI `text-embedding-3-small`, which produces the
1536-dimension vectors expected by this table design.

For a local Unix-socket setup, the operating-system user running the tools must
be able to connect to the database. One simple development setup is to create a
matching PostgreSQL role:

```bash
sudo -u postgres createuser "$USER"
sudo -u postgres psql -d qtcode_memory -c "GRANT ALL PRIVILEGES ON DATABASE qtcode_memory TO \"$USER\";"
sudo -u postgres psql -d qtcode_memory -c "GRANT ALL PRIVILEGES ON ALL TABLES IN SCHEMA public TO \"$USER\";"
sudo -u postgres psql -d qtcode_memory -c "GRANT ALL PRIVILEGES ON ALL SEQUENCES IN SCHEMA public TO \"$USER\";"
```

If the role already exists, keep it and only apply the grants.

## Python Setup

On Ubuntu and other PEP 668-managed systems, install the MCP tooling into a
project virtual environment instead of the system Python:

```bash
sudo apt install python3-full
sudo apt install libpq5
python3 -m venv .venv
. .venv/bin/activate
python -m pip install --upgrade pip
python -m pip install openai psycopg-binary "mcp[cli]"
```

Create `.env` from the example if you want to override the default key path or
database URL:

```bash
cp .env.example .env
```

Required values when you use `.env`:

```env
DATABASE_URL="postgresql:///qtcode_memory?host=/var/run/postgresql"
OPENAI_API_KEY="sk-..."
```

Do not commit `.env`.

If you prefer the key-file workflow, create the file once:

```bash
mkdir -p ~/.openAi
printf '%s\n' 'sk-...' > ~/.openAi/key
chmod 600 ~/.openAi/key
```

If you are on a normal Linux desktop and want to use the system `libpq`
runtime instead of the bundled wheel, install `psycopg` rather than
`psycopg-binary` and keep the `libpq5` package installed.

## Index Project Memory

Run the indexer from the repository root:

```bash
scripts/index-memory
```

The indexer automatically discovers repository documentation and repo-related
text files, including:

- root-level markdown files such as `README.md`, `PROMPT.md`, `AGENTS.md`,
  `CONTRIBUTING.md`, and `SECURITY.md`
- `docs/**/*.md`
- `.github/**/*.md`
- `.github/**/*.yml`
- `.github/**/*.yaml`
- `tools/*.md`

Generated directories and local virtual environments are skipped.

Each document is split into overlapping chunks. The indexer stores
`source_path`, `content`, a `content_hash`, and the embedding. Existing chunks
are skipped by `content_hash`, so repeated indexing is safe.

To rebuild the memory table from scratch:

```sql
TRUNCATE project_memory RESTART IDENTITY;
```

Then rerun:

```bash
scripts/index-memory
```

## Test Local Search

Before wiring MCP into an agent, verify direct search:

```bash
scripts/search-memory "current milestone and QTCode architecture"
```

Expected behavior:

- The command prints matching source paths and snippets.
- A missing `OPENAI_API_KEY` fails during embedding creation.
- A missing table or extension fails with a PostgreSQL error.

## Run The MCP Server

The MCP server runs over stdio:

```bash
scripts/run-memory-mcp
```

It exposes one tool:

| Tool | Arguments | Result |
| --- | --- | --- |
| `search_project_memory` | `query: str`, `limit: int = 8` | Matching project-memory snippets grouped by source path. |

Example MCP client command configuration:

```json
{
  "mcpServers": {
    "qtcode-memory": {
      "command": "/home/jaimie/qtcode/.venv/bin/python",
      "args": [
        "/home/jaimie/qtcode/tools/mcp_memory_server.py"
      ],
      "env": {
        "DATABASE_URL": "postgresql:///qtcode_memory?host=/var/run/postgresql",
        "OPENAI_API_KEY": "sk-..."
      }
    }
  }
}
```

If the client launches the server from a shell where `.env` is readable from the
repository root, the `env` block can omit `DATABASE_URL` and `OPENAI_API_KEY`.
If `OPENAI_API_KEY` is not set, the tools will fall back to `~/.openAi/key`.
Providing explicit environment values is usually clearer for editor and agent
integrations.

## Agent Usage

Implementation agents should query project memory before changing code. Useful
queries include:

- `current milestone`
- the affected subsystem, such as `QtCode dashboard settings`
- related ADR numbers or architecture terms
- known issue text or build failure text
- build, release, recovery, or packaging instructions

The memory result does not override approved documentation. Follow the binding
order in `AGENTS.md`: `/doc` specifications first, then MCP project memory, then
the remaining project notes and existing code behavior.

## Troubleshooting

| Symptom | Likely cause | Fix |
| --- | --- | --- |
| `Missing OpenAI API key` | Neither `OPENAI_API_KEY` nor `~/.openAi/key` is available. | Set the environment variable or create the key file. |
| `Missing Python dependency for QTCode memory MCP` | The MCP server dependencies are not installed in the project virtual environment. | Activate `.venv` and install `openai psycopg "mcp[cli]"`. |
| `relation "project_memory" does not exist` | The table has not been created in the configured database. | Run the SQL table setup in this document. |
| `type "vector" does not exist` | `pgvector` is not installed or `CREATE EXTENSION vector` has not been run. | Install the PostgreSQL `pgvector` package and enable the extension in `qtcode_memory`. |
| Authentication or peer errors | The local PostgreSQL role does not match the OS user or lacks grants. | Create/grant the role or set `DATABASE_URL` to a connection string with explicit credentials. |
| OpenAI authentication errors | `OPENAI_API_KEY` is missing or invalid, and `~/.openAi/key` is absent or unreadable. | Set the key in `.env`, export `OPENAI_API_KEY`, or create `~/.openAi/key`. |
| Empty search results | The database is empty. | Run `scripts/index-memory`. |

## Convenience Scripts

- See [scripts/README.md](../scripts/README.md) for the short command list and
  what each wrapper is for.
- `scripts/check-toolchain` validates the local build and development setup.
- `scripts/setup-memory-db` creates the `qtcode_memory` database and applies the schema.
- `scripts/index-memory` indexes the repo docs and governance files.
- `scripts/search-memory` searches indexed memory from the command line.
- `scripts/run-memory-mcp` starts the FastMCP server.
