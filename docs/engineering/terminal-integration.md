# Terminal Integration Architecture

## Goals

- Make terminal workflows first-class.
- Support multiple tabs.
- Make terminals project-aware.
- Run Git, GitHub CLI, agents, build tools, and tests.
- Persist useful terminal metadata.

## QTermWidget Usage

Each visible terminal tab owns a QTermWidget instance. `TerminalManager` owns logical session state and creates widgets through a controlled factory.

`TerminalManager::configureWidget()` applies the default Konsole-like appearance to every new and restored tab. The preferred bundled scheme is `WhiteOnBlack` (white foreground on a black background). If that scheme is missing from the QTermWidget install, QTCode falls back to `Linux`, then `GreenOnBlack`, and finally the QTermWidget built-in default while logging a warning.

## Session Metadata

Store:

- title
- project ID
- working directory
- shell path
- profile settings
- last command where available
- creation and update timestamps

MVP persistence restores metadata and creates fresh shell processes. True process persistence can be researched later.

## Terminal Launch Flow

```text
User opens terminal for project
  -> TerminalPanel requests session
  -> TerminalManager resolves shell and cwd
  -> QTermWidget starts shell
  -> metadata is stored
```

## Command Launch Flow

```text
Feature requests command
  -> TerminalManager selects or creates terminal
  -> command is inserted or executed
  -> terminal receives focus
```

For commands that should not be interactive, use `ProcessRunner` instead of the terminal.

## Profiles

Project terminal profiles should eventually include:

- shell path
- environment variables
- startup command
- title pattern
- working directory behavior
- color scheme override

Today the global default color scheme is applied centrally in `TerminalManager::configureWidget()` and is not persisted per project.

## Safety

Commands that mutate repositories should be transparent. The terminal is allowed to show exact commands before execution when the action is high risk.
