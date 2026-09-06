# Lifecycle, errors, and limits

This page defines the common contract every ERUI API 1.1 client needs,
regardless of which controls it uses. API 1.1 is the finished current runtime
API. ERNativeUI 1.1.0 is the first project release that packages it; those two
version numbers are independent.

The strict-C declarations in `<ernativeui/erui.h>` are normative. The C++17
types in `<ernativeui/ERNativeUI.hpp>` implement the same contract without
placing C++ objects across the DLL boundary.

## Lifecycle at a glance

```text
client DllMain
    -> capture HMODULE and start a worker
        -> connect to exact API 1.1
            -> register a private provider draft
                -> add pages, rows, bindings, and optional storage
                    -> commit and wait for final native installation
                        -> use retained runtime handles
                            -> process exit unloads everything
```

Connection and registration must run after `DllMain` returns. Both can wait,
allocate, and call another DLL, which is unsafe while Windows holds the loader
lock.

## Connect once, then reuse the connection

The normal C++17 entry point is:

```cpp
const auto connected = erui::connect();
if (!connected) {
    report_erui_error(connected.error());
    return;
}

const erui::LanguageInfo language = connected.value().game_language();
```

The default overload waits for at most 30 seconds. The duration overload can
choose another bounded timeout. It polls for the already-loaded canonical
`ERNativeUI.dll`; it does not call `LoadLibrary`. It requests API 1.1 exactly,
does not downgrade to API 1.0, and returns immediately when the host is ready.

A successful API 1.1 handshake guarantees that Steam language discovery has
settled. Language lookup can still return `ERUI_NOT_SUPPORTED` when Steam is
definitively unavailable; this does not disable the menu API. `LanguageInfo`
copies the exact Steam identifier into client-owned storage and separately
classifies identifiers known to ERNativeUI.

A strict-C client performs the same sequence explicitly:

1. Call `GetModuleHandleW(L"ERNativeUI.dll")`; never load a private copy.
2. Resolve `ERUI_GetApi` with `GetProcAddress`.
3. Zero a fresh `ERUI_Api`, set `size = ERUI_API_V1_1_SIZE`, and request
   `ERUI_API_VERSION_1_1`.
4. Retry only `ERUI_HOST_NOT_READY` from a worker. Reinitialize the table for
   each attempt instead of reusing a possibly partial result.
5. On success, verify the exact returned version, required table size,
   capability bits, and function pointers before registration.

`Connection` contains a client-side copy of the C function table. It has normal
copy semantics and does not own or unload the host.

## Registration and commit

```cpp
erui::ProviderOptions options{};
options.provider_id = "my-mod";
options.display_name = L"My Mod";
options.owner_module = module;

const auto registered = connected.value().register_menu(
    options,
    [](erui::Menu& menu) {
        auto root = menu.root();
        root.add_toggle(
            L"Enabled", L"Enable this mod.", 1, &enabled_changed);
    });

if (!registered) {
    report_erui_error(registered.error());
    return;
}

erui::Registration registration = registered.value();
```

`provider_id` is an opaque, permanent, case-sensitive ASCII machine identity.
It contains 1..255 letters, digits, `.`, `_`, or `-`
(`^[A-Za-z0-9_.-]+$`) and must be distinct among providers loaded in the
process. Keep it stable and do not localize it. `display_name` is player-facing.
`owner_module` must be the client's captured `HMODULE`; the host uses it to
validate callback addresses and pin a successfully committed provider.
`root_priority` defaults to `100`.

The builder operates on a private draft. Input text, identifiers, option lists,
initial values, and descriptors are copied synchronously by their builder
calls. The C++ wrapper records the first failed builder operation. If the
builder throws or any operation fails, it closes and aborts the draft instead
of publishing a partial provider.

Commit has two observable stages:

1. validate callbacks and atomically publish the complete provider declaration;
2. wait for the startup registry to close, all providers to merge, and the
   required native runtime interfaces to install.

A successful `register_menu` therefore means the native host became usable,
not merely that a draft entered a registry. A game update that invalidates a
required native address makes commit return `ERUI_HOST_FAILED` safely.

