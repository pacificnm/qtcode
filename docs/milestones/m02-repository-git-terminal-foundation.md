# M2: Repository, Git, And Terminal Foundation

## Goal

Make QTCode useful as a project-aware repository and terminal cockpit.

## User Outcome

The user can add a repository, inspect basic Git state, and open terminal tabs rooted in that repository.

## Scope

- Add/open local repositories.
- Persist repositories and recent projects.
- Show active branch and changed files.
- Show simple commit summary.
- Create QTermWidget terminal tabs per project.
- Select default shell.
- Launch common commands in terminals.
- Detect `git`, `gh`, and known agent CLI availability.

## Engineering Deliverables

- `ProjectManager`
- `GitService`
- `TerminalManager`
- Repository list model.
- Changed file model.
- Terminal tab model.
- CLI capability detection.

## Exit Criteria

- Repository panel updates after adding a local repo.
- Changed files refresh on demand.
- Terminal opens in selected repo root.
- Terminal metadata is saved to SQLite.

## Out Of Scope

- Full branch management.
- Agent sessions.
- GitHub issue and pull request details.
