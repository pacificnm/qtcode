# M1: Native App Shell

## Goal

Create the minimal native Qt/KDE application shell and primary panel layout.

## User Outcome

The user can launch QTCode and see the repository panel, AI agent panel, and terminal panel in a stable native window.

## Scope

- CMake project skeleton.
- KDE application metadata.
- `MainWindow` with resizable primary panels.
- Basic action collection and menus via `KActionCollection` (`KF6::XmlGui`).
- Empty state views for repositories, agents, and terminals.
- SQLite database initialization and migrations.
- Settings service with app-level defaults.

## Engineering Deliverables

- `qtcode` executable.
- Core libraries for `ui`, `core`, `storage`, and `settings`.
- First migration with schema metadata.
- Basic logging category setup.

## Exit Criteria

- App launches on KDE/Linux.
- Panels resize without visual breakage.
- Database file is created in the expected app data location.
- Empty states explain what each panel is waiting for.

## Out Of Scope

- Real Git operations.
- Agent execution.
- GitHub integration.
- Persistent terminal process restoration.
