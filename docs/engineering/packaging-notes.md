# Packaging Notes

This document outlines the current and near-term packaging path for QTCode on target Linux/KDE distributions.

## Current State

QTCode is distributed from source in this repository. The supported developer workflow is:

```bash
scripts/build-app
./build/src/app/qtcode
```

CMake already defines an install rule for the executable:

```cmake
install(TARGETS qtcode RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
```

There is no official `.deb`, RPM, Flatpak, or AppImage release yet. These notes describe the intended packaging direction for M6 beta hardening.

## Packaging Goals

- Ship one native `qtcode` executable linked against distro Qt/KDE libraries.
- Keep runtime dependencies explicit and testable with `scripts/check-toolchain`.
- Avoid bundling PostgreSQL, GitHub CLI, or agent CLIs inside the main package.
- Document optional integrations separately from the core application package.

## Runtime Dependency Set

A packaged QTCode binary should declare runtime dependencies equivalent to the build's linked libraries:

| Component | Notes |
| --- | --- |
| Qt 6 Core/Widgets/Sql/Concurrent | Required |
| KF6 CoreAddons | Required |
| KF6 I18n | Required |
| KF6 XmlGui | Required; menus, toolbars, and shared actions via `KActionCollection` |
| KF6 TextEditor | Required; workspace file editing via KTextEditor |
| QTermWidget 6 | Required |
| SQLite 3 | Required |
| libgit2 | Required |

Recommended companion packages, not hard dependencies of the core binary:

| Component | Notes |
| --- | --- |
| `git` | Repository workflows |
| `gh` | GitHub issue and pull request integration |
| Agent CLI (`codex`, etc.) | Agent panel execution |
| PostgreSQL client tools | Memory indexing and MCP workflows |

## Ubuntu And Debian Path

Recommended near-term packaging approach:

1. Build release binary with `-DCMAKE_BUILD_TYPE=Release`.
2. Install into a staging prefix:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DQTcode_BUILD_TESTS=OFF
cmake --build build
DESTDIR=/tmp/qtcode-stage cmake --install build
```

3. Generate metadata with the staged files:
   - binary: `usr/bin/qtcode`
   - dependency list from `dpkg-shlibdeps` or manual review
4. Publish a `.deb` that depends on the runtime libraries above.

Suggested package sections:

- `qtcode` — main application
- `qtcode-doc` — optional documentation bundle (future)

Validation checklist for Debian-family packages:

- [ ] `qtcode` starts on a clean KDE Plasma session
- [ ] repository add/open works against a local Git repo
- [ ] `gh`-backed panels degrade cleanly when `gh` is not installed
- [ ] SQLite database is created under `~/.local/share/QTCode/qtcode/`

## Fedora And RHEL Path

The same CMake install flow applies. RPM packaging should:

- use `%cmake` / `%cmake_build` / `%cmake_install` macros where available
- declare `Requires:` for Qt6, KF6, QTermWidget, sqlite, and libgit2 packages provided by the target distro
- ship a desktop entry at `/usr/share/applications/org.qtcode.QTCode.desktop` (future packaging task)

Because Fedora and RHEL package names differ from Debian, maintain a small mapping table in the spec file comments rather than hardcoding Ubuntu package names.

## Flatpak And AppImage (Future)

Not part of the M6 deliverable, but the likely direction is:

### Flatpak

- runtime: `org.kde.Platform`
- finish-args: filesystem access for project directories, network for GitHub/agent workflows
- separate extension or host tool access for `git`, `gh`, and agent CLIs

### AppImage

- bundle only QTCode and dynamically link against common host libraries where possible
- document known limitations for QTermWidget and KDE integration

Both formats need additional validation for terminal embedding and `XDG_DATA_HOME` behavior before they become primary install paths.

## Install Prefix Layout

Expected FHS layout after `cmake --install`:

```text
${CMAKE_INSTALL_PREFIX}/
  bin/qtcode
```

Future packaging may add:

```text
share/applications/org.qtcode.QTCode.desktop
share/icons/hicolor/scalable/apps/qtcode.svg
share/doc/qtcode/
```

## CI And Release Validation

Before publishing a binary package:

1. Run `scripts/test-app` on the build that produced the package.
2. Run smoke tests from [beta setup guide](beta-setup-guide.md).
3. Verify migration behavior against a fresh database and an upgraded database.
4. Record the Qt, KF6, and QTermWidget versions used during the build in release notes.

## Related Docs

- [Beta setup guide](beta-setup-guide.md)
- [Toolchain requirements](../toolchain-requirements.md)
- [Storage backup and migration](storage-backup-and-migration.md)
