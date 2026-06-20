# Scripts

This directory contains short wrapper commands for the common QTCode workflows.
They keep long commands out of memory and provide one stable entrypoint per task.

## Available Scripts

| Script | Purpose | Typical Use |
| --- | --- | --- |
| `check-toolchain` | Verifies the local build and development environment. | Run before building or when setting up a machine. |
| `build-app` | Configures and builds the app with CMake. | Run after code changes or when testing the build. |
| `test-app` | Runs `ctest` against the configured build directory. | Run after a successful build. |
| `setup-memory-db` | Creates the `qtcode_memory` database and schema. | Run once on a new machine or after resetting the DB. |
| `index-memory` | Indexes repository docs into project memory. | Run after docs or governance changes. |
| `search-memory` | Searches project memory from the command line. | Run before implementation or debugging work. |
| `save-agent-context` | Saves one agent context chunk for long-term retrieval. | Run before compaction or after major decisions. |
| `search-agent-context` | Searches saved agent context for a project scope. | Run after compaction or when resuming work. |
| `run-memory-mcp` | Starts the MCP memory server. | Use when an agent or client needs memory access. |

## Requirements

- `build-app` and `test-app` will become fully useful once the app CMake project exists.
- `check-toolchain` validates Qt/KDE/QTermWidget dependencies with pkg-config,
  and falls back to the installed `KF6I18n` CMake config when no pkg-config file
  is provided by the distro.
- The memory scripts require `.venv/bin/python`, PostgreSQL, `pgvector`, and the memory Python packages. Use `psycopg-binary` if the host `libpq` runtime library is not visible inside your Python environment.

## Examples

```bash
scripts/check-toolchain
scripts/build-app
scripts/test-app
scripts/setup-memory-db
scripts/index-memory
scripts/search-memory "QTCode architecture"
scripts/save-agent-context --scope-key /home/jaimie/qtcode --context-type compaction "Implemented SettingsService and panel layout persistence."
scripts/search-agent-context --scope-key /home/jaimie/qtcode "panel layout settings"
scripts/run-memory-mcp
```