A strict-C client zero-initializes `ERUI_ProviderDesc`, sets its `size`, copies
the negotiated `api.api_version` into `api_version`, supplies the client
`HMODULE`, and calls `register_provider`. After any descriptor/add failure it
calls `abort_provider`; otherwise it calls `commit_provider` exactly once.

Registration is a startup-only structural operation. Once the bounded startup
window closes, later topology changes return `ERUI_REGISTRATION_CLOSED`.
Provider order is ascending `root_priority`, then provider ID, then
registration order; rows keep provider-local insertion order.

Keep the returned `Registration` if runtime code needs row getters/setters or
alerts. Destroying this small client-side value does not unregister the menu.
The committed provider remains installed and pinned until process exit.

## Capability checks

API version selects a table layout. Capabilities describe feature surfaces in
that table; they are not substitutes for checking an operation's
`ERUI_Result`.

| API | Capability groups |
|---|---|
| 1.0 prefix retained by API 1.1 | Toggle, slider, button, submenu, pagination, host-owned row values, page presentation, alerts, inline choice, popup choice, game language |
| Added in 1.1 | TextInput, ColorPicker, built-in pages, input bindings, provider storage |

In C++, query `connection.supports(erui::Capability::text_input)` or
`menu.supports(...)` before capability-scoped code. The corresponding raw-C
test checks both the `ERUI_CAP_*` bit and its function-table pointer.

The current C++ wrapper accepts API 1.1 only when the complete appended
TextInput, ColorPicker, built-in-page, input-binding, and storage blocks are
present. Checks near individual builders still document their dependency and
guard bad tables; they are not an automatic downgrade path to a partial API
1.1 table or a host exposing only API 1.0.

A capability says the host implements the public surface. A game-build-
sensitive operation can still return `ERUI_NOT_SUPPORTED`, or final native
installation can fail with `ERUI_HOST_FAILED`. Always handle results.

## Ownership and lifetimes

| Object or data | Owner and required lifetime |
|---|---|
| IDs, visible text, choice arrays, initial values, storage paths, alert messages | Borrowed for the call and copied by the host before it returns |
| `Menu`, `Page`, `InputBindings`, and `InputSection` C++ builders | Valid only inside the `register_menu` builder; escaped copies close when that callback ends |
| Row, input-action, and storage handles | Opaque provider-owned identities; retain their C++ wrappers only after registration succeeds |
| Row callbacks, input-action callbacks, assignment handler, and their external `user_data` | Client-owned and valid until process exit after commit |
| Page-title formatter and `user_data` | Valid through `commit_provider`; the host copies each successful formatted title |
| Alert callback and `user_data` | Valid until its asynchronous completion callback runs |
| Callback contexts, ranges, string views, and TextInput change values | Borrowed for one callback invocation; copy anything needed later |
| Raw `ERUI_GameLanguageInfo.identifier` | Immutable host-owned memory valid until process exit |
| C++ `LanguageInfo::identifier` | Client-owned copy |
| `Connection` and `Registration` | Client-side function-table/handle values; destruction does not unload or unregister anything |

All committed callback functions must reside in `owner_module`. Do not pass a
callback implemented in a helper DLL while declaring a different owner module.
Do not hot-unload a committed client or the host.

Strict-C views are counted and do not need a terminator. Machine identifiers
use UTF-8 `ERUI_StringView`; player-visible text uses UTF-16
`ERUI_Utf16View`. Embedded NUL is rejected. UTF-16 must contain valid surrogate
pairs.

## Callback threads

| Callback or operation | Execution context |
|---|---|
| `register_menu` builder | Calling client's initialization worker |
| Page-title formatter | ERNativeUI's startup worker during compilation, before successful commit returns |
| Button callback | Synchronously on Elden Ring's UI thread |
| Toggle, slider, inline-choice, and popup-choice change callback | ERNativeUI worker |
| TextInput and ColorPicker confirmed-change callback | ERNativeUI worker |
| Input-action and assignment-change callback | Serialized on the ERNativeUI worker after input-thread sampling |
| Alert completion callback | Asynchronously on the ERNativeUI worker; never inside `alert`/`enqueue_alert` |
| Storage calls | Synchronously on the calling client thread |

