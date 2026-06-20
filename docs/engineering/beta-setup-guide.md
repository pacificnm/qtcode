# Beta Setup Guide

This guide covers the minimum setup needed to build, run, and dogfood QTCode on KDE/Linux during the M6 beta period.

## Target Environment

- Linux with KDE Plasma (Wayland or X11)
- Qt 6.10 or compatible distro packages
- A local Git checkout of this repository

QTCode is a native desktop application. It is not designed for headless servers except for automated smoke tests.

## Quick Start

From the repository root:

```bash
scripts/check-toolchain
scripts/build-app
./build/src/app/qtcode
```

If `scripts/check-toolchain` reports failures, install the missing packages listed in [toolchain-requirements.md](../toolchain-requirements.md) and rerun the check.

## Required External Tools

These tools are required for the core beta workflows:

| Tool | Purpose | Required for |
| --- | --- | --- |
| `git` | Repository validation, status, commits | Repository panel |
| `gh` | GitHub issue and pull request listing/detail | GitHub workflows |
| C++20 compiler + CMake + Ninja/Make | Build the application | Development |
| Qt 6 + KF6 + QTermWidget + SQLite + libgit2 | Native app runtime and build | Application shell |

These tools are optional but strongly recommended:

| Tool | Purpose | Required for |
| --- | --- | --- |
| `codex` or another supported agent CLI | AI agent sessions | Agent panel |
| PostgreSQL + `pgvector` | Project memory indexing/search | MCP memory tooling |
| Python `.venv` with `openai`, `psycopg`, `mcp` | Memory scripts and MCP server | Memory workflows |

QTCode detects CLI availability at startup and shows guidance in the repository and agent panels when a tool is missing.

## KDE And Linux Dependency Notes

### Build-Time Dependencies

Install development packages before running `scripts/build-app`. On Ubuntu/Debian-family systems:

```bash
sudo apt install \
  build-essential \
  cmake \
  ninja-build \
  pkg-config \
  qt6-base-dev \
  libkf6coreaddons-dev \
  libkf6i18n-dev \
  libqtermwidget6-2-dev \
  libutf8proc-dev \
  libsqlite3-dev \
  libgit2-dev \
  libpq5
```

`scripts/check-toolchain` validates the same dependency set and accepts either pkg-config or CMake config discovery for `KF6I18n`.

### Runtime Dependencies

After installation or when running from `build/src/app/qtcode`, the host still needs compatible runtime libraries for:

- Qt 6 Core, Widgets, Sql, and Concurrent
- KF6 CoreAddons and I18n
- QTermWidget 6
- SQLite 3
- libgit2

If a library is missing, the dynamic linker fails before QTCode starts. Use `ldd build/src/app/qtcode` on the build host to inspect linked runtime libraries.

### Python Memory Environment

Memory helper scripts expect a project virtual environment:

```bash
python3 -m venv .venv
.venv/bin/pip install --upgrade pip
.venv/bin/pip install openai psycopg-binary "mcp[cli]"
scripts/setup-memory-db
scripts/index-memory
```

See [MCP integration](mcp-integration.md) and [storage backup and migration](storage-backup-and-migration.md) for persistence details.

## First-Run Checklist

1. Build the app with `scripts/build-app`.
2. Launch `./build/src/app/qtcode`.
3. Add a local Git repository from the repository panel.
4. Authenticate GitHub CLI if you want live issue and pull request data:

```bash
gh auth login
```

5. Install and verify an agent CLI if you want to send prompts from the agent panel.
6. Optional: configure PostgreSQL memory and run `scripts/index-memory`.

## External Tool Authentication

### GitHub CLI

QTCode reads GitHub metadata through `gh`. Authenticate once per user account:

```bash
gh auth status
gh auth login
```

When `gh` is missing or unauthenticated, GitHub panels degrade to cached SQLite data or empty states instead of blocking startup.

### Agent CLIs

Supported agent discovery order:

1. Codex CLI
2. Claude CLI
3. aider
4. Cursor CLI

Place the preferred tool on `PATH`, then restart QTCode or wait for capability detection to finish.

## Local Data Locations

Application state is stored in SQLite:

```text
~/.local/share/QTCode/qtcode/qtcode.db
```

Panel layout, active project selection, agent sessions, terminal metadata, and GitHub cache entries live in this file. See [storage backup and migration](storage-backup-and-migration.md) for backup and restore steps.

## Verification Commands

Developer verification:

```bash
scripts/check-toolchain
scripts/build-app
scripts/test-app
```

Smoke-only headless launch:

```bash
QTCODE_AUTO_QUIT=1 QT_QPA_PLATFORM=offscreen ./build/src/app/qtcode
```

Logging for startup and refresh timing:

```bash
QT_LOGGING_RULES="qtcode.core.info=true;qtcode.ui.info=true" ./build/src/app/qtcode
```

## Troubleshooting

| Symptom | Likely cause | What to check |
| --- | --- | --- |
| App exits immediately on startup | SQLite migration failure | Logs from `qtcode.storage`; inspect `schema_migrations` |
| GitHub lists stay empty | Missing or unauthenticated `gh` | `gh auth status`, repository panel capability banner |
| Agent prompt fails instantly | Agent CLI missing or blocked | `which codex`, agent panel capability state |
| Terminal tabs fail to start | Shell path or permissions | `QTCODE_TERMINAL_SHELL`, terminal profile settings |
| Memory search unavailable | PostgreSQL or MCP tooling not configured | `scripts/setup-memory-db`, `.venv` packages |

## Related Docs

- [Toolchain requirements](../toolchain-requirements.md)
- [Packaging notes](packaging-notes.md)
- [Performance notes](performance-notes.md)
- [Storage backup and migration](storage-backup-and-migration.md)
