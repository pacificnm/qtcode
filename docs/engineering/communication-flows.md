# Communication Flows

## Application Startup

```text
QtCodeApplication starts
  -> ApplicationController::initialize()
  -> AppConfigService loads KDE startup config
  -> StorageService opens SQLite
  -> MigrationRunner applies pending migrations
  -> AgentManager registers built-in adapters
  -> AgentManager restores persisted agent sessions
  -> CliCapabilityService schedules background CLI detection
  -> ProjectManager restores active project (unless disabled in settings)
  -> TerminalManager restores terminal metadata
  -> MainWindow constructs TerminalPanel and restores or creates initial tabs
  -> MainWindow applies AppConfig column widths and SQLite collapse/selection state
  -> AgentPanel restores project session list and default agent selector
  -> CliCapabilityService completes and updates agent/GitHub executable paths
```

## Open Repository

```text
User selects Add Repository
  -> RepositoryPanel emits add request
  -> ProjectManager validates path
  -> GitService verifies repository
  -> StorageService persists project/repository
  -> RepositoryListModel updates
  -> TerminalManager prepares project terminal defaults
```

## Select Active Repository

```text
User selects repository
  -> ProjectManager sets active project
  -> GitService refreshes status
  -> GitHubService resolves remote
  -> AgentPanel::onActiveProjectChanged()
  -> AgentPanel refreshes agent selector (repo default from .qtcode/config.yaml when needed)
  -> AgentManager restores last active session for project or creates one
  -> TerminalPanel::onActiveProjectChanged()
       TerminalManager::syncSessionsToActiveProject() updates cwd/title metadata
       create first tab when none exist, otherwise sync open tabs
  -> UI models refresh
```

## Send Agent Prompt

```text
User submits prompt
  -> AgentPanel starts async ContextManager retrieval
  -> ContextResultsView shows deduplicated results (manual SourceFile entries preserved)
  -> AgentPanel dispatches with attachedContextExcerpts() from checked rows
  -> AgentManager persists retrieval metadata (live view not replaced)
  -> AgentAdapter streams events
  -> AgentSessionRepository persists messages
  -> RepositoryPanel git status reflects agent file changes
```

See [retrieved context spec](../specs/retrieved-context-spec.md).

## Review Agent File Changes

```text
Agent modifies files externally
  -> GitService refreshes repository status
  -> User inspects changed files in RepositoryPanel
  -> User opens files in WorkspaceTabs for review
```

## List GitHub Issues

```text
Repository selected
  -> GitHubService resolves owner/name
  -> GhCliClient runs gh issue list with JSON
  -> GitHubService normalizes issue models
  -> GitHub cache updates
  -> RepositoryPanel displays issues
```

## Create GitHub Issue Branch

```text
User right-clicks issue in RepositoryPanel
  -> RepositoryPanel::resolveIssueBranchName()
       GitHubService::listIssueLinkedBranchesForProject (when gh is ready)
       GitService::listRepositoryBranchReferences
       GitHubIssueBranchNaming::resolveIssueBranchName
  -> Menu shows Create Branch when no branch is resolved
  -> CreateIssueBranchDialog opens with suggested branch name and local base-branch list
  -> User confirms Create Branch
  -> GitHubService::developIssueBranchForProject (gh issue develop)
  -> GitService::fetchRemoteBranch (git fetch origin <branch>)
  -> Dialog offers Change Branch to check out locally
  -> RepositoryPanel refreshes git status on checkout
```

## Checkout GitHub Issue Branch

```text
User right-clicks issue with an existing branch
  -> RepositoryPanel shows Change Branch in the issues context menu
  -> GitService::checkoutRemoteBranch runs asynchronously
  -> StatusService reports success or error
  -> RepositoryPanel refreshes git status
```

## Open Terminal Tab

```text
User clicks terminal + button or File > New Terminal Tab
  -> TerminalPanel::addTerminalTab()
  -> TerminalManager::createTerminal(activeProjectId)
       buildSessionForProject() resolves shell, cwd, title
       configureWidget() starts shell in PTY cwd
       persist TerminalSession row
  -> TerminalPanel adds tab and focuses widget
```

## Restore Terminal Sessions

```text
TerminalPanel startup (after TerminalManager::restoreState())
  -> for each persisted TerminalSession:
       resolveSessionWorkingDirectory() re-canonicalizes cwd from repo config
       restoreTerminal() creates QTermWidget + fresh shell
       tab title shows "<display name> (restored)"
  -> if no sessions and active project exists:
       createTerminal() for active project
```

## Save Repository Terminal Settings

```text
User edits Project display name / Project path in File > Settings
  -> RepoConfigWriter saves .qtcode/config.yaml
  -> MainWindow calls TerminalManager::syncSessionsToActiveProject()
  -> TerminalPanel::syncTabsFromSessions() updates titles and PTY cwd
```

## Run Build Command

```text
User selects build profile
  -> TerminalManager creates or selects terminal
  -> command is shown/executed
  -> output remains visible in TerminalPanel
  -> optional metadata saved
```