The registry mutex is not held while client callbacks run. Every callback must
be short and non-throwing; no exception may cross the C ABI. Synchronize state
shared with other client/game threads and queue work that requires a different
thread.

Input-action callbacks observe global released-to-pressed edges. They do not
consume Elden Ring input, and every action sharing an assignment may fire.
They are suppressed while native remapping owns input, while a tracked text
editor is active, and while an ERNativeUI modal owns input. A suppressed press
must be released before it can trigger a later edge.

## Error handling

### C++ result objects

`erui::connect()` returns `ConnectionResult`; `Connection::register_menu`
returns `RegistrationResult`. Test either result before calling `value()`.
Failures expose an `Error` with a wrapper-level `code()`, the raw
`native_result()`, and a human-readable `message()`. The message can be empty
on an allocation-failure fallback, so do not make program logic depend on it.

| `erui::ErrorCode` | Meaning |
|---|---|
| `none` | No wrapper error |
| `host_not_loaded` | The canonical host DLL was not observed before the connection deadline |
| `export_not_found` | A DLL with the canonical name lacks `ERUI_GetApi` |
| `host_not_ready` | The host was observed but startup did not settle before the deadline |
| `host_failed` | Host initialization or required native installation failed |
| `incompatible_api` | Exact version negotiation or table validation failed |
| `registration_failed` | Provider creation or final commit failed |
| `builder_exception` | The client builder threw; its draft was aborted |
| `operation_failed` | A builder operation failed, or a wrapper allocation/exception fell outside the more specific groups |

Runtime methods such as `set_value`, `set_text`, `set_color`, binding methods,
storage calls, and `alert` return `ERUI_Result` directly. Check it at the point
where recovery or logging is still meaningful.

### Raw `ERUI_Result` values

| Result | Meaning |
|---|---|
| `ERUI_OK` | The operation completed according to its synchronous contract |
| `ERUI_INVALID_ARGUMENT` | A pointer, descriptor, enum, reserved field, value, row type, or range is invalid |
| `ERUI_HOST_NOT_READY` | Host startup or a required runtime subsystem is not ready |
| `ERUI_HOST_FAILED` | The host entered a terminal failed state for this process |
| `ERUI_UNSUPPORTED_VERSION` | The requested exact API contract is unavailable |
| `ERUI_DUPLICATE_PROVIDER_ID` | Another provider already owns that process-wide ID |
| `ERUI_INVALID_HANDLE` | A zero, stale, wrong-provider, or otherwise unknown handle was used |
| `ERUI_ALREADY_COMMITTED` | A draft-only operation targeted a committed provider, or an abort targeted it |
| `ERUI_REGISTRATION_CLOSED` | The startup topology window has closed |
| `ERUI_OUT_OF_MEMORY` | A bounded allocation failed or a declared object limit was reached |
| `ERUI_CALLBACK_REJECTED` | Callback ownership validation or module pinning failed |
| `ERUI_INTERNAL_ERROR` | The host detected an unexpected internal state or invalid returned data |
| `ERUI_QUEUE_FULL` | The bounded global alert queue cannot accept another request |
| `ERUI_NOT_SUPPORTED` | The selected API/provider/game path cannot provide this optional operation |
| `ERUI_BUFFER_TOO_SMALL` | A copy-out buffer or fixed structure is smaller than required |
| `ERUI_DUPLICATE_ACTION_ID` | An input action repeats an ID already used by its provider |
| `ERUI_NOT_FOUND` | A requested storage key is absent |
| `ERUI_STORAGE_NOT_LOADED` | A storage read, mutation, or save was attempted before `load()` |
| `ERUI_STORAGE_ALREADY_LOADED` | `load()` was called more than once for the same document |
| `ERUI_STORAGE_IO_ERROR` | Reading, writing, replacing, or resolving storage failed |
| `ERUI_STORAGE_FORMAT_ERROR` | Stored or serialized input is malformed |

