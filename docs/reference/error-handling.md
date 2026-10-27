# Error handling

ERNativeUI uses explicit result values across the DLL boundary. It does not
throw C++ exceptions from runtime API operations, and client code must not let
an exception escape through an ERNativeUI callback.

This page is the central reference for connection failures, registration
failures, and runtime `ERUI_Result` values. Feature guides show where to check
a result, then link here instead of redefining the error model.

## Three result shapes

The C++ wrapper has two rich startup result objects and one fixed-width runtime
status type:

| Operation | Return type | Success check |
|---|---|---|
| `erui::connect()` | `erui::ConnectionResult` | `if (connected)` |
| `Connection::register_menu(...)` | `erui::RegistrationResult` | `if (registered)` |
| Runtime operations such as row edits, value writes, alerts, bindings, and storage | `ERUI_Result` | `result == ERUI_OK` |

Do not call `.value()` on a failed `ConnectionResult` or
`RegistrationResult`. Their `error()` object contains:

- `code()`: the C++ wrapper-level `erui::ErrorCode`;
- `native_result()`: the underlying `ERUI_Result`; and
- `message()`: a human-readable diagnostic string.

The message can be empty during an allocation-failure fallback. Use the
numeric/code fields for program logic and the message only for diagnostics.

```cpp
const erui::ConnectionResult connected = erui::connect();
if (!connected) {
    const erui::Error& error = connected.error();
    log_connection_failure(
        error.code(),
        error.native_result(),
        error.message());
    return;
}
```

## C++ startup errors

| `erui::ErrorCode` | Meaning |
|---|---|
| `none` | No wrapper error |
| `host_not_loaded` | The canonical host DLL was not observed before the connection deadline |
| `export_not_found` | A DLL with the canonical name lacks `ERUI_GetApi` |
| `host_not_ready` | The host was observed but startup did not settle before the deadline |
| `host_failed` | Host initialization or required native installation failed |
| `incompatible_api` | Exact version negotiation or function-table validation failed |
| `registration_failed` | Provider creation or final commit failed |
| `builder_exception` | The client builder threw; its private draft was aborted |
| `operation_failed` | A builder operation failed, or a wrapper allocation/exception fell outside a more specific group |

Registration remains atomic: if the builder throws or one builder operation
fails, ERNativeUI aborts the private provider draft instead of publishing a
partial menu.

## Runtime `ERUI_Result`

`ERUI_Result` is a `uint32_t` status code shared by the strict-C ABI and the
C++ wrapper. It is not a Boolean and does not contain the operation's output.

In this example, `intensity` is a retained `erui::SliderRow` returned by
`Page::add_slider(...)` during registration.

```cpp
const ERUI_Result result = intensity.edit()
    .help(L"The active profile changed.")
    .commit();

if (result != ERUI_OK) {
    log_erui_failure("update intensity help", result);
    // Keep using the last successfully published UI state.
}
```

Check the result where recovery, fallback, or useful logging is still
possible. Do not ignore a failure merely because a capability check succeeded:
capabilities describe available surfaces, while each operation still validates
its concrete handles, descriptors, values, and current runtime state.

