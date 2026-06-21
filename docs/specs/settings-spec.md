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
- The user can set default left and right column widths that apply on launch and after **Reset Panel Layout**.
- Each repository can override the help entry file, default agent, terminal tab title, and terminal working directory in `.qtcode/config.yaml` so documentation paths, agent choice, and shell context differ per project.
- When a repository has no prior agent selection, the agent selector uses that repository's default agent from `.qtcode/config.yaml`.
- The user can collapse the right column and terminal panel and have that collapse state restored later.
- The user can drag splitters during a session to adjust layout temporarily; column widths reset to the configured defaults on the next launch.
- The user can quit and relaunch QTCode without losing collapse state, active right-panel selection, or agent sessions.

## Settings Types

QTCode uses three persistence tiers. When adding a new setting, choose the tier that matches when the value must be available and whether it is global or repository-specific.

| Tier | Storage | When loaded | Examples |
| --- | --- | --- | --- |
| System startup | KDE config file via `AppConfigService` | Before SQLite opens | restore last project, start maximized, default column widths, system default repo help entry |
| Repository | `.qtcode/config.yaml` via `RepoConfigLoader` | When the active project needs the value | default agent, repo help entry override, terminal display name, terminal working directory |
| Runtime shell | SQLite via `SettingsService` | After storage opens | panel collapse state, active right panel, agent sessions |

### System Startup Preferences

Startup preferences are loaded by `AppConfigService` from the KDE config file before SQLite opens.

Supported fields in the `General` config group:

| Field | KDE key | Default | Purpose |
| --- | --- | --- | --- |
| `restoreLastProjectOnStartup` | `restoreLastProjectOnStartup` | `true` | Restore the last active project on launch |
| `startMaximized` | `startMaximized` | `false` | Open the main window maximized |
| `leftPanelWidth` | `leftPanelWidth` | `240` | Default left column width in pixels |
| `rightPanelWidth` | `rightPanelWidth` | `320` | Default right column width in pixels |
| `repoHelpPath` | `repoHelpPath` | `doc/index.md` | System fallback repository help entry file, relative to project root |

These values control the initial application experience and must be available during controller startup.

The Settings dialog **Global** group edits the first four user-facing fields. `repoHelpPath` remains in the KDE config file as the system fallback for help resolution, but it is not currently exposed in the dialog UI; the **Repository** group shows it as the placeholder for the per-repository help override field.

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
| `defaultAgentKey` | top-level scalar | Default agent adapter key for this repository |
| `agent.defaultAgentKey` | nested under `agent:` | Same as `defaultAgentKey`; preferred documented form |
| `repoHelpPath` | top-level scalar | Repository help entry file, relative to project root |
| `help.entryPath` | nested under `help:` | Same as `repoHelpPath`; preferred documented form |
| `project.displayName` | nested under `project:` | Terminal tab title override for this repository |
| `project.path` | nested under `project:` | Terminal working directory override (absolute or relative to repository root) |

Example:

```yaml
project:
  displayName: my-app
  path: .
agent:
  defaultAgentKey: codex
help:
  entryPath: docs/README.md
```

Alternative form:

```yaml
defaultAgentKey: codex
repoHelpPath: docs/README.md
```

If a repository does not define an override, QTCode uses the system default from `AppConfigService` for help entry resolution and the first available registered agent adapter for agent selection.

Repository preferences are repo-native, versioned with the project, and can differ between repositories without changing global settings. The Settings dialog **Repository** group writes these values to `.qtcode/config.yaml` through `RepoConfigWriter` when an active project is open.

#### Repository help entry resolution

**Help > Repo Help** opens the configured markdown entry file in `RepoHelpView`.

Resolution flow:

1. `WorkspaceTabs` reads the active project root from `ProjectManager`.
2. `RepoConfigLoader::loadFromProjectRoot()` reads `.qtcode/config.yaml` if present.
3. `effectiveRepoHelpPath(appConfig, repoConfig)` returns the repository override when set; otherwise the system default from `AppConfigService`.
4. `normalizedRepoHelpPath()` trims whitespace, applies the default `doc/index.md` when empty, and expands directory-only values to `index.md` inside that directory.

Link navigation in `RepoHelpView` is scoped to the parent directory of the resolved entry file.

#### Default agent resolution

When the user opens or switches to a repository, `AgentPanel::refreshAgentSelector()` chooses the agent selector value in this order:

1. Keep the current selector value when refreshing within the same project context.
2. Otherwise use `agent.defaultAgentKey` or top-level `defaultAgentKey` from `.qtcode/config.yaml` when set and the adapter is registered and available.
3. Otherwise select the first available registered adapter.

After project switch, `AgentPanel::onActiveProjectChanged()` restores the last active session for that project from SQLite when one exists. If no session exists, `ensureActiveSession()` creates one using the currently selected agent key.

Saving a new default agent in **File > Settings** updates `.qtcode/config.yaml` and triggers `AgentPanel::reloadAgentSelector()` so the selector reflects the saved preference immediately.

#### Terminal project display name and path resolution

Terminal tabs use repository config to choose tab title and shell working directory.

Resolution flow:

