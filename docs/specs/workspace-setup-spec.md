# Workspace Setup Specification

## Goal

Let QTCode scaffold repo-local memory tooling when a developer opens a new repository, without reimplementing the Python MCP/RAG stack in C++.

The app orchestrates installation. Python scripts, PostgreSQL, and MCP remain the execution layer.

## Manifest

Each prepared repository contains:

```text
.qtcode/workspace.yaml
```

Example:

```yaml
templateVersion: 1
projectName: my-app
rootPath: /home/dev/my-app
scopeKey: /home/dev/my-app
installedAt: 2026-06-21T06:30:00.000Z
installedBy: qtcode

memory:
  databaseUrl: postgresql:///qtcode_memory
  pythonVenv: .venv/bin/python

index:
  includeGlobs:
    - "docs/**"
    - "*.md"
    - "AGENTS.md"
  excludeGlobs:
    - ".git/**"
    - ".venv/**"
    - "build/**"
```

## Installed Files

Workspace setup is idempotent. Existing files are never overwritten.

| Path | Purpose |
| --- | --- |
| `.qtcode/workspace.yaml` | Repo manifest and scope key |
| `.qtcode/config.yaml` | Per-repository QTCode settings that override system defaults |
| `.env.example` | Local env placeholders |
| `scripts/*` | Bash wrappers for memory tooling |
| `tools/*.py` | Python MCP/RAG scripts |
| `.cursor/mcp.json` | Cursor MCP server config |
| `.cursor/hooks.json` | Cursor memory workflow hooks |
| `.cursor/hooks/memory-workflow.py` | Hook implementation |
| `.cursor/rules/memory-workflow.mdc` | Cursor rule for memory workflow |

## Install Trigger

When the active repository is missing `.qtcode/workspace.yaml`, the Repository panel shows a setup banner with **Set up QTCode workspace**.

The installer:

1. Resolves the bundled template source from `QTcode_SOURCE_DIR` during development or from the installed app data directory.
2. Renders templated files with project name, root path, scope key, and database URL.
3. Copies static scripts, tools, and Cursor config files.
4. Marks skipped files that already exist.

After install, the developer still needs to:

```bash
python3 -m venv .venv
.venv/bin/pip install openai psycopg "mcp[cli]"
scripts/setup-memory-db
scripts/index-memory
```

## Workspace Health

The MCP panel includes a workspace health summary for the active repository:

- workspace manifest present
- memory scripts installed
- Python tools installed
- project virtual environment present
- Cursor MCP config present

Health checks are advisory. Missing `.venv` is a warning, not a hard error.

## Repository Config

Prepared repositories may also contain `.qtcode/config.yaml`. This file holds repository-specific QTCode settings that override system defaults from **File > Settings**. It is separate from `workspace.yaml`, which describes memory tooling and scope metadata.

The installer copies `templates/config.yaml.tpl` to `.qtcode/config.yaml` when that file does not already exist. Existing config files are never modified.

Example:

```yaml
help:
  entryPath: docs/README.md
```

The first supported override is the repository help entry — the markdown file opened by **Help > Repo Help**. QTCode also accepts a top-level `repoHelpPath` key with the same meaning.

If no override is present, QTCode uses the system default from the Settings dialog (`doc/index.md` unless changed). See [settings spec](settings-spec.md) for resolution rules and normalization behavior.

## Related Docs

- [ADR 0012: Split startup configuration from SQLite-backed settings](../adrs/0012-settings-persistence-and-configuration.md)
- [Settings spec](settings-spec.md)
- [ADR 0005: Memory through MCP and RAG](../adrs/0005-memory-through-mcp-rag-system.md)
- [MCP integration](mcp-integration.md)
- [Toolchain requirements](../toolchain-requirements.md)
