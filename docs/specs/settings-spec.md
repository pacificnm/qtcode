# Settings Specification

ADR: [ADR 0012: Split Startup Configuration From SQLite-Backed Settings](../adrs/0012-settings-persistence-and-configuration.md)

Related docs:

- [Engineering architecture](../engineering/architecture.md)
- [Class responsibilities](../engineering/class-responsibilities.md)
- [Database schema](../engineering/database-schema.md)
- [Storage backup and migration](../engineering/storage-backup-and-migration.md)
- [MCP integration](../engineering/mcp-integration.md)
- [UI layout spec](../design/ui-layout-spec.md)

## Purpose

Define how QTCode stores, restores, and uses user-facing settings and local shell state. The settings feature is not a generic preferences system; it is the small set of configuration and layout controls needed to make the native cockpit feel stable between launches.

## Scope

The current settings feature covers:

- startup preferences loaded before SQLite opens
- persisted main-window layout and panel state
- restore behavior for the active project
- the modal Settings dialog exposed from the File menu

Out of scope for this feature:

- GitHub account login state
- MCP server records and secrets
- agent adapter configuration
- per-project workflow state that belongs to other services

## User Outcomes

- The user can choose whether QTCode restores the last active project on startup.
- The user can choose whether the main window starts maximized.
- The user can resize and collapse the main shell panels and have that state restored later.
- The user can quit and relaunch QTCode without losing their preferred shell layout.

## Settings Types

### Startup Preferences

Startup preferences are loaded by `AppConfigService` from the KDE config file before SQLite opens.

Supported fields:

- `restoreLastProjectOnStartup`
- `startMaximized`

These values control the initial application experience and must be available during controller startup.

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
- save immediately when the Settings dialog is accepted

If the config file cannot be saved, the dialog reports the error and does not close as a successful save.

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

If startup config loading fails, QTCode continues with defaults so the app remains usable. If SQLite opening or migration fails, startup stops because the rest of the application depends on that storage.

## User Interaction

### Settings Dialog

`MainWindow` opens the Settings dialog from the File menu.

The dialog currently exposes:

- restore last active project on startup
- start main window maximized

When the user saves:

- the values are written through `AppConfigService`
- the dialog closes on success
- the main window shows a brief success message

### Automatic Layout Persistence

`MainWindow` captures its current panel layout when the application shuts down normally and saves it through `SettingsService`.

This covers:

- window size
- splitter state
- collapse state
- active right panel

The current layout is restored on the next launch.

## Behavior Rules

- Use defaults when no stored setting exists.
- Keep the startup config minimal and easy to reason about.
- Clamp the left column width to a safe range when loading or saving shell layout.
- Preserve older layout payloads when possible so upgrades do not lose the user's arrangement.
- Treat missing startup config as recoverable.
- Treat missing SQLite as fatal for the main application shell.

## Extensibility Rules

When adding a new setting:

1. Decide whether it must be read before SQLite opens.
2. Put startup-only preferences in the KDE config path.
3. Put durable shell state in SQLite if it belongs to the application lifecycle.
4. Document the new field in this spec and the relevant engineering docs.
5. Add tests for load/save and migration behavior if the value is persisted.

## Acceptance Criteria

- Users can edit startup preferences from the Settings dialog.
- Startup preferences are loaded before SQLite opens.
- Main-window layout state is restored automatically across launches.
- Panel layout saves on normal shutdown.
- The backup story remains coherent because runtime settings live in SQLite and startup config stays in the KDE config path.
