# Project Structure

Recommended source tree:

```text
qtcode/
  CMakeLists.txt
  README.md
  PROMPT.md
  docs/
  cmake/
    QtCodeCompilerOptions.cmake
    QtCodeDependencies.cmake
  src/
    app/
      main.cpp
      QtCodeApplication.h
      QtCodeApplication.cpp
    ui/
      MainWindow.h
      MainWindow.cpp
      panels/
        RepositoryPanel.h
        AgentPanel.h
        TerminalPanel.h
      models/
        RepositoryListModel.h
        GitStatusModel.h
        AgentSessionModel.h
        TerminalTabModel.h
      views/
        ContextResultsView.h
    core/
      ApplicationController.h
      AppConfigService.h
      ProjectManager.h
      ContextManager.h
      SettingsService.h
    agents/
      AgentAdapter.h
      AgentManager.h
      AgentSession.h
      adapters/
        CodexAgentAdapter.h
        ClaudeAgentAdapter.h
        AiderAgentAdapter.h
    terminal/
      TerminalManager.h
      TerminalSession.h
      TerminalProfile.h
    git/
      GitService.h
      GitRepository.h
      GitStatus.h
    github/
      GitHubService.h
      GhCliClient.h
      GitHubModels.h
    memory/
      MemoryService.h
      McpClient.h
      ContextResult.h
    storage/
      StorageService.h
      MigrationRunner.h
      repositories/
    settings/
      SettingsModels.h
    shared/
      Result.h
      ProcessRunner.h
      Logging.h
  tests/
    unit/
    integration/
  resources/
    icons/
    qtcode.qrc
```

## Dependency Direction

Allowed direction:

```text
ui -> core -> integrations -> shared
core -> storage
integrations -> shared
storage -> shared
```

Avoid:

- `core` depending on concrete UI widgets.
- agent adapters depending on panels.
- storage depending on GitHub, Git, memory, or terminal code.
- circular library dependencies.

## Notes

- Keep public interfaces small and explicit.
- Group feature-specific models near their feature unless they are genuinely shared.
- Keep CLI parsing in integration modules, not in UI code.