`ERUI_OK` has operation-specific timing. In particular, `alert()` returning
`ERUI_OK` means the request entered the FIFO, not that the player dismissed
it. Its callback reports eventual completion. Likewise, programmatic row
writes update canonical state synchronously but marshal native presentation to
a safe later UI boundary.

An alert callback reports `ERUI_OK` plus its `primary`, `secondary`, or
`dismissed` response after normal completion. If an accepted alert later fails
to present or complete safely, its callback receives a non-OK result and the
`none` response.

Retry `ERUI_HOST_NOT_READY` only as part of the documented connection/startup
path. Treat `ERUI_HOST_FAILED` and `ERUI_REGISTRATION_CLOSED` as terminal for
the current process. `ERUI_QUEUE_FULL` is a transient alert-backpressure result;
do not discard client state merely because a dialog was not queued.

## Value-update semantics

ERNativeUI owns the canonical UI value after a row or action is declared.
Initial values seed it, player interaction updates it, and retained handles can
read or change it explicitly.

| Surface | Player notification | Programmatic behavior |
|---|---|---|
| Toggle | Changed byte on the host worker | `set_value` is silent; any nonzero value normalizes to `1` |
| Slider | Changed byte on the host worker | `set_value` is silent, clamps to the declared `0..255` range, then rounds down to a step from the minimum |
| Inline/popup choice | Changed zero-based index on the host worker | `set_value` is silent; an out-of-range index is rejected; popup cancel and unchanged confirmation are silent |
| TextInput | One callback after a changed confirmation | `set_text` copies silently; over-limit text is rejected, not truncated |
| ColorPicker | One callback after a changed confirmation | `set_color` copies RGB silently; Cancel and unchanged confirmation are silent |
| Input action | Activation callback on a global rising edge | Bind, unbind, and reset update native rows/runtime silently; player assignment and Clear enter the optional assignment handler |
| Storage | No implicit callback or write | `set`, `erase`, and `apply` mutate the loaded in-memory document; `save` is explicit |

`get_value`, `get_text`, and `get_color` copy the current canonical value.
TextInput and ColorPicker have a defined active-editor race rule: a
programmatic write made while the editor is open survives Cancel; a later
player confirmation replaces it. Avoid continuously racing other row setters
against player input; the last observed write wins.

### Input assignment states

Every `ActionInputs` device slot has three distinct states:

- **absent**: unsupported in a declaration or complete runtime snapshot;
- **unbound**: supported but intentionally has no assignment; or
- **bound**: supported and assigned one semantic ERNativeUI input value.

For `InputAction::bind` and raw `set_action_inputs`, the value is a patch:
absent means leave that device unchanged, while present slots are applied
atomically. Queries and callback snapshots are complete, so absent means the
action does not support that device. `InputAction::unbind()` clears every
supported slot; device views can bind, unbind, query, or reset one slot.

Programmatic bind/unbind/reset calls do not emit assignment-change events. The
current native screens report player assignment and Clear through the
provider-wide event. API 1.1 reserves a Reset-to-Defaults reason for a future
native path that can report one. Event ranges, changes, and `action_id()` views
expire when the callback returns.

Stored `ActionInputs` are sparse overrides: an absent slot inherits the mod's
declared default and an unbound slot preserves an explicit Clear. If a reserved
Reset-to-Defaults event is supplied, it removes that device's override.
`StorageSection::apply(event)` validates the whole batch before changing the
in-memory document. It does not save to disk; call `Storage::save()` explicitly
or install the opt-in persistence helper.

### Storage lifecycle

A provider can open one storage document while its registration draft is open.
The returned `Storage` value can be retained after successful commit. Opening
creates only an in-memory handle:

- `load()` is explicit; once the document is loaded, another call returns
  `ERUI_STORAGE_ALREADY_LOADED`. A missing file loads as an empty clean
  document without creating a file;
- reads, writes, erases, and assignment application require a loaded document;
- `set` and `erase` modify only memory and mark a real change dirty;
- `save()` is explicit; saving a clean document returns `ERUI_OK` without a
  disk write; and
- nothing is saved automatically at process exit.

## Important API 1.1 limits

These limits are enforced by the public contract or by the bounded host
service behind it.

