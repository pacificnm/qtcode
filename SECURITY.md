# Security Policy

QTCode is an open-source desktop application and its helper tools may interact with local repositories, GitHub credentials, PostgreSQL, OpenAI APIs, and MCP services. Please report security issues carefully.

## Reporting A Vulnerability

Do not open a public issue for a suspected vulnerability.

Instead, contact the project maintainers privately using the repository's preferred security contact process or GitHub's private vulnerability reporting flow if enabled.

Include:

- a short description of the issue
- the affected component or file
- steps to reproduce
- impact
- any suggested mitigation

## What To Avoid In Reports

- Do not include live API keys, tokens, or private repository URLs if you can avoid it.
- Do not attach unrelated secrets or production credentials.
- Do not paste full exploit payloads into public channels.

## Scope

This policy covers:

- application code
- build and packaging scripts
- local memory and MCP tooling
- Git and GitHub integration
- documentation that affects secure setup or operation

## Response Expectations

We will triage reports as quickly as practical, confirm impact, and coordinate a fix or mitigation before public disclosure when necessary.
