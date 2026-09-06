# ERNativeUI documentation

This is the documentation home for players, mod authors, contributors, and
researchers. If you are new to ERNativeUI, start with the path that matches
what you want to do.

## I want to install ERNativeUI

Follow the [player installation guide](getting-started/player-installation.md)
for requirements, Mod Engine 2 setup, optional GFX assets, runtime
configuration, verification, and troubleshooting.

## I want to create a mod with ERNativeUI

1. Follow the [mod-author getting-started path](getting-started/README.md) to
   set up the SDK, build a minimal client, deploy it, and diagnose common
   integration errors.
2. Check the [supported features](features.md), including API-version and
   release status.
3. Check [game and mod compatibility](compatibility.md) for the running game
   build, other DLLs, and optional loose assets.
4. Understand the independent [release and API versions](versioning.md).
5. Read [how ERNativeUI works](how-it-works.md) for the host/client model,
   registration lifecycle, ownership, and callbacks.
6. Use the task-oriented [mod-author guides](guides/README.md) and concise
   [API reference](reference/README.md) while building real features.
7. Continue from the extensively commented
   [mod template](../examples/template/README.md).

When updating an existing client, use the permanent
[API migration guides](migrations/README.md). The main documentation always
describes the current API; migration guides record only the changes needed
between adjacent API versions. Tagged releases preserve the documentation that
shipped with that version.

Three focused, buildable examples are also available:

- [Localized Greeting](../examples/localized_greeting/README.md) demonstrates
  connection, game-language selection, fallback text, and a native dialog.
- [Tarnished UI Showcase](../examples/tarnished_ui_showcase/README.md)
  demonstrates the broader menu, dialog, presentation, localization, and
  input APIs.
- [Showcase Screenshot Helper](../examples/showcase_screenshot_helper/README.md)
  reads an INI beside its DLL and publishes one isolated English control so
  documentation screenshots can match their minimal code examples.

The public SDK has two header-only client surfaces: a friendly C++17 wrapper
and a strict C ABI. Client mods discover the already-loaded host at runtime;
they do not link an ERNativeUI import library.

## I want to understand or contribute to ERNativeUI

Begin with [how ERNativeUI works](how-it-works.md), then consult the build,
test, and contribution information in the
[contribution index](contributing/README.md). Release history is recorded in
the [changelog](../CHANGELOG.md), while source changes required by client mods
are kept in the [API migration guides](migrations/README.md).

For repository work, use the [contribution index](contributing/README.md).
The verified [release procedure](contributing/releasing.md) explains the
Release Please flow, GitHub asset, and separate local Nexus package.

The [reference](reference/README.md) records exact public contracts. Native
implementation evidence, reproducible tooling, and confidence labels live in
the separate [research library](research/README.md).

## I want to study the native UI research

Start from the [native UI research index](research/README.md). It separates
confirmed observations from inference, documents the reference executable,
and links the reproducible Ghidra workflow and architecture map.

The research library includes reproducible case studies for settings-page
placement and pagination, dialogs, popup choices, TextInput, ColorPicker,
input bindings, GFX presentation, and Steam language discovery. Use the
[supported features](features.md), [mod-author guides](guides/README.md), the
[reference](reference/README.md), and the public headers for current behavior
and contracts; use the research library for native implementation evidence and
reproduction procedures.
