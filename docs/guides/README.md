# Mod-author guides

These guides explain the current ERUI client surface from the point of view of
a client-mod author. They complement the buildable examples: each guide starts
with the player-facing behavior, shows the C++17 API, and then states the
important lifetime, fallback, and failure rules.

If this is your first client, complete the
[getting-started path](../getting-started/README.md) before using these as a
reference.

## Guides by task

| I want to... | Read... |
|---|---|
| Add rows, submenus, built-in destinations, or paginated pages | [Menus, pages, and pagination](menus-and-pages.md) |
| Change a row's text, value, availability, or visibility at runtime | [Row updates](row-updates/README.md) |
| Replace every Numeric or Custom Slider display label without changing its value/index | [Runtime slider display labels](row-updates/slider-labels.md) |
| Give my mod a top-level Configuration tab and custom icon | [Custom Configuration tabs](custom-tabs.md) |
| Add version or status text to a title-screen corner | [Title-screen text](title-screen-text.md) |
| Choose and configure a settings control | [Settings controls](controls/README.md) |
| Check whether the connected host supports a feature | [Capabilities](capabilities.md) |
| Commit updates to several rows atomically | [Atomic row-update batching](row-updates/batching.md) |
| Handle connection, registration, or runtime-operation failures | [Error handling](../reference/error-handling.md) |
| Show a native message or confirmation dialog | [Native dialogs](native-dialogs.md) |
| Translate client text using the selected or configured language | [Game language and localization](localization.md) |
| Declare controller, keyboard, or mouse actions | [Native input bindings](input-bindings.md) |
| Load and save provider configuration | [Provider storage](storage.md) |
| Understand runtime GFX completion, modified-GFX compatibility, or customize titles | [Runtime GFX completion and presentation](presentation-and-gfx.md) |

For one concise inventory of availability and limits, use the
[feature matrix](../features.md). For exact ABI ownership, structure-size,
threading, and error rules, use the [reference](../reference/README.md). The
[research library](../research/README.md) explains how the native behavior was
established; it is not an additional public API.

API versions and ERNativeUI releases are independent. Each feature guide names
the first API that exposes its surface. API 1.4 is the current source contract
and is feature-complete for release preparation, but it is not frozen or
published yet. The stable SDK remains API 1.3. Source being migrated from an
earlier API should follow each permanent
[adjacent migration guide](../migrations/README.md) in order.
