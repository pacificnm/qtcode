# Settings Specification

ADR: [ADR 0012: Split Startup Configuration From SQLite-Backed Settings](../adrs/0012-settings-persistence-and-configuration.md)

Related docs:

- [Engineering architecture](../engineering/architecture.md)
- [Class responsibilities](../engineering/class-responsibilities.md)
- [Database schema](../engineering/database-schema.md)
- [Storage backup and migration](../engineering/storage-backup-and-migration.md)
- [Workspace setup spec](workspace-setup-spec.md)
- [MCP integration](../engineering/mcp-integration.md)
- [UI layout spec](../design/ui-layout-spec.md)

## Purpose

Define how QTCode stores, restores, and uses user-facing settings and local shell state. The settings feature is not a generic preferences system; it is the small set of configuration and layout controls needed to make the native cockpit feel stable between launches.

## Scope

The current settings feature covers:

- system startup preferences loaded before SQLite opens
- repository-specific preferences in `.qtcode/config.yaml`
- persisted main-window layout and panel state
- restore behavior for the active project
- the modal Settings dialog exposed from the File menu

Out of scope for this feature:

- GitHub account login state
- MCP server records and secrets
- agent adapter configuration
- SQLite-backed session and workflow state owned by other services

## User Outcomes

- The user can choose whether QTCode restores the last active project on startup.
- The user can choose whether the main window starts maximized.
- The user can set a default repository help entry file that applies to all projects unless overridden.
- Each repository can override the help entry file in `.qtcode/config.yaml` so documentation paths differ per project without changing global settings.
- The user can resize and collapse the main shell panels and have that state restored later.
- The user can quit and relaunch QTCode without losing their preferred shell layout.

## Settings Types

QTCode uses three persistence tiers. When adding a new setting, choose the tier that matches when the value must be available and whether it is global or repository-specific.

| Tier | Storage | When loaded | Examples |
| --- | --- | --- | --- |
| System startup | KDE config file via `AppConfigService` | Before SQLite opens | restore last project, start maximized, default repo help entry |
| Repository | `.qtcode/config.yaml` via `RepoConfigLoader` | When the active project needs the value | repo help entry override |
| Runtime shell | SQLite via `SettingsService` | After storage opens | panel layout, window size, collapse state |

### System Startup Preferences

Startup preferences are loaded by `AppConfigService` from the KDE config file before SQLite opens.

Supported fields in the `General` config group:

| Field | KDE key | Default | Purpose |
| --- | --- | --- | --- |
| `restoreLastProjectOnStartup` | `restoreLastProjectOnStartup` | `true` | Restore the last active project on launch |
| `startMaximized` | `startMaximized` | `false` | Open the main window maximized |
| `repoHelpPath` | `repoHelpPath` | `doc/index.md` | Default repository help entry file, relative to project root |

These values control the initial application experience and must be available during controller startup.

The Settings dialog header explains that these are system defaults and that per-repository overrides belong in `.qtcode/config.yaml`.

### Repository Preferences

Repository-specific preferences override system defaults when an active project is open.

They live in:

```text
.qtcode/config.yaml
```

The workspace installer creates this file from `resources/workspace-bundle/templates/config.yaml.tpl` when a repository is prepared. The installer is idempotent: an existing `.qtcode/config.yaml` is never overwritten.

Supported fields:

| Field | YAML shape | Purpose |
| --- | --- | --- |
| `repoHelpPath` | top-level scalar | Repository help entry file, relative to project root |
| `help.entryPath` | nested under `help:` | Same as `repoHelpPath`; preferred documented form |

Example:

```yaml
help:
  entryPath: docs/README.md
```

Alternative form:

```yaml
repoHelpPath: docs/README.md
```

If a repository does not define an override, QTCode uses the system default from **File > Settings**.

Repository preferences are repo-native, versioned with the project, and can differ between repositories without changing global settings.

#### Repository help entry resolution

**Help > Repo Help** opens the configured markdown entry file in `RepoHelpView`.

Resolution flow:

1. `WorkspaceTabs` reads the active project root from `ProjectManager`.
2. `RepoConfigLoader::loadFromProjectRoot()` reads `.qtcode/config.yaml` if present.
3. `effectiveRepoHelpPath(appConfig, repoConfig)` returns the repository override when set; otherwise the system default from `AppConfigService`.
4. `normalizedRepoHelpPath()` trims whitespace, applies the default `doc/index.md` when empty, and expands directory-only values to `index.md` inside that directory.

Link navigation in `RepoHelpView` is scoped to the parent directory of the resolved entry file.

Behavior on failure:

- Missing `.qtcode/config.yaml` — use system default.
- Unreadable config file — treat as empty repository config; use system default.
- Missing help entry file — show an in-view message and a warning through `StatusService`; do not block the rest of the shell.

Legacy migration:

- Older KDE config values that stored only a directory (for example, `doc`) are normalized to `doc/index.md` on load and save through `normalizedRepoHelpPath()`.

### Runtime Shell State

Runtime shell state is stored in SQLite through `SettingsService`.

Supported fields:

- root window width and height
- left column, center area, and right column splitter sizes
- terminal panel splitter sizes
- active right-panel selection
- right column collapsed state
- terminal panel collapsed state
- stored right column width
- stored terminal height

