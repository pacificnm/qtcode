# ADR 0010: Use KF6 XmlGui For Application Actions, Menus, And Toolbars

## Status

Accepted

## Context

QTCode is a native KDE/Linux cockpit. The M1 app shell requires a menu bar, main toolbar, and shared actions for high-frequency workflows such as adding a repository, refreshing status, and opening a new terminal tab.

`MainWindow` initially wired panel splitters only. Panel buttons duplicated entry points that also belong in standard desktop chrome. The engineering docs already recommend a Command pattern so actions can be reused from menus, shortcuts, toolbar buttons, and a future command palette.

Qt widgets alone can build menus and toolbars with `QAction`, but KDE applications normally centralize actions through `KActionCollection` from KF6 XmlGui. That module also provides standard help-menu behavior through `KHelpMenu` and integrates with KDE shortcut and Human Interface Guidelines conventions.

## Decision

Add **KF6 XmlGui** as a required build and runtime dependency and use it in `qtcode_ui` for top-level application chrome:

- `KActionCollection` owns shared actions for `MainWindow`.
- `KStandardAction` provides standard entries such as Quit.
- `KHelpMenu` provides Help menu entries, including About QTCode, using existing `KAboutData` metadata from application startup.
- Menu items and toolbar buttons reference the same action objects rather than duplicating handlers in `MainWindow`.

Panel workflows remain the source of business logic. Where a menu or toolbar action mirrors an existing panel workflow, `MainWindow` connects the shared action to an existing public panel slot instead of reimplementing the workflow.

Initial scope in `MainWindow`:

- **File:** Add Repository, Refresh Status, Quit
- **View:** Reset Panel Layout, Main Toolbar visibility toggle
- **Help:** About QTCode and other standard help entries from `KHelpMenu`
- **Main toolbar:** Add Repository, Refresh Status, New Terminal Tab

Do not adopt `KXmlGuiWindow` or XML GUI description files in this milestone. Keep the dependency limited to action collection, standard actions, and help-menu support.

## Consequences

Positive:

- Aligns QTCode with KDE desktop conventions for menus, shortcuts, and help dialogs.
- Gives one action definition for menus, toolbars, and future command-palette wiring.
- Keeps `MainWindow` thin by delegating workflow behavior to panels and services.
- Makes packaging intent explicit: QTCode depends on distro KF6 XmlGui packages, not only CoreAddons and I18n.

Tradeoffs:

- Adds another required KDE Frameworks module to build, runtime, and packaging checks.
- Developers must install `libkf6xmlgui-dev` (or the equivalent on their distro) in addition to other KF6 packages.
- Action ownership and shortcut configuration now follow KActionCollection rules, including associating the collection with the main window.

## Implementation Notes

- CMake discovers the module through `find_package(KF6XmlGui)` in `cmake/QtCodeDependencies.cmake`.
- `qtcode_ui` links `KF6::XmlGui`.
- `scripts/check-toolchain` treats `KF6XmlGui` like `KF6I18n`: accept either pkg-config or the installed CMake config file.
- Ubuntu/Debian package mapping: `libkf6xmlgui-dev` for builds, `libkf6xmlgui6` at runtime.

## Follow-Ups

- Reuse the same `KActionCollection` when a command palette is added.
- Evaluate `KXmlGuiWindow` only if QTCode later needs XML-defined toolbars, configurable shortcuts UI, or richer KDE main-window integration.
- Wire additional panel workflows to shared actions incrementally rather than duplicating handlers in `MainWindow`.

## Related Documents

- [ADR 0001: Build QTCode as a native Qt/KDE desktop application](0001-native-qt-kde-desktop.md)
- [Toolchain requirements](../toolchain-requirements.md)
- [Design patterns: Command](../engineering/design-patterns.md)
- [Class responsibilities: MainWindow](../engineering/class-responsibilities.md)
- [M1: Native app shell](../milestones/m01-native-app-shell.md)
