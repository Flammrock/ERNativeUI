# Provider storage

API 1.1 provides an optional host-owned, provider-scoped INI document for mods
that do not already have a configuration backend. The C++17 wrapper supports
UTF-8 strings, booleans, numbers, and input assignments; strict C exposes raw
UTF-8 values plus dedicated input-assignment operations. `ERNativeUI.dll`
owns the implementation and in-memory document; the client explicitly chooses
when to load, change memory, and save to disk.

Storage is available through both public client surfaces:

- strict C uses the API 1.1 `open_storage` and `storage_*` function-table
  members; and
- C++17 uses `Menu::storage`, `Storage`, and `StorageSection`.

> **Storage is host-backed, not standalone.** `ERNativeUI.dll` must be loaded,
> the API 1.1 connection must succeed, and the client must begin provider
> registration before it can open storage. If the host is absent or
> `erui::connect()` fails, ERNativeUI Storage is unavailable in both C and
> C++. A mod that must load configuration without the host should retain its
> own storage backend. Only the public `ActionInputs` text codec is
> header-only and usable without connecting to the host.

Storage requires a capability check and was not present in API 1.0. See the
[feature matrix](../features.md) and [versioning guide](../versioning.md).

## Open, load, and save a document

Open storage while the provider registration callback is active. The returned
handle may also be copied into client state and used after registration
commits successfully.

```cpp
// Inside connection.value().register_menu(...):
if (menu.supports(erui::Capability::storage)) {
    erui::Storage config = menu.storage();

    if (config.load() == ERUI_OK) {
        erui::StorageSection general = config.section("general");
        erui::StorageRead<bool> enabled = general.get<bool>("enabled");

        if (enabled.found()) {
            use_enabled(enabled.value());
        } else if (enabled.result() == ERUI_NOT_FOUND) {
            use_enabled(true); // Mod-owned default.
        }

        if (general.set("enabled", true) == ERUI_OK) {
            const ERUI_Result save_result = config.save();
            if (save_result != ERUI_OK) {
                // Report the failed disk write or retry later.
            }
        }
    }
}
```

ERNativeUI Storage is an in-memory configuration document with disk backing:

| Operation | Effect |
|---|---|
| `menu.storage()` | Open the provider's one in-memory document handle |
| `load()` | Read the backing file into memory |
| `get()` | Read memory only |
| `set()`, `erase()`, `apply()` | Change memory only |
| `save()` | Atomically replace the complete backing file when dirty |

Nothing is loaded or saved automatically, including at destruction or process
shutdown. A missing file loads successfully as an empty document and is not
created merely by loading it. Call `load()` before any read, mutation, or
save. A second load reports `ERUI_STORAGE_ALREADY_LOADED`; saving before load
reports `ERUI_STORAGE_NOT_LOADED`; saving a clean document succeeds without a
write.

## Choose the storage location

The default call:

```cpp
erui::Storage config = menu.storage();
```

resolves beside the shared host as:

```text
ERNativeUI.dll
mods/
+-- my-mod/
    +-- config.ini
```

The default directory uses the provider ID verbatim when that cannot create a
Windows path alias. Otherwise it uses an isolated `~<sha256>` directory so
distinct provider IDs cannot share a file.

Two explicit alternatives are available:

```cpp
erui::Storage beside_client = menu.storage(
    erui::StorageOptions::beside_module(L"config/my-mod.ini"));

erui::Storage absolute = menu.storage(
    erui::StorageOptions::at(L"D:\\ModConfigs\\my-mod.ini"));
```

`beside_module` requires a relative path that stays beneath the client DLL's
directory. `at` requires an absolute normalized path without parent
components. One provider owns at most one configuration document: opening the
same resolved path returns the same handle, while asking that provider for a
different path is rejected. A custom path should be exclusive to that
provider; independently implemented configuration writers do not merge
changes.

For a compact example that reads one human-edited setting from an INI beside
its own DLL, see the
[Showcase Screenshot Helper](../../examples/showcase_screenshot_helper/README.md).
It selects one documentation page at startup and demonstrates the missing-file
fallback without introducing a separate configuration library.

## Store typed values

Sections support UTF-8 strings, booleans, integral values, floating-point
values, and `ActionInputs`:

```cpp
erui::StorageSection general = config.section("general");

ERUI_Result result = general.set("enabled", true);
if (result == ERUI_OK) {
    result = general.set("intensity", 75);
}
if (result == ERUI_OK) {
    result = general.set("profile", "balanced");
}

erui::StorageRead<bool> enabled = general.get<bool>("enabled");
if (enabled.found()) {
    use_enabled(enabled.value());
} else if (enabled.result() == ERUI_NOT_FOUND) {
    use_enabled(true); // Mod-owned default.
}

general.erase("obsolete-key"); // Missing keys are a successful no-op.
config.save();
```

