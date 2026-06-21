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
3. **Runtime shell state** — stored in SQLite through `SettingsService`, including panel collapse/selection state and agent sessions. Column widths are **not** stored in SQLite; they come from the KDE config file.

The modal Settings dialog in `MainWindow` edits two groups:

- **Global** — KDE config-backed system startup preferences: restore last project, start maximized, and default left/right column widths.
- **Repository** — per-repository overrides for the active project, written to `.qtcode/config.yaml` through `RepoConfigWriter`: default agent and repository help entry.

The KDE config file also stores `repoHelpPath` as the system fallback for help resolution (`doc/index.md` by default). That fallback is not currently exposed in the Settings dialog UI; the Repository group shows it as the placeholder for the per-repository help override field.

Per-repository overrides remain repo-native, versioned with the project, and travel through Git with the repository.

Panel collapse state, active right-panel selection, and terminal collapse state are restored and persisted automatically by the shell. Left and right column widths are configured in the KDE config file and applied on launch, settings save, and reset panel layout; runtime splitter drags are session-only.

### Repository overrides

Repository-specific settings live beside the workspace manifest:

```text
.qtcode/config.yaml
```

The workspace installer scaffolds this file from a bundled template when a repository is prepared. Existing files are never overwritten.

The first supported overrides are:

- **Repository help entry** — the markdown page opened by **Help > Repo Help**.
- **Default agent** — the agent adapter key preselected in the agent panel when the repository has no prior selector value.

Resolution order for help entry:

1. If `.qtcode/config.yaml` defines `help.entryPath` or top-level `repoHelpPath`, use that value (relative to the project root).
2. Otherwise use the system fallback from `AppConfigService` (`repoHelpPath`, default `doc/index.md`).
3. Normalize directory-only values to `index.md` inside that directory (for example, `doc` becomes `doc/index.md`).

Resolution order for default agent:

1. If `.qtcode/config.yaml` defines `agent.defaultAgentKey` or top-level `defaultAgentKey`, use that adapter key when it is registered and available.
2. Otherwise select the first available registered adapter.

`RepoConfigLoader` reads the YAML file with a minimal line parser. `RepoConfigWriter` saves merged repository preferences from the Settings dialog. `effectiveRepoHelpPath()` in `RepoConfig.h` merges repository help overrides over `AppConfig`. `AgentPanel` reads the default agent override on project switch and after settings save. `WorkspaceTabs` resolves the active project's help entry at open time.

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
- Preserve the SQLite-backed panel layout schema during future shell changes; keep collapse/selection in SQLite and column widths in the KDE config file.
- Extend `RepoConfigLoader` or adopt a shared YAML parser only when the repo config surface grows beyond a handful of scalar fields.
