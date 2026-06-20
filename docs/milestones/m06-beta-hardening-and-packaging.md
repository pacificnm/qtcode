# M6: Beta Hardening And Packaging

## Goal

Make QTCode stable enough for daily dogfooding on KDE/Linux.

## User Outcome

The user can rely on QTCode for routine repository, terminal, agent, GitHub, and memory workflows without frequent crashes or data loss.

## Scope

- Crash and error handling pass.
- Logging and diagnostics.
- Settings backup and migration validation.
- UI polish and keyboard shortcuts.
- Performance pass for startup and repository refresh.
- Packaging investigation for target distributions.
- Documentation for setup and required external tools.

## Engineering Deliverables

- Smoke tests.
- Database migration tests.
- Adapter capability tests.
- Packaging notes.
- User setup guide.

## Exit Criteria

- App starts quickly with multiple saved projects.
- Known missing external tools produce clear UI states.
- SQLite migrations are repeatable.
- Beta setup instructions are accurate.

## Out Of Scope

- Marketplace-style plugin system.
- Cross-platform support outside Linux/KDE.