1. `TerminalManager` loads the active `ProjectRecord` from SQLite.
2. `RepoConfigLoader::loadFromProjectRoot()` reads `.qtcode/config.yaml` when present.
3. `effectiveProjectDisplayName(project, repoConfig)` returns `project.displayName` when set; otherwise the SQLite project name.
4. `effectiveProjectPath(project, repoConfig)` returns `project.path` when set (expanding relative paths against the repository root); otherwise the SQLite project `rootPath`.
5. On project switch, settings save, or session restore, `TerminalManager::syncSessionsToActiveProject()` and `resolveSessionWorkingDirectory()` re-run this resolution so persisted SQLite cwd values do not go stale.

The Settings dialog **Repository** group exposes **Project display name** and **Project path** as editable fields. Empty values clear the override and fall back to SQLite project metadata.

After a successful repository settings save, `MainWindow` calls `TerminalManager::syncSessionsToActiveProject()` so open terminal tabs pick up the new title and cwd without relaunching QTCode.

See [terminal integration](../engineering/terminal-integration.md) for shell startup and cwd application details.

Behavior on failure:

- Missing `.qtcode/config.yaml` — use system default.
- Unreadable config file — treat as empty repository config; use system default.
- Missing help entry file — show an in-view message and a warning through `StatusService`; do not block the rest of the shell.

Legacy migration:

- Older KDE config values that stored only a directory (for example, `doc`) are normalized to `doc/index.md` on load and save through `normalizedRepoHelpPath()`.

### Runtime Shell State

Runtime shell state is stored in SQLite through `SettingsService`.

Panel layout uses schema version 7 under the `app.panel_layout` key. **Only collapse and selection state is persisted.** Splitter positions and window geometry are intentionally not saved.

Persisted fields:

| Field | Default | Purpose |
| --- | --- | --- |
| `activeRightPanel` | `sessions` | Last active right-panel view (`sessions`, `context`, or `mcp`) |
| `rightColumnCollapsed` | `false` | Whether the right column is hidden |
| `terminalCollapsed` | `false` | Whether the terminal panel is collapsed |
| `storedTerminalHeight` | `240` | Terminal height to restore when expanding after collapse; saved only while collapsed |

Not persisted (session-only):

- window width and height
- left, center, and right column splitter sizes
- workspace/terminal vertical splitter sizes
- stored right column width

Column widths always come from the KDE config file (`leftPanelWidth`, `rightPanelWidth`) and are applied by `MainWindow::applyConfiguredColumnWidths()` on launch, after **File > Settings** save, and when choosing **View > Reset Panel Layout**. Runtime splitter drags are allowed but discarded on exit.

Legacy payloads may still contain older keys such as `columnSizes`, `verticalSizes`, `windowWidth`, or `storedRightColumnWidth`. `PanelLayoutSettings::fromJson()` reads them for migration but `toJson()` no longer writes them.

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
- written by the Settings dialog **Repository** group through `RepoConfigWriter` (atomic save via `QSaveFile`)
- scaffolded by workspace setup when missing
- parsed with a minimal line-oriented parser in `RepoConfigLoader` (comments, quoted strings, and the supported scalar/nested key shapes only)
- clearing a repository override field removes that key from the saved YAML; an empty config file is rewritten as commented template content

Malformed or partial YAML does not crash the app. Unrecognized keys are ignored until explicitly supported.

### SQLite Settings

Runtime settings live in the `settings` table in `qtcode.db`.

Behavior:

- load defaults if no setting exists yet
- deserialize JSON into `PanelLayoutSettings`
- persist layout changes during normal shutdown
- keep settings in the same backup and migration model as other durable local state

## Startup Flow

### Application startup

1. `ApplicationController` creates `AppConfigService`.
2. `AppConfigService::load()` reads the KDE startup config file.
3. `StorageService` opens SQLite.
4. `MigrationRunner` applies any pending migrations.
5. `SettingsService` becomes available for SQLite-backed settings.
6. `AgentManager` registers built-in adapters and restores persisted agent sessions from SQLite.
7. `CliCapabilityService` schedules background CLI detection; startup does not wait for agent, Git, or `gh` probes to finish.
8. `ProjectManager` restores the active project unless `restoreLastProjectOnStartup` is disabled.
9. `TerminalManager` restores terminal metadata from SQLite.
10. `MainWindow` applies configured column widths from `AppConfigService`, then restores SQLite collapse/selection state and re-applies column widths in `finalizeInitialLayout()`.

Repository config is not loaded during application startup. It is read when the active project changes, when **Help > Repo Help** opens, or when another feature resolves an effective repository preference.

If startup config loading fails, QTCode continues with defaults so the app remains usable. If SQLite opening or migration fails, startup stops because the rest of the application depends on that storage. Agent session restore failure also stops startup because the agent workflow depends on that state.

### Agent workflow startup (after UI is visible)

When the main window connects panels to services and an active project is present:

1. `AgentPanel::onActiveProjectChanged()` loads the last active session id for that project from `AgentManager`.
2. `refreshAgentSelector()` applies the repository default agent when no prior selector value exists.
3. `refreshSessionList()` selects the restored session or the most recently updated session for the project.
4. If no session exists yet, `ensureActiveSession()` creates one with the selected agent key.
5. When CLI capability detection completes, `AgentPanel` refreshes adapter availability and shows a warning if the agent CLI is missing.

