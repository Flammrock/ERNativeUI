# Mod-author guides

These guides explain the current ERUI API 1.1 from the point of view of a
client-mod author. They complement the buildable examples: each guide starts
with the player-facing behavior, shows the C++17 API, and then states the
important lifetime, fallback, and failure rules.

If this is your first client, complete the
[getting-started path](../getting-started/README.md) before using these as a
reference.

## Guides by task

| I want to... | Read... |
|---|---|
| Add rows, submenus, built-in destinations, or paginated pages | [Menus, pages, and pagination](menus-and-pages.md) |
| Choose and configure a settings control | [Settings controls](controls/README.md) |
| Check whether the connected host supports a feature | [Availability and capabilities](availability.md) |
| Show a native message or confirmation dialog | [Native dialogs](native-dialogs.md) |
| Translate client text using Elden Ring's selected language | [Game language and localization](localization.md) |
| Declare controller, keyboard, or mouse actions | [Native input bindings](input-bindings.md) |
| Load and save provider configuration | [Provider storage](storage.md) |
| Install optional GFX, customize titles, or reproduce the presentation patch | [Presentation and optional GFX assets](presentation-and-gfx.md) |

For one concise inventory of availability and limits, use the
[feature matrix](../features.md). For exact ABI ownership, structure-size,
threading, and error rules, use the [reference](../reference/README.md). The
[research library](../research/README.md) explains how the native behavior was
established; it is not an additional public API.

API versions and ERNativeUI releases are independent. The current guides cover
API 1.1 even when a later maintenance release still exposes that same API.
Source being migrated from API 1.0 should first follow the permanent
[1.0 to 1.1 guide](../migrations/1.0-to-1.1.md).
