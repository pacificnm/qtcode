# ADR 0012: Split Startup Configuration From SQLite-Backed Settings

## Status

Accepted

## Context

QTCode needs a small set of user preferences that must be available before SQLite is open, plus a larger set of durable UI and workflow state that should survive across sessions. The app also needs predictable restore behavior for its main window layout, active project selection, and other local state.

Some workflow preferences are repository-specific rather than global. For example, different projects use different documentation entry files for **Help > Repo Help**. Those values should travel with the repository through Git instead of living in per-user KDE config.

Qt and KDE already provide a native configuration system for startup-time preferences. QTCode also uses SQLite for the rest of its local application state. Keeping those responsibilities separate avoids forcing the database to be available before startup decisions are made.

## Decision

Use three persistence paths for settings:

1. **System startup preferences** — stored in QTCode's KDE config file, loaded through `AppConfigService` before SQLite opens.
2. **Repository preferences** — stored in `.qtcode/config.yaml` inside each project, loaded on demand when that project is active.
3. **Runtime shell state** — stored in SQLite through `SettingsService`, including panel layout and window geometry.

The modal Settings dialog in `MainWindow` edits system startup preferences only. It exposes a **Default Repo Help Entry** field that defines the fallback path when a repository does not override help in `.qtcode/config.yaml`. Per-repository overrides are repo-native, versioned with the project, and edited directly in `.qtcode/config.yaml` rather than through the Settings dialog.

Panel layout and window geometry are restored and persisted automatically by the shell.

### Repository overrides

Repository-specific settings live beside the workspace manifest:

```text
.qtcode/config.yaml
```

The workspace installer scaffolds this file from a bundled template when a repository is prepared. Existing files are never overwritten.

The first supported override is the repository help entry file — the markdown page opened by **Help > Repo Help**. Resolution order:

1. If `.qtcode/config.yaml` defines `help.entryPath` or top-level `repoHelpPath`, use that value (relative to the project root).
2. Otherwise use the system default from **File > Settings**.
3. Normalize directory-only values to `index.md` inside that directory (for example, `doc` becomes `doc/index.md`).

`RepoConfigLoader` reads the YAML file with a minimal line parser. `effectiveRepoHelpPath()` in `RepoConfig.h` merges repository overrides over `AppConfig`. `WorkspaceTabs` resolves the active project's help entry at open time.

## Consequences

Positive:

- Startup preferences are available early, before storage initialization.
- Layout and workflow state stay in the same SQLite backup and migration model as the rest of QTCode state.
- The UI can restore and persist window behavior without adding a second general-purpose settings database.
- The separation makes it explicit which settings belong to app startup, repository workflow, and runtime shell state.
- Repository overrides travel with the project through Git and do not require per-machine KDE config edits.

Tradeoffs:

- Settings are split across three storage locations, so a full profile backup must include the KDE config file and SQLite database; repository overrides are backed up with the Git repository.
- Features need an explicit placement decision: system KDE config, `.qtcode/config.yaml`, or SQLite.
- The app must handle failure of the KDE config path separately from SQLite failure.
- Missing or malformed `.qtcode/config.yaml` must fall back to system defaults without blocking startup.

## Follow-Ups

- Keep the KDE startup config surface intentionally small.
- Add new repository-specific workflow preferences to `.qtcode/config.yaml`, not the KDE config path.
- Document any new settings field in [settings spec](../specs/settings-spec.md) before implementation.
- Preserve the SQLite-backed layout schema during future shell changes.
- Extend `RepoConfigLoader` or adopt a shared YAML parser only when the repo config surface grows beyond a handful of scalar fields.
