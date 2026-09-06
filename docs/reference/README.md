# ERUI API 1.1 reference

This directory is the reference for the finished, current ERUI API 1.1
contract. It describes what a client mod may rely on rather than how Elden
Ring's private UI implementation happens to provide it.

ERUI API versions and ERNativeUI project releases are independent:

| Name | Current baseline | Meaning |
|---|---:|---|
| ERUI API | **1.1** | The runtime C function-table contract negotiated with `ERUI_GetApi` |
| ERNativeUI release | **1.1.0** | The first project release that packages API 1.1 |

A later project release may continue to expose API 1.1 unchanged. Conversely,
a future API requires an explicit new contract; matching version numbers are
not implied. See [Release versions and public API versions](../versioning.md).

Features discussed elsewhere as possible API 1.2 work are research targets,
not part of API 1.1 and not guaranteed for API 1.2. They may change or move to
a later API while their native ownership and teardown rules are investigated.

## Public surfaces

ERNativeUI exposes two header-only client surfaces:

| Client | Include | Build target | Boundary |
|---|---|---|---|
| C++17 | `<ernativeui/ERNativeUI.hpp>` | `ERNativeUI::SDK` | Friendly values, builders, results, and callback adapters |
| C11 | `<ernativeui/erui.h>` | `ERNativeUI::CABI` | Normative fixed-width C ABI and `ERUI_Result` values |

Neither target links an import library. A client discovers the already-loaded
`ERNativeUI.dll` and resolves its sole export, `ERUI_GetApi`. The C++ wrapper
performs that work through `erui::connect()`.

No C++ class, STL type, exception, native game pointer, or cross-module
allocator ownership crosses the binary boundary. The C header is the
authoritative ABI; the C++ header is an inline convenience layer over it.

## Find the right document

| Need | Read |
|---|---|
| Install the SDK and build a client | [Getting started](../getting-started/README.md) |
| See every supported feature and capability | [Supported features](../features.md) |
| Understand connection, commit, errors, threads, ownership, and limits | [Lifecycle, errors, and limits](lifecycle-errors-and-limits.md) |
| Look up every input value or the exact `ActionInputs` text format | [Input values and codec](input-values-and-codec.md) |
| Add rows, built-in destinations, submenus, or pagination | [Menus, pages, and pagination](../guides/menus-and-pages.md) |
| Use buttons, values, choices, TextInput, or ColorPicker | [Settings controls](../guides/controls/README.md) |
| Show a native message or confirmation dialog | [Native dialogs](../guides/native-dialogs.md) |
| Translate client text using the selected game language | [Game language and localization](../guides/localization.md) |
| Declare controller, keyboard, or mouse actions | [Native input bindings](../guides/input-bindings.md) |
| Load and save provider configuration | [Provider storage](../guides/storage.md) |
| Customize titles or install optional presentation assets | [Presentation and optional GFX](../guides/presentation-and-gfx.md) |
| Diagnose a client that does not appear or connect | [Common errors](../getting-started/common-errors.md) |
| Update API 1.0 source to API 1.1 | [API 1.0 to 1.1 migration](../migrations/1.0-to-1.1.md) |

## API map

| Task | C++17 surface | Strict-C surface |
|---|---|---|
| Connect | `erui::connect()` and `Connection` | `GetModuleHandleW`, `GetProcAddress`, `ERUI_GetApi` |
| Register one provider | `Connection::register_menu` | `register_provider`, `commit_provider`, `abort_provider` |
| Select a destination | `Menu::root`, `Menu::page` | root handle, `get_builtin_page` |
| Build pages and rows | `Page::add_*` | `add_*` function-table entries |
| Read or update row state | `Registration` | provider/row handles and typed getters/setters |
| Read the game language | `Connection::game_language` | `get_game_language` |
| Queue a native alert | `Registration::alert` | `enqueue_alert` |
| Declare and update bindings | `Menu::input_bindings`, `InputAction` | input-section/action entries and `ERUI_ActionInputs` |
| Persist provider data | `Menu::storage`, `Storage`, `StorageSection` | `open_storage` and `storage_*` entries |

## Contract in one minute

1. Mod Engine 2 loads one shared `ERNativeUI.dll` before ordinary clients.
2. Each client leaves `DllMain` quickly and connects from its own worker.
3. `erui::connect()` requests API 1.1 exactly and waits only while the host is
   still starting.
4. `Connection::register_menu` builds a private provider draft and commits it
   only if every builder operation succeeds.
5. The host merges all committed providers, validates the required native
   interfaces, installs one coordinated hook set, and then releases successful
   commit calls.
6. Menu topology is fixed for the process. Retained handles can update values,
   show alerts, manage bindings, and use storage, but cannot add rows later.
7. Committed callback code and state remain valid until process exit. A client
   provider and the host cannot be hot-unloaded safely.

For the exact ownership, callback-thread, error, and compatibility rules behind
those steps, continue with
[Lifecycle, errors, and limits](lifecycle-errors-and-limits.md).