The layout payload is serialized under the `app.panel_layout` key.

## Persistence Model

### Startup Config File

Startup preferences live in QTCode's KDE config file. The file is loaded before SQLite so startup decisions can be made even if the database is unavailable.

Behavior:

- load defaults first
- read the `General` config group
- merge any stored values over defaults
- normalize `repoHelpPath` on load and save
- save immediately when the Settings dialog is accepted

If the config file cannot be saved, the dialog reports the error and does not close as a successful save.

### Repository Config File

Repository preferences are plain YAML files checked into the project under `.qtcode/`.

Behavior:

- read on demand from the active project root
- never written by the Settings dialog
- scaffolded by workspace setup when missing
- parsed with a minimal line-oriented parser in `RepoConfigLoader` (comments, quoted strings, and the two supported key shapes only)

Malformed or partial YAML does not crash the app. Unrecognized keys are ignored until explicitly supported.

### SQLite Settings

Runtime settings live in the `settings` table in `qtcode.db`.

Behavior:

- load defaults if no setting exists yet
- deserialize JSON into `PanelLayoutSettings`
- persist layout changes during normal shutdown
- keep settings in the same backup and migration model as other durable local state

## Startup Flow

1. `ApplicationController` creates `AppConfigService`.
2. `AppConfigService::load()` reads the startup config file.
3. `StorageService` opens SQLite.
4. `MigrationRunner` applies any pending migrations.
5. `SettingsService` becomes available for SQLite-backed settings.
6. `ProjectManager` restores the active project unless startup config disables that behavior.
7. `MainWindow` applies the restored shell layout and panel state.

Repository config is not loaded during startup. It is read when the user opens **Help > Repo Help** or when another feature resolves an effective repository preference for the active project.

If startup config loading fails, QTCode continues with defaults so the app remains usable. If SQLite opening or migration fails, startup stops because the rest of the application depends on that storage.

## User Interaction

### Settings Dialog

`MainWindow` opens the Settings dialog from the File menu.

The dialog currently exposes:

- restore last active project on startup
- start main window maximized
- default repository help entry path

The dialog explains that per-repository overrides belong in `.qtcode/config.yaml`. Editing a repository override requires changing that file in the project (or committing a template change through workspace setup), not saving from the Settings dialog.

When the user saves:

- system default values are written through `AppConfigService`
- the dialog closes on success
- the main window shows a brief success message
- open repository help tabs are not automatically reloaded; the user can reopen **Help > Repo Help** to pick up a changed default or edited `.qtcode/config.yaml`

### Automatic Layout Persistence

`MainWindow` captures its current panel layout when the application shuts down normally and saves it through `SettingsService`.

This covers:

- window size
- splitter state
- collapse state
- active right panel

The current layout is restored on the next launch.

## Component Responsibilities

| Component | Role |
| --- | --- |
| `AppConfigService` | Load/save system startup preferences in the KDE config file |
| `AppConfig` / `RepoConfig` | Typed settings models and `effectiveRepoHelpPath()` merge helper |
| `RepoConfigLoader` | Read `.qtcode/config.yaml` from a project root |
| `SettingsDialog` | Edit system defaults; document repo override location |
| `WorkspaceTabs` | Resolve effective repo help path for the active project |
| `RepoHelpView` | Render the resolved markdown entry and in-doc links |
| `WorkspaceInstaller` | Scaffold `.qtcode/config.yaml` from the bundled template |
| `SettingsService` | Persist SQLite-backed shell layout state |

## Behavior Rules

- Use defaults when no stored setting exists.
- Keep the KDE startup config minimal and easy to reason about.
- Keep repository workflow preferences repo-native in `.qtcode/config.yaml`.
- Clamp the left column width to a safe range when loading or saving shell layout.
- Preserve older layout payloads when possible so upgrades do not lose the user's arrangement.
- Treat missing startup config as recoverable.
- Treat missing repository config as "use system default".
- Treat missing SQLite as fatal for the main application shell.

## Extensibility Rules

When adding a new setting:

1. Decide whether it must be read before SQLite opens.
2. Put global startup-only preferences in the KDE config path through `AppConfigService`.
3. Put repository-specific workflow preferences in `.qtcode/config.yaml` and extend `RepoConfigLoader`.
4. Put durable shell state in SQLite if it belongs to the application lifecycle.
5. Document the new field in this spec, ADR 0012 when the placement model changes, and the relevant engineering docs.
6. Add tests for load/save, merge precedence, and migration behavior if the value is persisted.

## Acceptance Criteria

- Users can edit system startup preferences from the Settings dialog.
- Startup preferences are loaded before SQLite opens.
- Users can set a default repository help entry path in the Settings dialog.
- Repositories can override the help entry path in `.qtcode/config.yaml`.
- Effective help entry resolution prefers the repository override over the system default.
- Directory-only help paths normalize to `index.md` inside that directory.
- Workspace setup scaffolds `.qtcode/config.yaml` without overwriting existing files.
- Main-window layout state is restored automatically across launches.
- Panel layout saves on normal shutdown.
- The backup story remains coherent: runtime settings in SQLite, system defaults in the KDE config file, repository overrides in Git with the project.
