# ADR 0001: Build QTCode As A Native Qt/KDE Desktop Application

## Status

Accepted

## Context

QTCode targets Linux and KDE Plasma users who want a fast, local, low-memory developer cockpit. The project explicitly avoids becoming another Electron application or browser-based IDE shell.

## Decision

Build QTCode with C++20, Qt 6, CMake, and KDE Frameworks. Prefer native widgets and KDE integration. Do not use Electron, Chromium, Monaco, or web-first UI frameworks.

## Consequences

Positive:

- Faster startup and lower idle memory than an Electron shell.
- Better integration with KDE settings, actions, shortcuts, file dialogs, and theming.
- Cleaner fit for QTermWidget and KTextEditor.

Tradeoffs:

- Fewer ready-made web UI components.
- Requires careful Qt model/view design.
- Packaging needs distribution-aware dependency handling.

## Follow-Ups

- Confirm required KDE Frameworks modules during CMake skeleton work.
- Keep UI implementation widget-first unless a specific Qt Quick use case appears.
