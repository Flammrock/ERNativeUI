# Common client-mod errors

This guide is for authors whose client DLL uses ERNativeUI but does not produce
the expected menu or callback behavior. Start by preserving the full C++
`Error`—its `code()`, `native_result()`, and `message()`—or the raw-C
`ERUI_Result`. “The menu did not appear” alone cannot distinguish host loading,
API negotiation, provider validation, and native hook installation.

ERNativeUI exposes two public API generations:

- **Released API 1.0:** the frozen SDK shipped with ERNativeUI 1.0.0. Any
  client built with it remains supported by later 1.x hosts.
- **Current API 1.1:** introduced with ERNativeUI 1.1.0. It provides explicit
  `erui::connect()`, TextInput, ColorPicker, additional built-in pages, input
  bindings, and provider storage.

See [Feature status](../features.md) for the exact boundary and
[How ERNativeUI works](../how-it-works.md) for the lifecycle and ownership
model.

## The client reports that `ERNativeUI.dll` was not loaded

**Symptom**

- API 1.1 `erui::connect()` returns `ErrorCode::host_not_loaded` with
  `ERUI_HOST_NOT_READY` after its timeout.
- A released API 1.0 client reports the equivalent missing-host error.
- The gameplay part of the mod works, but its ERNativeUI menu is absent.

**Likely cause**

The host is missing from Mod Engine 2's `external_dlls`, the path is wrong, the
DLL was renamed, Windows could not load it, or the client was loaded without
the host. The C++ wrapper uses `GetModuleHandleW(L"ERNativeUI.dll")`; it does
not load the host itself. A strict-C client must perform equivalent discovery
before calling the exported `ERUI_GetApi` function.

**Fix**

1. Keep the canonical filename `ERNativeUI.dll`.
2. Put one host in the Mod Engine 2 configuration before ordinary client DLLs.
3. Verify every path relative to the directory from which Mod Engine 2 reads
   its configuration.
4. Do not call `LoadLibrary` from the client and do not bundle a private host
   copy with each mod.
5. If Solid Uncapper is present, use the special order below.

```toml
external_dlls = [
    "mod\\Solid Uncapper\\Solid Uncapper.dll", # only when installed
    "mod\\ERNativeUI\\ERNativeUI.dll",
    "mod\\MyMod\\MyMod.dll"
]
```

Solid Uncapper must precede the host, and the host must precede clients. DLL
list order cannot serialize every worker-thread initialization, but it gives
ERNativeUI the intended modules to discover and its compatibility path handles
the remaining asynchronous hook order.

## The host is present, but `connect()` times out or fails

**Symptom**

- `ErrorCode::host_not_ready`: the host was seen, but API 1.1 did not become
  ready before the client-side timeout.
- `ErrorCode::host_failed`: host initialization failed.
- `ErrorCode::export_not_found`: the loaded module named `ERNativeUI.dll` has
  no `ERUI_GetApi` export.

**Likely cause**

API 1.1 negotiation remains temporarily `ERUI_HOST_NOT_READY` while the host
settles Steam language readiness. `erui::connect()` already retries that state
every 25 ms for up to 30 seconds by default. A final timeout therefore usually
means the wrong DLL was loaded or host startup did not settle; adding an
arbitrary sleep in the client is not the normal fix. `host_failed` means the
host reached a terminal initialization error. A missing export almost always
means a renamed/unrelated/obsolete binary was selected.

**Fix**

- Call `erui::connect()` once from a startup worker and log the complete error.
- Confirm the loaded host version and remove duplicate or obsolete copies.
- Enable the host log as described below, restart the game, and inspect the
  first error—not only the final `HOST_FAILED` result.
- For raw C API 1.1 clients, retry only `ERUI_HOST_NOT_READY` from a worker.
  Treat every other result as terminal. Released API 1.0 clients deliberately
  use their frozen lifecycle and should not be rewritten to imitate API 1.1
  negotiation.

## `find_package(ERNativeUI 1.1.0)` rejects an installed SDK

**Symptom**

CMake finds ERNativeUI but says package version 1.1.0 is incompatible or
unavailable.

**Likely cause**

The installed ERNativeUI package release is older than 1.1.0, its CMake files
are missing, or `CMAKE_PREFIX_PATH` points at the wrong prefix. The `1.1.0` in
`find_package` is a minimum release-package version; it is not public API
negotiation.

