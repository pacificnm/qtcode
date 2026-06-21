# First 20 Implementation Tasks

1. Create the root CMake project and executable target.
2. Add Qt 6, KDE Frameworks, QTermWidget, SQLite, and libgit2 dependency detection.
3. Create `src/` module folders and initial library targets.
4. Add `QtCodeApplication` and application metadata.
5. Add `MainWindow` with repository, agent, and terminal panel placeholders.
6. Add logging categories and a minimal diagnostics convention.
7. Implement `StorageService` with SQLite connection management.
8. Implement `MigrationRunner` and the first schema migration.
9. Implement `SettingsService` and basic settings persistence.
10. Implement `ProjectManager` with add/open repository metadata.
11. Implement `GitService` repository validation and current branch detection.
12. Implement changed-file status display.
13. Add `RepositoryListModel` and wire it to `RepositoryPanel`.
14. Integrate QTermWidget in `TerminalPanel`.
15. Implement `TerminalManager` with project-aware terminal creation.
16. Add CLI capability detection for `git`, `gh`, and the first agent CLI.
17. Define `AgentAdapter`, `AgentManager`, and `AgentSession` interfaces.
18. Implement the first CLI agent adapter.
19. Persist agent sessions and messages.
20. ~~Add a basic diff review surface with approve/reject states.~~ Superseded: agent file changes are reviewed through git status, the repository changed-files list, and KTextEditor workspace tabs. Retrieved Context attach/detach is documented in [retrieved context spec](../specs/retrieved-context-spec.md).

## Next Tasks After The First 20

- Add MCP server configuration.
- Implement `MemoryService` health checks.
- Add context retrieval before agent requests.
- Add GitHub CLI repository and issue listing.
- Add pull request listing and context attachment.
- Add terminal profile settings.
- Add migration tests and adapter tests.
- Implement QTCommands discovery, execution, validation, and suggestion per `docs/milestones/m07-qtcommands.md`.
