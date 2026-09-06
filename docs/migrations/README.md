# API migration guides

The main ERNativeUI documentation describes the current SDK and API. It is not
duplicated into a separate documentation tree for every release; repository
tags preserve the documentation shipped with each release.

Every new API version adds one focused guide for that adjacent version
transition. These guides are permanent: a mod author may still need
`1.0 -> 1.1` long after `1.2` has been released. If a transition requires no
source changes, its guide says so explicitly rather than leaving a gap in the
version chain.

## Available transitions

| From | To | Status | Guide |
|---|---|---|---|
| 1.0 | 1.1 | Current | [Migrate from API 1.0 to API 1.1](1.0-to-1.1.md) |

Future API versions add another guide instead of rewriting an earlier one. For
example, when an ERNativeUI release introduces API 1.2, this directory will
retain the `1.0 -> 1.1` guide and add a new `1.1 -> 1.2` guide.

Migration is optional when a mod does not need the newer API surface. A mod
author may intentionally keep building against an older SDK/API, and every
later host in the same release-major line must continue supporting it. A host
major-version change may drop earlier-major APIs unless that release
explicitly retains them.

Each guide should contain only what a client-mod author needs to migrate:

- source-level breaking changes and their replacements;
- changed initialization, ownership, or lifetime rules;
- newly required capability checks;
- a minimal before-and-after example;
- build and runtime validation steps; and
- confirmation that clients built against the earlier API retain the
  documented binary compatibility direction.

Historical API reference pages are not kept here. Use repository tags and Git
history when the exact documentation for an older release is required. See
[Versioning](../versioning.md) for why API transitions and ERNativeUI release
numbers are tracked separately.
