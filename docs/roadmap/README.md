# ERNativeUI development roadmap

This area tracks work that is being considered or developed after the current
stable release. It exists so players, mod authors, testers, and contributors
can follow progress without confusing development plans with published API
contracts.

The current stable contract is **ERUI API 1.3**, first published by
ERNativeUI 1.3.0. Everything in this roadmap remains unavailable to ordinary
client mods until a compatible release publishes it.

## Active roadmap

| Target | Theme | Status | Roadmap |
|---|---|---|---|
| ERUI API 1.4 | Reactive native menus: atomic updates, recursive Collapse controls, conditional content, repagination, and tab visibility | Proposed; alpha infrastructure is implementing, while feature research and ABI work have not started | [API 1.4 roadmap](api-1.4.md) |

## Status vocabulary

Roadmap entries use evidence-based states rather than percentages:

| Status | Meaning |
|---|---|
| **Proposed** | The user-facing goal is recorded, but its native boundary and public API are not proven. |
| **Researching** | Static analysis, GFX inspection, or a bounded runtime probe is in progress. |
| **Native behavior proven** | The required native behavior and failure boundaries have reproducible evidence. This is not yet a public feature. |
| **Implementing** | Production code and automated tests are being developed behind an unstable contract. |
| **Alpha** | A manually selected, working but incomplete milestone is available for opt-in testing under the [alpha policy](../contributing/alpha-builds.md). |
| **Release candidate** | The API shape is frozen for final compatibility, documentation, and live validation. |
| **Released** | The capability is part of a tagged release and has moved to the feature matrix, guides, migration documentation, and What's New archive. |
| **Deferred** | Work is intentionally postponed because a prerequisite or proof gate is missing. |
| **Rejected** | Evidence showed that the proposed design is unsafe, incompatible, or does not fit ERNativeUI. |

## What this roadmap is not

- It is not a compatibility promise or permission for a client mod to depend
  on an unreleased function.
- It does not replace the authoritative [feature matrix](../features.md),
  [API reference](../reference/README.md), or public headers.
- It does not replace the release-focused [What's New archive](../whats-new/README.md),
  generated changelog, or stable migration guides.
- A feature may change, move to a later API, or be rejected when native
  lifecycle, coexistence, or game-update validation fails.

Development builds are deliberately separate from stable releases. Read the
[alpha build policy](../contributing/alpha-builds.md) before installing or
publishing anything from an API-development branch.

