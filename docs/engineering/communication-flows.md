# Communication Flows

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
  -> AgentManager loads project agent preference
  -> TerminalManager updates default cwd
  -> UI models refresh
```

## Send Agent Prompt

```text
User submits prompt
  -> AgentPanel creates draft request
  -> ContextManager collects project state
  -> MemoryService retrieves context
  -> AgentPanel shows selected context
  -> AgentManager dispatches request
  -> AgentAdapter streams events
  -> AgentSessionRepository persists messages
  -> DiffReviewView shows artifacts
```

## Approve Agent Diff

```text
User approves diff
  -> DiffReviewView emits approval
  -> AgentManager validates artifact
  -> GitService or file patch service applies changes
  -> GitService refreshes status
  -> AgentSession records approval
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

## Run Build Command

```text
User selects build profile
  -> TerminalManager creates or selects terminal
  -> command is shown/executed
  -> output remains visible in TerminalPanel
  -> optional metadata saved
```