| Resource | Limit |
|---|---:|
| Providers in one process | 256 |
| Logical pages per provider | 256 |
| Rows per provider | 4,096 |
| Input sections | 4,096 per provider and 4,096 process-wide |
| Input actions | 4,096 per provider and 4,096 process-wide |
| Provider ID or action ID | 1..255 ASCII bytes; letters, digits, `.`, `_`, `-` only |
| Ordinary visible text field | At most 4,096 valid UTF-16 code units |
| Choice options | 1..32 non-empty labels; indices are zero-based |
| Slider storage | Byte values in `0..255`; positive step |
| TextInput content | Per-row inclusive maximum 1..35 UTF-16 code units; default 16 |
| Page-title formatter output | `ERUI_PAGE_TITLE_BUFFER_CAPACITY`, 4,096 UTF-16 code units |
| Accepted outstanding alerts | 64 process-wide; FIFO presentation one at a time |
| Alert message | 1..4,096 UTF-16 code units |
| Outstanding ERNativeUI ColorPicker session | One process-wide |
| Canonical serialized `ActionInputs` | `ERUI_ACTION_INPUTS_TEXT_MAX_BYTES`, 67 bytes excluding a terminator |
| Storage section or key | 1..255 bytes |
| Storage value | 65,536 UTF-8 bytes |
| Encoded storage file | 1,048,576 bytes |
| Decoded storage data | 262,144 total section-name, key, and value bytes |
| Storage document | 1,024 sections and 4,096 entries |
| Assignment changes applied in one storage batch | 4,096 |
| API 1.0 function-table prefix on Windows x64 | `ERUI_API_V1_0_SIZE`, 128 bytes |
| API 1.1 function-table prefix on Windows x64 | `ERUI_API_V1_1_SIZE`, 320 bytes |

