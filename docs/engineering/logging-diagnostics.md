# Logging And Diagnostics

QTCode uses Qt `QLoggingCategory` for development diagnostics. Logs are English-only,
developer-facing output and are not shown in the UI.

## Categories

| Category | Identifier | Use for |
| --- | --- | --- |
| Application shell | `qtcode.app` | Startup, shutdown, top-level lifecycle |
| UI | `qtcode.ui` | Windows, panels, models, user actions |
| Core services | `qtcode.core` | Orchestration, settings, project workflow |
| Storage | `qtcode.storage` | SQLite connections, migrations, queries |
| Agents | `qtcode.agents` | Agent adapters, sessions, execution |
| Terminal | `qtcode.terminal` | Terminal tabs, shells, command launch |
| Git | `qtcode.git` | Repository status, branches, diffs |
| GitHub | `qtcode.github` | `gh` integration, issues, pull requests |
| Memory | `qtcode.memory` | MCP and project-memory retrieval |

Include `shared/Logging.h` and use the matching category macro, for example
`qCInfo(qtcodeUi) << "message"`.

## Severity

- `qCDebug`: verbose flow useful while developing a subsystem
- `qCInfo`: normal lifecycle events such as startup and successful operations
- `qCWarning`: recoverable problems the user may notice indirectly
- `qCCritical`: failures that prevent a subsystem from working

## Rules

- Keep messages short and actionable.
- Do not log secrets, tokens, private keys, or full environment dumps.
- Prefer category context over repeating the subsystem name in every message.
- Use `qCInfo` for startup and other high-signal lifecycle events.

## Enabling Debug Output

Debug builds enable `qtcode.*.debug` by default when `QT_LOGGING_RULES` is unset.

Examples:

```bash
QT_LOGGING_RULES="qtcode.*.debug=true" build/src/app/qtcode
QT_LOGGING_RULES="qtcode.ui.debug=true;qtcode.git.debug=false" build/src/app/qtcode
```

Reference: [Qt logging rules](https://doc.qt.io/qt-6/qloggingcategory.html#configuring-categories)
