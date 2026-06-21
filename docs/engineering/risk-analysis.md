# Risk Analysis

## Scope Creep Into Full IDE

Risk:

QTCode could drift toward rebuilding VS Code or JetBrains features.

Mitigation:

- Keep terminal, agents, GitHub, and memory as milestone anchors.
- Treat editor work as preview/review infrastructure only.
- Reject extensions that require web runtime compatibility.

## Agent CLI Instability

Risk:

Agent CLIs differ in flags, output shape, authentication, and interactivity.

Mitigation:

- Use adapter capabilities.
- Prefer structured output where available.
- Add adapter-level tests.
- Surface degraded states clearly.

## MCP/RAG Contract Drift

Risk:

The existing memory service may change independently from QTCode.

Mitigation:

- Isolate MCP communication in `McpClient`.
- Validate response schemas.
- Add health checks and version metadata.
- Make memory failure non-fatal.

## Terminal Lifecycle Complexity

Risk:

Embedded terminals can leak processes or fail to restore sessions cleanly.

Mitigation:

- Define MVP persistence as metadata restoration.
- Centralize lifecycle in `TerminalManager`.
- Handle shutdown explicitly.

## Git Operation Safety

Risk:

Agent-generated changes and Git operations can mutate important work.

Mitigation:

- Require approval for patch application.
- Show diffs before mutation.
- Confirm high-risk Git operations.
- Refresh status after mutations.

## GitHub CLI Dependency

Risk:

`gh` may not be installed or authenticated.

Mitigation:

- Detect capability at startup and per project.
- Provide clear setup guidance.
- Keep GitHub features optional.

## KDE/Linux Packaging Differences

Risk:

Qt, KDE Frameworks, QTermWidget, and libgit2 availability may vary.

Mitigation:

- Keep dependencies explicit, including required KF6 modules documented in [toolchain-requirements.md](../toolchain-requirements.md) and [ADR 0010](../adrs/0010-kf6-xmlgui-action-collections.md).
- Document supported distribution versions.
- Add packaging notes during M6.

## Performance

Risk:

Repository refresh, GitHub calls, and memory searches can block the UI.

Mitigation:

- Use async jobs.
- Cache metadata.
- Avoid startup network calls.
- Defer expensive refreshes until panels need them.