The 4,096-section and 4,096-action values are defensive registration and
service ceilings, not a demonstrated practical size for Elden Ring's native
binding screens. Keep player-facing catalogs small enough to navigate
comfortably. Native vector growth at very large catalogs remains an open
research question; see [Input bindings](../research/case-studies/input-bindings.md#unresolved-questions).

Storage section/key identifiers accept ASCII letters, digits, `.`, `_`, `-`,
and internal spaces. Leading/trailing spaces and the complete names `.` and
`..` are rejected. Storage values are UTF-8 byte strings without embedded NUL,
CR, or LF; empty values and `=`, `#`, and `;` are preserved.

Native panel capacity is deliberately not a fixed public limit. Declare the
logical rows the mod needs and let ERNativeUI paginate against the live page
capacity and optional GFX presentation.

## Strict-C structure rules

- Zero-initialize every descriptor and output structure.
- Set each descriptor's `size` to its complete structure size and leave all
  flags/reserved fields zero.
- Initialize output structure sizes before calls that require them.
- Set `ERUI_ProviderDesc.api_version` to the exact successfully negotiated
  `ERUI_Api.api_version`.
- `ERUI_ActionInputs.size` must equal `sizeof(ERUI_ActionInputs)`; it is a
  fixed-layout value, not an extensible descriptor prefix.
- Treat every handle as an opaque 64-bit identity. Zero is always invalid, and
  a handle cannot be used with another provider or object kind.
- Use the documented two-call size query for variable-length output. Output is
  counted and not NUL-terminated.
- Call `abort_provider` after any raw-C draft failure. Do not continue adding
  objects after the first error and then attempt to commit.

### Pinned Windows x64 layouts

The ABI uses natural Windows x64 alignment and no packing pragma. Its released
prefixes are tested under MSVC and MinGW-w64. These sizes are useful when
reviewing a foreign-function binding or diagnosing an unexpected compiler
layout; application code should still write `sizeof(Type)` rather than a
numeric literal.

| Type | Bytes |
|---|---:|
| `ERUI_StringView`, `ERUI_Utf16View` | 16 |
| `ERUI_GameLanguageInfo` | 32 |
| `ERUI_ProviderDesc` | 56 |
| `ERUI_ButtonDesc`, `ERUI_ToggleDesc` | 64 |
| `ERUI_ChoiceDesc` | 72 |
| `ERUI_SliderDesc`, `ERUI_SubmenuDesc` | 80 |
| `ERUI_PageTitleFormatContext` | 56 |
| `ERUI_PagePresentationDesc` | 64 |
| `ERUI_AlertDesc` | 48 |
| `ERUI_TextInputChangeContext` | 48 |
| `ERUI_TextInputDesc` | 96 |
| `ERUI_Color` | 4 |
| `ERUI_ColorPickerChangeContext` | 32 |
| `ERUI_ColorPickerDesc` | 64 |
| `ERUI_ControllerInput`, `ERUI_KeyboardInput`, `ERUI_MouseInput` | 8 |
| `ERUI_ActionInputs` | 40 |
| `ERUI_InputActionActivatedContext` | 32 |
| `ERUI_AssignmentChange` | 128 |
| `ERUI_AssignmentsChangedContext` | 32 |
| `ERUI_AssignmentsChangedHandlerDesc` | 32 |
| `ERUI_InputSectionDesc` | 32 |
| `ERUI_InputActionDesc` | 104 |
| `ERUI_StorageDesc`, `ERUI_StorageInfo` | 40 |
| `ERUI_StorageKey` | 48 |
| API 1.0 function-table prefix | 128 |
| API 1.1 function-table prefix | 320 |

The complete offset and guard-page assertions live in
[`tests/abi/current/c_layout_test.c`](https://github.com/Flammrock/ERNativeUI/blob/main/tests/abi/current/c_layout_test.c).
Frozen API 1.0 headers and clients are kept separately under
[`tests/abi/releases/v1_0`](https://github.com/Flammrock/ERNativeUI/tree/main/tests/abi/releases/v1_0).

## API 1.0 binary compatibility

Compatibility is directional:

| Client binary | Loaded host | Result |
|---|---|---|
| Built with the released API 1.0 headers, before or after API 1.1 was published | Host exposing API 1.1 | Supported through the unchanged 128-byte API 1.0 prefix and original lifecycle |
| Compiled with current API 1.1 headers | Host exposing API 1.1 | Supported through explicit connection |
| Compiled with current API 1.1 headers | Host exposing only API 1.0 | Rejected; there is no silent downgrade |
| Requests an unknown API version | Current host | `ERUI_UNSUPPORTED_VERSION` |

An API 1.0 DLL embeds its selected header-only wrapper and does not need to be
recompiled merely because a player installs a host release that also supports
API 1.1. Source intentionally updated to the API 1.1 header must adopt the 1.1
`connect()` and `Connection::register_menu` flow.

Every 1.x host retains each API 1.x contract introduced by it or an earlier
1.x host release. A future host major may drop APIs from the previous major
unless its release documentation explicitly retains them. See
[Versioning](../versioning.md#same-major-host-compatibility-policy).

The API 1.0 prefix remains byte-for-byte frozen at 128 bytes. API 1.1 appends
new entries through its 320-byte prefix; it does not reorder API 1.0 fields.
The host writes only the selected supported prefix within the caller-declared
table size. Fixed-width integers, size-prefixed POD structures, `ERUI_CALL`
(`__cdecl`), and opaque handles keep MSVC, clang-cl, MinGW-w64, C, and C++
clients on the same public boundary.

This is runtime API compatibility, not release-package selection. CMake's
`find_package(ERNativeUI 1.1.0)` checks an ERNativeUI project release at build
time; `ERUI_GetApi(ERUI_API_VERSION_1_1, ...)` negotiates the ERUI contract in
the game process. See [Versioning](../versioning.md) and the permanent
[API 1.0 to 1.1 migration](../migrations/1.0-to-1.1.md).

## Client checklist

- Connect and register from a worker after `DllMain`.
- Treat ERNativeUI as optional so UI failure does not disable unrelated mod
  behavior.
- Use one stable provider ID and the real client `HMODULE`.
- Check `ConnectionResult`, `RegistrationResult`, and every runtime
  `ERUI_Result`.
- Copy callback-scoped views before returning.
- Keep committed callback code/state alive and do not hot-unload.
- Persist ordinary values and input assignments explicitly.
- Declare logical rows without assuming a native page capacity.
