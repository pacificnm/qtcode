# ADR 0013: Command Library Is A Persistent Repo-Native Right Panel

## Status

Accepted

## Context

QTCode needs a UI surface for repo-native commands that is easy to discover, explicit to use, and
safe to review before generating changes.

The Command Library must show repository-owned commands, templates, rules, examples, and validation
in a way that makes it obvious the content belongs to the active repository. The surface also needs
to support command initialization for repositories that do not yet have a `.qtcode/` tree.

Two common alternatives do not fit the product:

- A modal dialog is too transient for a repository artifact the user may return to repeatedly.
- An editor-local or provider-specific command browser would hide the repository source of truth
  behind local settings or a single AI integration.

## Decision

Expose the Command Library as a persistent right-side panel in QTCode, opened from the right-side
Action icon and closed manually by the user.

The panel will:

- remain visible until the user closes it
- show the active repository name or path in the header
- browse `.qtcode/commands`, `.qtcode/templates`, `.qtcode/rules`, `.qtcode/examples`, and
  `.qtcode/validation`
- provide an initialization flow that creates the `.qtcode/` scaffold when it is missing
- keep command execution reviewable through preview and validation before apply

The repository remains the source of truth. The panel is a browser and executor for repo-native
artifacts, not the storage location for them.

## Consequences

Positive:

- Users can find the Command Library in the same shell area as other workflow panels.
- Command definitions remain obviously tied to the active repository.
- The UI supports repeated review and reuse without hiding behind a transient dialog.
- The same panel can later host related repository-native agent tools.

Tradeoffs:

- The right column must remain available for a persistent panel surface.
- The panel needs explicit empty-state handling for repositories without `.qtcode/`.
- Command initialization must avoid overwriting repository-owned files.

## Follow-Ups

- Document the feature in `docs/features/command-library.md`.
- Keep the schema and validation rules in `docs/specs/qtcommands-spec.md`.
- Keep the panel interaction model in `docs/design/command-library-panel.md`.
- Update the right-panel layout spec so the activity bar and panel list include Command Library.

