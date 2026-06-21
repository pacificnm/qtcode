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

## Ubuntu And Debian Packages

On Ubuntu 26.04 and similar Debian-family systems, the development packages
checked by `scripts/check-toolchain` map roughly to:

```bash
sudo apt install \
  qt6-base-dev \
  libkf6coreaddons-dev \
  libkf6i18n-dev \
  libkf6xmlgui-dev \
  libqtermwidget6-2-dev \
  libutf8proc-dev \
  libsqlite3-dev \
  libgit2-dev \
  libpq5
```

Notes:

- `qt6-base-dev` provides the `Qt6Core` and `Qt6Widgets` pkg-config modules.
- `libqtermwidget6-2-dev` depends on `libutf8proc` through pkg-config. Install
  `libutf8proc-dev` if `qtermwidget6` is reported missing even after the
  QTermWidget package is installed.
- Some KDE Frameworks 6 packages, including `KF6I18n`, ship CMake config files
  instead of pkg-config modules on current Ubuntu releases. `scripts/check-toolchain`
  accepts either a `KF6I18n.pc` file or `KF6I18nConfig.cmake`.

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
  - validates Git, CMake, Ninja/Make, a C++20 compiler, Qt/KDE/QTermWidget
    dependencies, SQLite, libgit2, GitHub CLI, PostgreSQL memory tooling, and
    the project `.venv`
  - treats `KF6I18n` as present when either pkg-config or the installed CMake
    config is available
  - warns instead of failing when the host `libpq` runtime library is not
    visible, because the memory tools can use `psycopg-binary`
- `scripts/build-app`
- `scripts/test-app`
- `scripts/index-memory`
- `scripts/search-memory "QTCode architecture"`
- `scripts/run-memory-mcp`
- `.venv/bin/python -m py_compile tools/index_memory.py tools/mcp_memory_server.py tools/search_memory.py tools/memory_common.py tools/agent_context.py tools/save_agent_context.py tools/search_agent_context.py tools/memory_retrieval.py .cursor/hooks/memory-workflow.py`
- `git status --short`

## Script Reference

See [scripts/README.md](../scripts/README.md) for the short command list and
what each wrapper is for.

## Notes

If the host distro ships newer compatible versions, prefer the distro packages. The main rule is that the toolchain must be stable enough for C++20 Qt development and the repository's Python helper scripts.

On standard Linux installs, `psycopg` plus `libpq5` is fine. In snap-like or sandboxed shells where the host `libpq` path is not visible, `psycopg-binary` is the simplest way to keep the memory tools working.
