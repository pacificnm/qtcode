# Toolchain Requirements

This document records the baseline environment needed to build, run, and work on QTCode successfully.

## Target Platform

- Linux
- KDE Plasma
- Native Qt desktop application

## Required Build Tools

- Git
- CMake 3.28 or newer
- Ninja or GNU Make
- A C++20-capable compiler
- Qt 6 development packages
- KDE Frameworks development packages
- QTermWidget development packages
- SQLite development packages
- libgit2 development packages
- PostgreSQL client runtime libraries (`libpq5` on Ubuntu), or the bundled `psycopg-binary` wheel when the host runtime library is not visible to Python

## Recommended Compiler Versions

- GCC 13 or newer
- Clang 16 or newer

## Local Scripting Runtime

The repository helper scripts in `tools/` should be run from a project virtual
environment when they need third-party packages such as `openai`, `psycopg`, or
`mcp`:

```bash
sudo apt install python3-full
sudo apt install libpq5
python3 -m venv .venv
. .venv/bin/activate
python -m pip install --upgrade pip
python -m pip install openai psycopg-binary "mcp[cli]"
```

Use `/usr/bin/python3` only for simple stdlib-only helper usage.

## Memory Tooling Dependencies

QTCode's memory workflow assumes:

- PostgreSQL
- `pgvector`
- an `OPENAI_API_KEY` environment variable or `~/.openAi/key`
- the local `qtcode_memory` database

## GitHub Integration

For MVP GitHub workflows, install and authenticate:

- `gh`

## Suggested Verification Commands

- `scripts/check-toolchain`
- `scripts/build-app`
- `scripts/test-app`
- `scripts/index-memory`
- `scripts/search-memory "QTCode architecture"`
- `scripts/run-memory-mcp`
- `.venv/bin/python -m py_compile tools/index_memory.py tools/mcp_memory_server.py tools/search_memory.py tools/memory_common.py`
- `git status --short`

## Script Reference

See [scripts/README.md](../scripts/README.md) for the short command list and
what each wrapper is for.

## Notes

If the host distro ships newer compatible versions, prefer the distro packages. The main rule is that the toolchain must be stable enough for C++20 Qt development and the repository's Python helper scripts.

On standard Linux installs, `psycopg` plus `libpq5` is fine. In snap-like or sandboxed shells where the host `libpq` path is not visible, `psycopg-binary` is the simplest way to keep the memory tools working.
