# Performance Notes

This document describes the startup and repository refresh performance characteristics introduced for M6 beta hardening.

## Design Goals

- Keep the first window paint responsive.
- Avoid blocking startup on subprocess or network-adjacent CLI probes.
- Run repository refresh work off the UI thread.
- Log timing for the slow paths developers are most likely to inspect.

## Startup Path

`ApplicationController::initialize()` opens SQLite, restores local state, and schedules CLI capability detection. It does **not** wait for `git`, `gh`, or agent CLI subprocess probes to finish before returning.

`CliCapabilityService::scheduleDetection()` runs capability detection in a background thread via `QtConcurrent::run`. When detection completes, the service emits `capabilitiesDetected`, and `ApplicationController::applyIntegrationPathsFromCapabilities()` updates GitHub and agent executable paths.

MCP health checks are already deferred with `QTimer::singleShot(0, ...)` so they do not block service initialization.

## Repository Refresh Path

`RepositoryPanel::refreshStatus()` dispatches work to a background future. The worker loads:

- libgit2 repository status and recent commits
- GitHub issue and pull request summaries through `GitHubService`

The UI thread only updates widgets in `onRefreshFinished()`.

The first refresh is deferred with `QTimer::singleShot(0, ...)` so panel construction and the initial window layout can complete first. A second refresh may run after CLI capability detection finishes so GitHub executable paths are available.

## Timing Logs

The following messages are emitted at info level:

| Message prefix | Source |
| --- | --- |
| `ApplicationController initialization completed in ... ms` | total startup service initialization |
| `CLI capability detection completed in ... ms` | subprocess probes for git/gh/agent CLIs |
| `Repository snapshot refreshed with ... in ... ms` | end-to-end repository panel refresh |

Enable them with:

```bash
QT_LOGGING_RULES="qtcode.core.info=true;qtcode.ui.info=true" ./build/src/app/qtcode
```

## Manual Verification

1. Launch the app and confirm the main window appears before GitHub issue/PR lists populate.
2. Click **Refresh status** repeatedly and confirm the UI remains interactive while loading labels are shown.
3. Inspect logs for the timing messages above during startup and refresh.

## Out Of Scope

- Profiling every panel and adapter call path.
- Automatic regression thresholds in CI.
- Network caching beyond the existing SQLite `github_cache` layer.