Malformed typed text reports `ERUI_STORAGE_FORMAT_ERROR`; disk failures report
`ERUI_STORAGE_IO_ERROR`. Section and key names use 1 through 255 ASCII letters,
digits, `.`, `_`, `-`, and internal spaces, with no leading or trailing space
and no `.` or `..` identifier. Values are bounded UTF-8 without NUL, CR, or
LF. The strict limits for values, file size, decoded document size, sections,
entries, and assignment batches are published as `ERUI_STORAGE_MAX_*` in
[`erui.h`](https://github.com/Flammrock/ERNativeUI/blob/main/include/ernativeui/erui.h).

## Persist input assignments

Input Bindings and Storage are separate features. An action works from its
declared defaults without opening storage, and opening storage does not save
assignments automatically. Read the [input-bindings guide](input-bindings.md)
before adding persistence.

The recommended flow is:

1. Declare the action defaults.
2. Load the optional sparse overrides.
3. Apply a saved override to the live action.
4. Explicitly opt into saving later player changes.

The following excerpt assumes `bindings` and `action` were created as shown in
the input-bindings guide:

```cpp
if (menu.supports(erui::Capability::storage)) {
    erui::Storage config = menu.storage();

    if (config.load() == ERUI_OK) {
        erui::StorageSection saved = config.section("bindings");
        erui::StorageRead<erui::ActionInputs> override_value =
            saved.get<erui::ActionInputs>("toggle-overlay");

        if (override_value.found()) {
            action.bind(override_value.value());
        } else if (override_value.result() != ERUI_NOT_FOUND) {
            // Report malformed or unreadable configuration if desired.
        }

        // Explicit opt-in: apply each future player event in memory and save
        // it. Omitting this line keeps assignments process-local.
        bindings.persist_assignments_to(saved);
    }
}
```

`persist_assignments_to` stores a safe copy of the lightweight
`StorageSection` handle for the process-lifetime callback. Internally it calls
`saved.apply(event)` and then `saved.config().save()`. `apply(event)` validates
the complete batch and changes memory atomically. It preserves sparse intent:

- a player assignment stores the changed slot;
- a player Clear stores explicit `unbound`;
- when no overrides remain, the action key is erased.

The current native binding screens emit assignment and Clear events. API 1.1
also reserves a Reset-to-Defaults event reason; if a future path supplies that
reason, `apply(event)` removes the affected override so the declared default
is inherited. Programmatic binding resets are silent, so a mod-provided Reset
command that resets the complete action must erase that action's saved
override and call `save()` itself. A per-device reset must instead remove only
that device's sparse override and preserve the other device slots.

For custom error reporting or filtering, install a manual handler instead:

```cpp
void persist_changes(
    erui::StorageSection& saved,
    const erui::AssignmentsChangedEvent& event) noexcept
{
    if (saved.apply(event) == ERUI_OK) {
        const ERUI_Result result = saved.config().save();
        if (result != ERUI_OK) {
            // The in-memory document remains dirty; report or retry later.
        }
    }
}

bindings.on_assignments_changed<&persist_changes>(saved);
```

The complete template uses this explicit pattern in
[`mod_main.cpp`](../../examples/template/mod_main.cpp).

## Use another configuration backend

A mod may keep its existing configuration system. The public headers provide
a stable text codec for `ActionInputs`, so another INI, JSON, or database
backend does not need to store ERNativeUI's private numeric representation.
The codec itself works even when `ERNativeUI.dll` is absent or connection
fails.

Read [Input values and the `ActionInputs` codec](../reference/input-values-and-codec.md)
for the canonical format and strict C and C++ helpers. The
[input-bindings guide](input-bindings.md#persist-assignments-with-another-configuration-backend)
shows the complete custom-backend flow.

## Lifetime, failure, and fallback rules

- `menu.storage()` and strict C `open_storage` may be called only while the
  API 1.1 provider registration draft is open. The returned `Storage` and
  `StorageSection` values are lightweight live handles that may be copied and
  retained after registration commits successfully.
- If the host is absent, connection fails, Storage is unavailable, or
  `load()` fails, use mod-owned defaults or a client-owned configuration
  backend. Do not prevent an otherwise independent mod feature from running.
- Storage methods execute synchronously on the calling client thread and
  return `ERUI_Result`. Do not discard I/O and format errors where persistence
  matters.
- A provider owns at most one resolved storage document. Serialize any
  higher-level policy that combines related client state and storage
  operations.
- Nothing saves automatically. `set`, `erase`, and `apply` only change the
  loaded in-memory document; call `save()` explicitly.
- A successfully committed provider module is pinned until process exit.
  Hot-unloading is unsupported.

Strict-C clients use the API 1.1 table's `open_storage`, `storage_load`,
`storage_save`, UTF-8 read/write, erase, `ActionInputs`,
assignment-application, and information members. Initialize every
size-prefixed structure, keep flags and reserved fields zero, and honor the
same provider, handle, and borrowed-view lifetimes. These are host calls, not
header-only storage functions.

The
[strict-C header](https://github.com/Flammrock/ERNativeUI/blob/main/include/ernativeui/erui.h)
is the normative ABI. The
[C++17 wrapper](https://github.com/Flammrock/ERNativeUI/blob/main/include/ernativeui/ERNativeUI.hpp)
provides the typed values and handle conveniences used above.

## Implementation note

Provider Storage is an ERNativeUI service rather than a reverse-engineered
Elden Ring UI facility. The
[native input-bindings case study](../research/case-studies/input-bindings.md)
discusses storage only where native assignment events meet sparse
persistence.

Return to the [documentation home](../README.md), return to
[native input bindings](input-bindings.md), or continue with
[menus, pages, and pagination](menus-and-pages.md).
