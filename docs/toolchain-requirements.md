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

## Recommended Compiler Versions

- GCC 13 or newer
- Clang 16 or newer

## Local Scripting Runtime

The repository helper scripts in `tools/` use the system Python runtime:

- `/usr/bin/python3`

Install the Python packages needed by the memory tooling when you plan to use it:

- `openai`
- `psycopg`
- `mcp[cli]`

## Memory Tooling Dependencies

QTCode's memory workflow assumes:

- PostgreSQL
- `pgvector`
- an `OPENAI_API_KEY` available to the memory helper scripts
- the local `qtcode_memory` database

## GitHub Integration

For MVP GitHub workflows, install and authenticate:

- `gh`

## Suggested Verification Commands

- `/usr/bin/python3 tools/search_memory.py "QTCode architecture"`
- `/usr/bin/python3 -m py_compile tools/index_memory.py tools/mcp_memory_server.py tools/search_memory.py tools/memory_common.py`
- `git status --short`

## Notes

If the host distro ships newer compatible versions, prefer the distro packages. The main rule is that the toolchain must be stable enough for C++20 Qt development and the repository's Python helper scripts.