If no project is active, the agent panel stays disabled with guidance to select a repository.

## User Interaction

### Settings Dialog

`MainWindow` opens the Settings dialog from the File menu. The dialog is grouped into **Global** and **Repository** sections.

**Global** (always available):

- restore last active project on startup
- start main window maximized
- left panel width
- right panel width

**Repository** (requires an active project):

- active repository name (read-only label)
- default agent (dropdown of registered adapters)
- repo help entry (repository override; placeholder shows the system fallback from `AppConfigService`)

When no project is active, the Repository section shows guidance to open or select a project first.

When the user saves:

- global values are written through `AppConfigService`
- repository values are written to `.qtcode/config.yaml` through `RepoConfigWriter`, preserving both help and agent fields in the merged config
- `MainWindow` applies configured column widths and reloads the agent selector
- the dialog closes on success
- the main window shows a brief success message through `StatusService`
- open repository help tabs are not automatically reloaded; reopen **Help > Repo Help** to pick up a changed help entry

### Automatic Layout Persistence

`MainWindow` captures collapse and selection state when the application shuts down normally and saves it through `SettingsService`.

This covers:

- active right panel
- right column collapsed state
- terminal collapsed state
- stored terminal height while collapsed

The current collapse/selection state is restored on the next launch. Column widths always come from the KDE config file, not from SQLite.

### Panel Layout Rules

These rules are intentional and should be preserved when changing the shell:

1. **Column widths are settings, not session state.** Edit them in **File > Settings > Global** or reset them with **View > Reset Panel Layout**.
2. **SQLite stores collapse/selection only.** Do not reintroduce splitter-size or window-geometry persistence without revisiting resize performance.
3. **Apply configured widths at stable points only:** launch (`finalizeInitialLayout()`), settings save, and reset panel layout. Do not call `applyConfiguredColumnWidths()` from every panel toggle.
4. **Right column collapse is explicit.** Hide/show the right column from the activity bar toggle; the root horizontal splitter must not collapse the right column by drag-to-zero.
5. **Splitters use rubber-band resize.** Both root and main vertical splitters use `setOpaqueResize(false)` so dragging does not relayout chat, editor, and terminal widgets on every pixel.
6. **Avoid redundant `setSizes()` calls.** Only update splitter sizes when the computed list differs from the current sizes.
7. **Default active right panel is Agent Sessions.** Unknown or legacy `activeRightPanel` values fall back to `sessions`.

## Component Responsibilities

| Component | Role |
| --- | --- |
| `AppConfigService` | Load/save system startup preferences in the KDE config file |
| `AppConfig` / `RepoConfig` | Typed settings models and `effectiveRepoHelpPath()` merge helper |
| `RepoConfigLoader` | Read `.qtcode/config.yaml` from a project root |
| `RepoConfigWriter` | Write merged repository preferences to `.qtcode/config.yaml` |
| `SettingsDialog` | Edit global startup preferences and active-repository overrides |
| `AgentPanel` | Apply repository default agent, restore sessions, and create the first session when needed |
| `WorkspaceTabs` | Resolve effective repo help path for the active project |
| `RepoHelpView` | Render the resolved markdown entry and in-doc links |
| `WorkspaceInstaller` | Scaffold `.qtcode/config.yaml` from the bundled template |
| `SettingsService` | Persist SQLite-backed shell layout state |
| `AgentManager` | Register adapters, restore sessions, and create sessions on demand |

## Behavior Rules

- Use defaults when no stored setting exists.
- Keep the KDE startup config minimal and easy to reason about.
- Keep repository workflow preferences repo-native in `.qtcode/config.yaml`.
- Clamp configured left and right column widths to safe ranges when loading from the KDE config file.
- Preserve older SQLite layout payloads when possible so upgrades do not lose collapse/selection state.
- Do not persist runtime splitter positions; they caused resize jank and fought configured defaults.
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

- Users can edit global startup preferences from the Settings dialog **Global** group.
- Startup preferences are loaded before SQLite opens.
- Users can edit per-repository default agent and help entry overrides from the Settings dialog **Repository** group when a project is active.
- Repositories can also define overrides directly in `.qtcode/config.yaml`.
- Effective help entry resolution prefers the repository override over the system default.
- Default agent selection prefers the repository override when no prior selector value exists for that project context.
- Directory-only help paths normalize to `index.md` inside that directory.
- Workspace setup scaffolds `.qtcode/config.yaml` without overwriting existing files.
- Agent sessions restore from SQLite on startup; project switch restores the last active session or creates one with the selected agent.
- Collapse and right-panel selection state restore automatically across launches.
- Configured column widths from the KDE config file apply on launch, after settings save, and after reset panel layout.
- Runtime splitter drags do not change the next launch layout.
- Panel collapse/selection state saves on normal shutdown.
- The backup story remains coherent: runtime settings and agent sessions in SQLite, system defaults in the KDE config file, repository overrides in Git with the project.
