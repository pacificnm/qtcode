templateVersion: {{TEMPLATE_VERSION}}
projectName: {{PROJECT_NAME}}
rootPath: {{ROOT_PATH}}
scopeKey: {{SCOPE_KEY}}
installedAt: {{INSTALLED_AT}}
installedBy: qtcode

memory:
  databaseUrl: {{DATABASE_URL}}
  pythonVenv: .venv/bin/python

index:
  includeGlobs:
    - "docs/**"
    - "*.md"
    - "AGENTS.md"
    - ".cursor/rules/**"
  excludeGlobs:
    - ".git/**"
    - ".venv/**"
    - "build/**"
    - "node_modules/**"