**Fix**

- Install the ERNativeUI 1.1.0 SDK or a later release that documents API 1.1
  support, then point `CMAKE_PREFIX_PATH` at the directory containing its
  `include`, `lib`, and `bin` children.
- Alternatively, set `ERNativeUI_DIR` to the exact
  `lib\cmake\ERNativeUI` directory containing
  `ERNativeUIConfig.cmake`. Use one discovery method, then delete the CMake
  build directory before retrying if it cached an older package location.
- When working from source, use `add_subdirectory` or build the client inside
  the matched tree instead of pretending an older installed package is newer.
- Do not remove the package-version requirement to hide a mismatched SDK. See
  [Versioning](../versioning.md) for the two independent version lines.

## The API version or capability is rejected

**Symptom**

- `ERUI_UNSUPPORTED_VERSION`, `ErrorCode::incompatible_api`, or “API 1.1 is
  required; update ERNativeUI.dll.”
- A raw-C function pointer is null or an operation returns
  `ERUI_NOT_SUPPORTED`.
- A current API 1.1 client works with the current host but not a host exposing
  only API 1.0.

**Likely cause**

Version negotiation selects a complete, exact function-table contract. The
released API 1.0 prefix is 128 bytes. The current API 1.1 table is 320 bytes.
An API 1.1 client cannot use a host exposing only API 1.0, and the current C++
wrapper intentionally does not downgrade. Conversely, any client built with
the API 1.0 SDK is supported by later 1.x hosts through the unchanged API 1.0
prefix.

**Fix**

- Choose the SDK/API feature level intentionally and document the first host
  release that provides it. Players may use that host or a later compatible
  release from the same major line.
- API 1.1 C++ code must establish a valid `Connection` and register through
  `connection.value().register_menu(...)`.
- Raw-C code must zero the table, set its exact prefix size, request the API
  version compiled into its header, then check the returned version,
  capability bit, and every function pointer it uses.
- Use `menu.supports(erui::Capability::...)` where the C++ API exposes an
  optional feature. Do not infer availability from a similarly named Elden
  Ring widget or from a non-null pointer belonging to another API version.
- `ERUI_NOT_SUPPORTED` can also mean that a game-version-sensitive optional
  native facility was unavailable; inspect the host log before blaming table
  negotiation.

## Provider registration or commit fails

**Symptom**

`Connection::register_menu()` returns `ErrorCode::registration_failed`,
`builder_exception`, or `operation_failed`. Common native results include
`ERUI_INVALID_ARGUMENT`, `ERUI_DUPLICATE_PROVIDER_ID`,
`ERUI_DUPLICATE_ACTION_ID`, `ERUI_CALLBACK_REJECTED`,
`ERUI_REGISTRATION_CLOSED`, and `ERUI_HOST_FAILED`.

**Likely cause and fix**

| Result | Likely cause | Fix |
|---|---|---|
| `ERUI_INVALID_ARGUMENT` | Empty/malformed/oversized text or identifiers, invalid descriptor size/flags/reserved fields, invalid row bounds, or callback/user-data mismatch. | Start from a zero-initialized descriptor or the C++ option types. Keep visible text valid UTF-16. Provider and action IDs contain 1..255 ASCII letters, digits, `.`, `_`, or `-`. Log the first builder operation that fails. |
| `ERUI_DUPLICATE_PROVIDER_ID` | The same client was loaded twice, two mods copied one sample ID, or two drafts use the same ID. | Load the DLL once and give every mod a distinct, stable provider ID. Do not localize or casually rename it. |
| `ERUI_DUPLICATE_ACTION_ID` | API 1.1 registered the same stable action ID in more than one section of one provider. | Make action IDs unique provider-wide; section placement is not part of identity. |
| `ERUI_CALLBACK_REJECTED` | `owner_module` is wrong, a callback address is outside that module, or callback metadata violates the contract. | Capture the client's actual `HMODULE` in its `DllMain`, pass it as `owner_module`, and keep callback functions in that module. |
| `ERUI_REGISTRATION_CLOSED` | The client tried to register after the bounded startup collection phase. | Register from the client's startup worker, not lazily when a player opens a menu. Increasing `RegistrationMaxWaitMs` may help diagnose a slow client but is not a substitute for prompt startup registration. |
| `builder_exception` | The C++ builder threw. | Make the builder deterministic and catch failures in client-owned preparation. No exception may cross a callback or C ABI boundary. |
| `ERUI_HOST_FAILED` during commit | The logical provider was accepted, but final menu compilation, address resolution, or hook installation failed. | Enable logging and follow the game-update/native-address entry below. |

