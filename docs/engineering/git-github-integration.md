# Git And GitHub Integration Architecture

## Local Git Goals

- Show repository status quickly.
- Support branch, tag, changed file, commit, and diff views.
- Keep destructive operations explicit.
- Preserve compatibility with command-line Git behavior.

## Git Implementation

Use `GitService` as the only public entry point.

Recommended backend split:

- libgit2 for repository discovery, status, branch metadata, and read-only fast paths.
- Git CLI for operations where user expectations and config behavior matter.

## Git Models

- `GitRepository`
- `GitStatus`
- `GitBranch`
- `GitTag`
- `GitCommitSummary`
- `GitDiff`
- `ChangedFile`

## GitHub Goals

- Use the user's existing GitHub CLI authentication.
- Surface repositories, issues, and pull requests.
- Attach GitHub content to agent sessions as context.
- Avoid REST API complexity in MVP.

## GitHub Implementation

Use `GitHubService` and `GhCliClient`.

Prefer `gh` JSON output:

- `gh repo view --json ...`
- `gh issue list --json ...`
- `gh issue view --json ...`
- `gh pr list --json ...`
- `gh pr view --json ...`

Issue branch workflows also use `gh issue develop`:

- `gh issue develop --list <number> --repo <owner/name>` — linked branches for an issue.
- `gh issue develop <number> --repo <owner/name> --base <branch> --name <branch>` — create and link a branch on GitHub.

After branch creation, QTCode fetches the remote branch with Git CLI (`git fetch origin <branch>`) and can check it out locally through `GitService`.

## Issue Branch Detection And Naming

`GitHubIssueBranchNaming` provides shared helpers:

- `suggestedIssueBranchName(issueNumber, title)` — `{number}-{slugified-title}` with a 60-character slug cap.
- `matchingIssueBranches(issueNumber, branchNames)` — local or remote names equal to the issue number or starting with `{number}-`.
- `resolveIssueBranchName(...)` — prefers GitHub-linked branches, then repository branch matches.

`RepositoryPanel` uses these helpers to decide whether the issues context menu shows **Create Branch** or **Change Branch**.

## Repository Mapping

Repository remote URLs should map to GitHub owner/name when possible.

Supported forms:

- `git@github.com:owner/name.git`
- `https://github.com/owner/name.git`
- `https://github.com/owner/name`

## Error States

- `gh` is not installed.
- user is not authenticated.
- remote is not GitHub.
- repository is private and inaccessible.
- rate limit or network failure.
- JSON output cannot be parsed.

## Safety

Destructive actions, branch switches with dirty worktrees, pushes, force pushes, rebases, and patch application must use explicit confirmation flows.

Issue branch checkout follows the same Git CLI path as other branch switches. Dirty-worktree protection is delegated to Git command failures surfaced through `StatusService`.