| Result | Meaning |
|---|---|
| `ERUI_OK` | The operation completed according to its synchronous contract |
| `ERUI_INVALID_ARGUMENT` | A pointer, descriptor, enum, reserved field, value, row type, field combination, or range is invalid |
| `ERUI_HOST_NOT_READY` | Host startup or a required runtime subsystem is not ready |
| `ERUI_HOST_FAILED` | The host entered a terminal failed state for this process |
| `ERUI_UNSUPPORTED_VERSION` | The requested exact API contract is unavailable |
| `ERUI_DUPLICATE_PROVIDER_ID` | Another provider already owns that process-wide ID |
| `ERUI_INVALID_HANDLE` | A zero, stale, wrong-provider, or otherwise unknown handle was used |
| `ERUI_ALREADY_COMMITTED` | A one-shot provider or edit draft received another commit/abort attempt |
| `ERUI_REGISTRATION_CLOSED` | The startup topology window has closed |
| `ERUI_OUT_OF_MEMORY` | A bounded allocation failed or a declared object limit was reached |
| `ERUI_CALLBACK_REJECTED` | Callback ownership validation or module pinning failed |
| `ERUI_INTERNAL_ERROR` | The host detected an unexpected internal state or invalid returned data |
| `ERUI_QUEUE_FULL` | The bounded global alert queue cannot accept another request |
| `ERUI_NOT_SUPPORTED` | The selected API, provider, or validated game path cannot provide this optional operation |
| `ERUI_BUFFER_TOO_SMALL` | A copy-out buffer or fixed structure is smaller than required |
| `ERUI_DUPLICATE_ACTION_ID` | An input action repeats an ID already used by its provider |
| `ERUI_NOT_FOUND` | A requested storage key is absent |
| `ERUI_STORAGE_NOT_LOADED` | A storage read, mutation, or save was attempted before `load()` |
| `ERUI_STORAGE_ALREADY_LOADED` | `load()` was called more than once for the same document |
| `ERUI_STORAGE_IO_ERROR` | Reading, writing, replacing, or resolving storage failed |
| `ERUI_STORAGE_FORMAT_ERROR` | Stored or serialized input is malformed |
| `ERUI_DUPLICATE_TAB_ID` | A custom tab repeats an ID already used by its provider |
| `ERUI_TAB_LIMIT_REACHED` | Committing or staging custom tabs would exceed the process-wide tab limit |
| `ERUI_TITLE_SCREEN_TEXT_LIMIT_REACHED` | Committing or staging title-screen text would exceed the process-wide line limit |

## What success means

`ERUI_OK` follows the synchronous contract of the operation; it does not
always mean that the player has already seen the effect.

- A row edit or programmatic value write updates/copies logical state before
  returning, then native presentation catches up at a safe UI boundary.
- An atomic update batch is completely validated, copied, and published as one
  logical generation before returning; its native reconstruction can occur
  later.
- `alert()` returning `ERUI_OK` means the request entered the FIFO, not that
  the player dismissed it. Its completion callback reports the eventual
  result and response.
- A storage mutation updates the loaded in-memory document. Disk persistence
  still requires an explicit `save()`.

This distinction makes runtime edits safe from ERNativeUI callbacks: the
callback finishes against the current native generation instead of triggering
recursive destruction or reconstruction underneath itself.

## Recovery policy

Use operation-specific recovery rather than retrying every non-OK result:

| Category | Typical results | Recommended response |
|---|---|---|
| Optional feature unavailable | `ERUI_NOT_SUPPORTED`, `ERUI_UNSUPPORTED_VERSION` | Disable that integration or use a documented fallback |
| Invalid client request | `ERUI_INVALID_ARGUMENT`, `ERUI_INVALID_HANDLE`, duplicate-ID results, `ERUI_ALREADY_COMMITTED` | Fix the client logic; do not loop/retry unchanged input |
| Terminal for this process | `ERUI_HOST_FAILED`, `ERUI_REGISTRATION_CLOSED` | Disable the affected ERNativeUI integration until the next game launch |
| Bounded/transient resource | `ERUI_QUEUE_FULL`, sometimes `ERUI_OUT_OF_MEMORY` | Keep client state, report the failure, and retry only when the operation documents a useful retry |
| Storage/data problem | storage-specific results, `ERUI_NOT_FOUND` | Apply the storage guide's fallback or default-value policy |

Retry `ERUI_HOST_NOT_READY` only through the documented bounded connection
path. Reinitialize a raw-C function table before each negotiation attempt; do
not keep using a possibly partial table.

## Row-edit failures

A C++ `row.edit()` draft is inert until `commit()`. The first commit attempt
consumes it even when validation fails. Create a new draft to retry.

For a strict-C multi-row update batch, any invalid descriptor, duplicate row,
wrong-provider row, allocation failure, or invalid final page projection
rejects the entire batch. No partial update is published.

For the row-edit and batching model, see
[Row updates](../guides/row-updates/README.md). For broader lifetime,
threading, ownership, and limit rules, see
[Lifecycle and limits](lifecycle-errors-and-limits.md). For symptoms during
installation or startup, see [Common errors](../getting-started/common-errors.md).