Keep the returned `Registration` when later getters, setters, storage-independent
alerts, or other registration-scoped operations are needed. A `Page` is only a
builder handle and must not be used after `register_menu()` returns.

## The client registers from `DllMain` and hangs, times out, or behaves randomly

**Symptom**

Game startup stalls, `connect()` times out inconsistently, registration closes,
or the process deadlocks while loading DLLs.

**Likely cause**

Windows holds the loader lock while calling `DllMain`. API 1.1 connection may
poll, and provider commit may wait for the remaining startup window plus native
installation. Calling either operation under the loader lock is unsupported.

**Fix**

Follow the [client template](../../examples/template/README.md): in
`DLL_PROCESS_ATTACH`, capture the module handle, disable thread notifications,
start a worker, and return. Connect and register on that worker. Never wait for
the worker from `DllMain`, and do not perform registration in detach handling.

## A callback crashes later, or unloading the client crashes the game

**Symptom**

Registration succeeds, but selecting a row later jumps into invalid code;
hot-reloading/unloading a client or the host crashes; callback text becomes
corrupt after returning.

**Likely cause**

The client released callback state/code too soon, retained a borrowed view, or
attempted unsupported hot unloading. The host pins successfully committed
provider modules because native rows retain callbacks.

**Fix**

- Treat committed clients and the host as process-lifetime modules. Do not call
  `FreeLibrary`, hot-reload them, or implement manual detach cleanup of
  registered callbacks.
- Keep row, TextInput, ColorPicker, input-action, and assignment callback code
  plus `user_data` valid through process exit.
- Alert callback state must remain valid until completion. Page-title formatter
  state needs to remain valid until registration/commit returns.
- Copy callback-scoped strings, views, contexts, and assignment batches before
  returning if the mod needs them later.
- Store client state at a stable address; do not pass a temporary or a moved
  object as long-lived `user_data`.

## A row works, but its appearance or capacity is different

**Symptom**

- TextInput edits correctly but lacks the red empty state or bracketed frame.
- ColorPicker opens correctly but appears as an ordinary action row rather
  than a bracketed live swatch.
- A Game/Camera action row lacks the matching left label presentation.
- Game Options paginates earlier than screenshots, or no custom Camera Options
  row appears.

**Likely cause**

The optional loose GFX movies are absent, installed at the wrong Mod Engine 2
root, or replaced by another mod. GFX presentation is separate from the native
API. Most controls keep a functional fallback. The tested native Camera Options
panel is exceptional: all authored slots are occupied, so it has no room for a
custom row or even a Next row until capacity is expanded.

**Fix**

- Install the release's optional files at their original relative paths under
  the active Mod Engine 2 mod directory:

  ```text
  menu/win/02_040_optionsetting.gfx
  menu/win/02_042_pc_graphicsetting.gfx
  ```

- Use `02_040` for the 13-row Game/Camera capacity and shared action-label
  presentation. Use both movies for matching root/subpage TextInput and
  ColorPicker presentation.
- Do not diagnose a missing frame or fallback action row as an API failure when
  activation and callbacks still work.
- If loose assets conflict, reproduce the narrow transformations from the
  user's own extracted movie with the
  [presentation and optional GFX guide](../guides/presentation-and-gfx.md).
  The host never patches `eldenring.exe` or game archives.

## `ERNativeUI.log` is missing or contains old information

**Symptom**

The wrapper asks you to inspect `ERNativeUI.log`, but no new file appears, or an
existing log's timestamp/content does not change.

**Likely cause**

Logging is disabled by default. With `EnableLog = 0`, the host does not create,
truncate, append, or flush the log; an old file is deliberately left untouched.
`EnableDiagnostics` produces useful detail only when logging itself is enabled.
You may also be editing an INI beside a host copy that is not the one loaded.

**Fix**

1. Edit `ERNativeUI.ini` beside the exact loaded `ERNativeUI.dll`:

   ```ini
   [Logging]
   EnableLog = 1

   [Diagnostics]
   EnableDiagnostics = 1
   ```

2. Fully restart the game; configuration is read at host startup.
3. Check the new timestamp and the startup/version line before using the log.
4. Disable diagnostics—and normally logging—after collecting a bounded report.

