# Database Schema Design

## Purpose

SQLite stores QTCode local state and metadata. It does not store vector embeddings or replace the existing PostgreSQL/pgvector memory system.

## Tables

### `schema_migrations`

```sql
CREATE TABLE schema_migrations (
  version INTEGER PRIMARY KEY,
  name TEXT NOT NULL,
  applied_at TEXT NOT NULL
);
```

### `settings`

```sql
CREATE TABLE settings (
  key TEXT PRIMARY KEY,
  value_json TEXT NOT NULL,
  updated_at TEXT NOT NULL
);
```

### `projects`

```sql
CREATE TABLE projects (
  id TEXT PRIMARY KEY,
  name TEXT NOT NULL,
  root_path TEXT NOT NULL UNIQUE,
  created_at TEXT NOT NULL,
  updated_at TEXT NOT NULL,
  last_opened_at TEXT
);
```

### `repositories`

```sql
CREATE TABLE repositories (
  id TEXT PRIMARY KEY,
  project_id TEXT NOT NULL,
  local_path TEXT NOT NULL,
  remote_url TEXT,
  default_branch TEXT,
  github_owner TEXT,
  github_name TEXT,
  created_at TEXT NOT NULL,
  updated_at TEXT NOT NULL,
  FOREIGN KEY (project_id) REFERENCES projects(id)
);
```

### `agent_configs`

```sql
CREATE TABLE agent_configs (
  id TEXT PRIMARY KEY,
  agent_key TEXT NOT NULL,
  display_name TEXT NOT NULL,
  config_json TEXT NOT NULL,
  enabled INTEGER NOT NULL DEFAULT 1,
  created_at TEXT NOT NULL,
  updated_at TEXT NOT NULL
);
```

### `project_agent_preferences`

```sql
CREATE TABLE project_agent_preferences (
  project_id TEXT PRIMARY KEY,
  agent_key TEXT NOT NULL,
  updated_at TEXT NOT NULL,
  FOREIGN KEY (project_id) REFERENCES projects(id)
);
```

### `agent_sessions`

```sql
CREATE TABLE agent_sessions (
  id TEXT PRIMARY KEY,
  project_id TEXT NOT NULL,
  agent_key TEXT NOT NULL,
  title TEXT NOT NULL,
  status TEXT NOT NULL,
  created_at TEXT NOT NULL,
  updated_at TEXT NOT NULL,
  FOREIGN KEY (project_id) REFERENCES projects(id)
);
```

### `agent_messages`

```sql
CREATE TABLE agent_messages (
  id TEXT PRIMARY KEY,
  session_id TEXT NOT NULL,
  role TEXT NOT NULL,
  content TEXT NOT NULL,
  metadata_json TEXT,
  created_at TEXT NOT NULL,
  FOREIGN KEY (session_id) REFERENCES agent_sessions(id)
);
```

### `context_retrievals`

```sql
CREATE TABLE context_retrievals (
  id TEXT PRIMARY KEY,
  session_id TEXT,
  project_id TEXT NOT NULL,
  query TEXT NOT NULL,
  provider_key TEXT NOT NULL,
  result_count INTEGER NOT NULL,
  metadata_json TEXT,
  created_at TEXT NOT NULL,
  FOREIGN KEY (session_id) REFERENCES agent_sessions(id),
  FOREIGN KEY (project_id) REFERENCES projects(id)
);
```

### `context_results`

```sql
CREATE TABLE context_results (
  id TEXT PRIMARY KEY,
  retrieval_id TEXT NOT NULL,
  source_type TEXT NOT NULL,
  source_uri TEXT,
  title TEXT,
  excerpt TEXT NOT NULL,
  score REAL,
  metadata_json TEXT,
  FOREIGN KEY (retrieval_id) REFERENCES context_retrievals(id)
);
```

### `terminal_sessions`

```sql
CREATE TABLE terminal_sessions (
  id TEXT PRIMARY KEY,
  project_id TEXT,
  title TEXT NOT NULL,
  shell_path TEXT NOT NULL,
  working_directory TEXT NOT NULL,
  profile_json TEXT,
  last_command TEXT,
  created_at TEXT NOT NULL,
  updated_at TEXT NOT NULL,
  FOREIGN KEY (project_id) REFERENCES projects(id)
);
```

### `mcp_servers`

```sql
CREATE TABLE mcp_servers (
  id TEXT PRIMARY KEY,
  name TEXT NOT NULL,
  endpoint TEXT NOT NULL,
  transport TEXT NOT NULL,
  config_json TEXT NOT NULL,
  enabled INTEGER NOT NULL DEFAULT 1,
  created_at TEXT NOT NULL,
  updated_at TEXT NOT NULL
);
```

### `github_cache`

```sql
CREATE TABLE github_cache (
  id TEXT PRIMARY KEY,
  repository_id TEXT NOT NULL,
  object_type TEXT NOT NULL,
  object_key TEXT NOT NULL,
  payload_json TEXT NOT NULL,
  fetched_at TEXT NOT NULL,
  FOREIGN KEY (repository_id) REFERENCES repositories(id),
  UNIQUE(repository_id, object_type, object_key)
);
```

## Data Rules

- Store JSON only for provider-specific details that do not need relational querying.
- Keep secrets out of SQLite when KDE Wallet is available.
- Use ISO-8601 UTC timestamps.
- Use stable text IDs for easier cross-table references and export.
- Add indexes after real query patterns emerge.
