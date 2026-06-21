# ADR 0012: Split Startup Configuration From SQLite-Backed Settings

## Status

Accepted

## Context

QTCode needs a small set of user preferences that must be available before SQLite is open, plus a larger set of durable UI and workflow state that should survive across sessions. The app also needs predictable restore behavior for its main window layout, active project selection, and other local state.

Qt and KDE already provide a native configuration system for startup-time preferences. QTCode also uses SQLite for the rest of its local application state. Keeping those responsibilities separate avoids forcing the database to be available before startup decisions are made.

## Decision

Use two persistence paths for settings:

- Store startup-time app preferences in a KDE config file loaded through `AppConfigService` before SQLite opens.
- Store durable runtime settings, including panel layout and window state, in SQLite through `SettingsService`.

The current user-facing settings surface is a modal Settings dialog in `MainWindow` that edits the KDE-config-backed app preferences. Panel layout and window geometry are restored and persisted automatically by the shell.

## Consequences

Positive:

- Startup preferences are available early, before storage initialization.
- Layout and workflow state stay in the same SQLite backup and migration model as the rest of QTCode state.
- The UI can restore and persist window behavior without adding a second general-purpose settings database.
- The separation makes it explicit which settings belong to app startup and which belong to runtime state.

Tradeoffs:

- Settings are split across two storage systems, so backup and restore procedures must include both when a full profile is needed.
- Features that belong in startup config versus SQLite need an explicit placement decision.
- The app must handle failure of the KDE config path separately from SQLite failure.

## Follow-Ups

- Keep the startup config surface intentionally small.
- Document any new settings field in the settings spec before implementation.
- Preserve the SQLite-backed layout schema during future shell changes.