If the INI is missing, the host normally creates it from an embedded default
without overwriting an existing file. A failure to create it is sent to the
Windows debug output instead. The reference defaults are in
the [default `ERNativeUI.ini`](https://github.com/Flammrock/ERNativeUI/blob/main/assets/ERNativeUI.ini).

## A game update causes address or hook installation failure

**Symptom**

- A client gets `ERUI_HOST_FAILED`, often while committing.
- The host log reports unresolved/ambiguous addresses, failed validation,
  incomplete required native interfaces, or hook installation failure.
- One additional built-in page is absent and the log says that destination was
  disabled safely, while other pages still work.

**Likely cause**

Elden Ring changed game-version-sensitive machine code, or another mod already
patched a required hook site in an unsupported way. ERNativeUI intentionally
fails closed instead of calling a nearby or guessed address. Optional built-in
destinations are isolated where possible; a required facility used by the
compiled menu can fail the full host installation.

**Fix**

- Install an ERNativeUI release that explicitly supports the running Elden
  Ring executable. Do not disable validation, copy an old fallback RVA, or
  patch/distribute `eldenring.exe`.
- Reproduce once with ERNativeUI and a minimal client alone. Then restore other
  DLLs one at a time to distinguish a game update from a hook conflict.
- For Solid Uncapper, use the documented load order and compatible versions.
- Report the ERNativeUI/client/other-mod versions, DLL order, game PE timestamp
  and image size from the log, the first failed address/hook line, and the exact
  page/action that was requested. See
  [Game and mod compatibility](../compatibility.md).

## Two host copies or two client copies produce inconsistent behavior

**Symptom**

Different clients appear to negotiate different behavior, a new client says
the API is old despite an updated DLL being present, a provider ID is reported
twice, or configuration/log files appear beside an unexpected directory.

**Likely cause**

More than one mod bundled `ERNativeUI.dll`, an obsolete copy remains in another
configured path, or the same client DLL is listed twice. Clients search for the
already-loaded module by the exact basename, so duplicate hosts make ownership
and version diagnosis ambiguous even if Windows happens to return one of them.

**Fix**

- Keep one canonical host installation and make client mods declare it as a
  dependency instead of redistributing private copies.
- Search every active Mod Engine 2 DLL path, remove/disable duplicate hosts,
  and verify the remaining file version and adjacent INI/log.
- List each client once. If `ERUI_DUPLICATE_PROVIDER_ID` remains, replace copied
  sample IDs with one stable unique ID per mod.
- Never rename the host to work around a collision.

## A callback runs on an unexpected thread or the game becomes unstable

**Symptom**

A callback is observed on a different thread than the mod expects, shared
state races, UI freezes while a callback runs, or a programmatic change appears
without invoking the player's change callback.

**Likely cause**

ERNativeUI deliberately uses more than one callback context:

| Callback or operation | Execution context |
|---|---|
| Button action | Elden Ring UI thread, synchronously |
| Toggle, slider, inline-choice, and popup-choice changes | ERNativeUI host worker |
| API 1.1 TextInput and ColorPicker confirmed changes | ERNativeUI host worker |
| API 1.1 input-action and assignment-change events | ERNativeUI host worker, after bounded native-input handoff |
| Alert completion | ERNativeUI host worker, asynchronously after dismissal/failure |
| Explicit API 1.1 storage operations | The calling client thread |

Programmatic setters are intentionally silent; they do not simulate a player
callback. Holding an input binding does not repeat, and binding callbacks are
suppressed while native remapping, tracked text editing, or an ERNativeUI modal
owns input.

**Fix**

- Keep every callback short and `noexcept`; never block the UI thread or let an
  exception cross the C ABI.
- Synchronize state shared with the mod's own threads. Queue gameplay or render
  work to the thread required by that subsystem rather than doing it directly
  in an ERNativeUI callback.
- Treat input-binding callbacks as global, non-consuming observations. Apply
  the mod's own gameplay/menu context policy.
- Update client state explicitly after a successful programmatic setter; do not
  wait for a callback that the contract says will not occur.
- When testing a player callback, actually change and confirm the value. Cancel
  and same-value confirmation are intentionally silent for TextInput and
  ColorPicker.

The extensively commented [client template](../../examples/template/README.md)
shows the supported worker, callback, state, and persistence patterns.
