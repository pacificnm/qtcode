# M5: GitHub Workflows

## Goal

Bring GitHub issues and pull requests into the cockpit without implementing early OAuth complexity.

## User Outcome

The user can inspect GitHub repositories, issues, and pull requests for the active project and use them as context for terminal and agent workflows.

## Scope

- GitHub CLI authentication detection.
- Repository remote to GitHub repository resolution.
- List issues and pull requests.
- View issue and PR details.
- Open issue/PR context in agent sessions.
- Create and checkout issue-linked branches from the issues list.
- Launch `gh` commands from terminal workflows where appropriate.
- Cache lightweight GitHub metadata in SQLite.

## Engineering Deliverables

- `GitHubService`
- `GhCliClient`
- `GitHubIssueBranchNaming`
- `CreateIssueBranchDialog`
- GitHub repository model.
- Issue model.
- Pull request model.
- Issue branch list/develop result models.
- Error states for missing or unauthenticated `gh`.

## Exit Criteria

- Active repository shows related GitHub issues and PRs when available.
- Issue/PR content can be attached to agent context.
- Users can create an issue branch with `gh issue develop`, fetch it locally, and optionally check it out from the issues context menu.
- Existing issue branches are detected from GitHub-linked branches and local/remote names matching `{number}-*`.
- GitHub features degrade gracefully when `gh` is unavailable.

## Out Of Scope

- GitHub REST API background sync.
- OAuth device flow.
- Full PR review UI.
