# Storage Backup And Migration

This document describes where QTCode stores local SQLite state, how migrations behave on failure, and how to back up or restore application data safely.

## Database Location

QTCode uses one SQLite database file for local application state. The default path is resolved through Qt `QStandardPaths::AppDataLocation`:

```text
~/.local/share/QTCode/qtcode/qtcode.db
```

On Linux this follows `XDG_DATA_HOME` when set. QTCode sets:

- organization name: `QTCode`
- application name: `qtcode`

`StorageService` creates the parent directory on first open if it does not exist.

For tests and smoke runs, the same layout is used under an isolated `XDG_DATA_HOME` directory.

## What Lives In SQLite

All durable local settings and metadata are stored in `qtcode.db`, including:

| Area | Examples |
| --- | --- |
| Settings | `app.panel_layout`, `app.active_project_id`, terminal profile keys |
| Projects | registered local repositories and GitHub metadata |
| Agents | sessions, messages, and adapter-related persisted state |
| GitHub cache | cached issue and pull request summaries |
| MCP servers | configured memory server records |

Most durable local settings live in SQLite, including panel layout and project selection. If you back up `qtcode.db`, you back up the state that matters for day-to-day shell restore.

QTCode also keeps a small KDE config file for system app preferences that must load before SQLite opens, such as startup behavior, default column widths, and the system fallback repository help entry path. That file uses the normal KDE INI-style config format and lives beside the rest of the user's KDE application config data.

Repository-specific preference overrides live in each project's `.qtcode/config.yaml`. Those files are repo-native, versioned with Git, and may define the default agent and repository help entry for that project. They are backed up with the Git repository, not with `qtcode.db` or the KDE config file.

## Migration Behavior

Migrations are applied by `MigrationRunner` during `ApplicationController::initialize()`.

### Normal Startup

1. Open SQLite at the default database path.
2. Ensure `schema_migrations` exists.
3. Apply each registered migration that is not already recorded.
4. Record `(version, name, applied_at)` after a migration succeeds.

The current built-in migration set starts with version `1` (`initial_schema`).

### Re-Running Migrations

Re-running startup against an already migrated database is safe. Applied versions are skipped based on the `schema_migrations` table.

This behavior is covered by `tests/unit/storage/MigrationRunnerTest.cpp`.

### Migration Failure

Each migration runs inside a storage transaction:

- schema DDL/DML for that migration executes first
- the `schema_migrations` insert happens in the same transaction
- the transaction commits only if every statement succeeds

If a migration fails:

1. `MigrationRunner::runPendingMigrations()` returns `false`.
2. `ApplicationController::initialize()` returns `false`.
3. `QtCodeApplication::run()` logs a critical startup error and exits with code `1`.
4. The failed migration is **not** recorded in `schema_migrations`.
5. Already applied migrations remain intact.

There is no partial application of a single migration version. Fix the underlying issue, restore from backup if needed, and restart the app.

## Settings Persistence Timing

Panel layout settings are saved from `MainWindow` during normal shutdown via `SettingsService::savePanelLayout()`.

That means:

- a clean app exit persists active right panel, right-column collapse, and terminal collapse state
- splitter sizes and window geometry are **not** persisted; column widths come from the KDE config file on the next launch
- killing the process abruptly may leave the previous saved collapse/selection state in SQLite
- backup/restore should be performed while QTCode is not running

## Backup Procedure

1. Quit QTCode normally.
2. Copy the database file:

```bash
cp ~/.local/share/QTCode/qtcode/qtcode.db ~/qtcode-backups/qtcode-$(date +%Y%m%d).db
```

3. Optional integrity check:

```bash
sqlite3 ~/qtcode-backups/qtcode-YYYYMMDD.db "PRAGMA integrity_check;"
```

## Restore Procedure

1. Quit QTCode.
2. Replace the live database with the backup copy:

```bash
cp ~/qtcode-backups/qtcode-YYYYMMDD.db ~/.local/share/QTCode/qtcode/qtcode.db
```

3. Launch QTCode and confirm projects, panel layout, and GitHub cache state look correct.

Restoring an older schema version is safe only if the backup came from the same or compatible QTCode release. Newer app versions may add migrations; older databases are upgraded automatically on startup.

## What Backup Does Not Cover

- PostgreSQL/pgvector project memory used by MCP tooling
- Git working trees or uncommitted repository changes
- Per-repository `.qtcode/config.yaml` overrides (back up with the repository itself)
- External CLI authentication state (`gh auth login`, agent CLI credentials)
- KDE Wallet or OS keychain entries if added in future releases

Back up those systems separately when they matter to your workflow.

If you want a full QTCode profile backup, include the KDE config file as well. It stores system startup preferences separately from SQLite. Repository overrides remain in each project's `.qtcode/config.yaml`.

## Verification Commands

Run the storage unit tests:

```bash
scripts/build-app
scripts/test-app --output-on-failure -R "migration_runner|settings_persistence"
```

Inspect settings directly:

```bash
sqlite3 ~/.local/share/QTCode/qtcode/qtcode.db "SELECT key, updated_at FROM settings ORDER BY key;"
```

## Related Docs

- [Database schema](database-schema.md)
- [Storage backup and migration](storage-backup-and-migration.md)
- [Performance notes](performance-notes.md)
- [Toolchain requirements](../toolchain-requirements.md)
