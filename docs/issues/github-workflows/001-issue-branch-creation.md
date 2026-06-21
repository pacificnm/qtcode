# Issue 001: GitHub Issue Branch Create And Checkout

Labels: `type:task`, `component:ui`, `component:github`, `component:git`, `priority:p1`

Milestone: M5 GitHub Workflows / M6 Beta Hardening And Packaging

Source:

- [ADR 0006](../../adrs/0006-git-and-github-cli-first-integration.md)
- [M5 GitHub workflows](../../milestones/m05-github-workflows.md)
- [Git and GitHub integration](../../engineering/git-github-integration.md)

## Task

Let users create and check out GitHub issue-linked branches from the repository panel issues list without leaving QTCode.

## Acceptance Criteria

- [x] Right-clicking an issue without a resolved branch shows **Create Branch** when Git and authenticated `gh` are available.
- [x] Right-clicking an issue with a resolved branch shows **Change Branch** when Git is available.
- [x] `CreateIssueBranchDialog` suggests `{number}-{slug}` branch names and lets the user pick a local base branch.
- [x] Branch creation runs `gh issue develop`, then `git fetch origin <branch>`.
- [x] After creation, the dialog offers **Change Branch** to check out the new branch before closing.
- [x] Existing branches are detected from `gh issue develop --list` and local/remote names matching `{number}-*`.
- [x] **Add to Context** and **Copy Link** remain available on the issues context menu.
- [x] Unit coverage exists in `github_issue_branch_naming` (`GitHubIssueBranchNamingTest`).
