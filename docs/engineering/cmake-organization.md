# CMake Organization

## Goals

- Keep the build modular.
- Make dependencies explicit.
- Support unit testing early.
- Avoid global include/link sprawl.

## Targets

Recommended targets:

```text
qtcode_shared
qtcode_storage
qtcode_core
qtcode_agents
qtcode_terminal
qtcode_git
qtcode_github
qtcode_memory
qtcode_ui
qtcode_app
```

`qtcode_app` builds the executable and links the feature libraries.

## Root CMake Responsibilities

- Set project name and C++ standard.
- Enable Qt automatic MOC/UIC/RCC.
- Find Qt 6 components.
- Find KDE Frameworks components.
- Find QTermWidget.
- Find libgit2.
- Include project CMake helper modules.
- Add subdirectories.

## Dependency Guidance

- Link Qt widgets only where needed.
- Keep `qtcode_shared` free of UI dependencies.
- Keep `qtcode_storage` independent of feature services.
- Keep test targets close to the libraries they validate.

## Tests

Start with:

- storage migration tests
- agent adapter capability tests
- `gh` JSON parsing tests
- Git status parsing or libgit2 mapping tests
- context request composition tests

## Packaging Notes

Packaging should wait until the app shell exists, but dependency detection should be clean from the first build milestone.
