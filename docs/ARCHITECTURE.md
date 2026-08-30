# Host architecture and lifecycle

## Ownership boundary

Exactly one `ERNativeUI.dll` owns Elden Ring addresses, hooks, native text IDs,
controller capacity, routing, and pagination. Client DLLs own their gameplay
logic but declare menus through the public C ABI.

The DLL embeds the default `ERNativeUI.ini` as a Windows `RCDATA` resource. At
startup the host atomically materializes it beside the DLL only when no INI is
present. Logging is opt-in and is checked before the log file is opened.

```text
client workers -> C ABI drafts -> deterministic host registry
                                      |
                                      v
                            one immutable menu model
                                      |
                                      v
                       one set of Elden Ring hooks
```

The host synchronously copies provider IDs, UTF-16 text, and both inline/popup
choice option arrays. Toggle/slider/choice
native bytes live in stable host allocations. Callback pointers and opaque
`user_data` remain client-owned. Commit verifies that callbacks belong to the
declared module and pins that module before native publication.

## Startup state machine

```text
initializing
    -> logger/config/native worker initialized
accepting registrations
    -> no draft active and activity quiet, or absolute deadline reached
registry frozen and merged
    -> logical model compiled and native interfaces installed
runtime_ready | failed
```

Providers may register concurrently. Each successful create/add/abort/commit
updates the activity time. A quiet freeze requires at least one commit and no
active draft; `RegistrationMaxWaitMs` remains the hard bound for a broken or
stalled client. The decision and freeze occur under the same registry lock.

Public `commit_provider` publishes the provider draft, then waits until the
host reaches `runtime_ready` or `failed`. This makes client success meaningful:
a game update that breaks address resolution returns `ERUI_HOST_FAILED`.

## Transactionality

`register_provider` creates a private draft. Handles are globally unique and a
page/row operation must also name its owning provider. Individual host
insertions roll back on allocation failure. `abort_provider` removes an
unpublished draft, including after the startup deadline; committed providers
cannot be aborted.

Commit validates and publishes one provider declaration atomically to the
registry. The final multi-provider allocation/compilation is a later host
phase and can still fail globally, primarily through allocation failure or an
unsupported game binary. Waiting commit callers then receive
`ERUI_HOST_FAILED`, and retained value APIs reject operations in that state.

Providers are merged by priority, provider ID, and registration ordinal. Rows
keep local insertion order. Disabled action/submenu rows are filtered before
reachability and pagination; unreachable child pages cannot force interfaces
that the active menu never calls.

## Language service

The host dynamically resolves the flat Steamworks language exports from the
game's already-loaded `steam_api64.dll`. The first successful current-language
result is copied into process-lifetime host storage and classified without
discarding its original token. Failed discovery is not cached, allowing a
later retry if Steam was temporarily unavailable. No Steam or game entry point
is patched.

## Threads

- Host discovery and registration: each client's initialization worker.
- Native row materialization and buttons: Elden Ring's UI thread.
- Toggle/slider/inline-choice/popup-choice polling and callbacks: ERNativeUI's
  worker.
- Native-alert completion callbacks: ERNativeUI's worker.
- `set_row_value`: any client thread; native-facing application is queued to
  the host worker.

The registry mutex is never held while invoking Elden Ring or client code. No
callback may unwind an exception through the ABI. MSVC builds also put an SEH
boundary around provider callbacks, but this is not a substitute for correct
client exception and thread handling.

The native value is an aligned byte because that is the storage Elden Ring's
constructors retain. On x64, native/player and programmatic writes are treated
as atomic byte stores with last-observed-write semantics. Clients should not
continuously race programmatic writes against UI input.

## Native core

After publication, provider records are adapted into the internal
`erui::Menu`. `MenuCompiler` allocates custom text IDs, computes reachable
pages, and prepares vanilla/patched root plans plus subpage plans. At install,
only pagination required by the detected controller capacity is mandatory.

Page presentation is compiled with the same immutable plan. A provider may
attach presentation metadata to one of its submenus, but never to the shared
root. For each physical slice, the compiler either uses the default title
(`Title (n/t)` for a paginated page) or calls the provider's bounded
formatter once. It validates and copies the UTF-16 result into host storage;
the runtime never calls a provider merely because the player opens a page.

Outer chrome follows ownership rather than the provider that happened to
register first. Elden Ring's existing Controller Settings root retains
`Configuration`, shared-root continuation slices use `ERNativeUI`, and a
provider-owned submenu uses its provider display name unless its page
presentation specifies an override.

The hub and subpage hooks materialize one requested slice. Next calls the
validated native open-subpage function. Previous validates the current route
and calls the discovered native Back wrapper, preserving Elden Ring's real
parent stack.

Elden Ring's native action-row constructor consumes an MSVC
`std::function<void()>` object. Consequently the hook-owning host must use an
MSVC-compatible compiler ABI (MSVC or clang-cl). This private restriction does
not cross the C ABI; MinGW-w64 client DLLs remain supported.

The source-level `src/menu.hpp` model is private implementation plumbing and
is not installed as the inter-mod SDK. Public mods use `include/ernativeui`.

The resolver bridge and the two Scaleform title layers are documented in
[Page titles and presentation](PAGE_PRESENTATION.md). Their internal native
addresses remain game-version-sensitive and are not part of the public ABI.

## Process lifetime

The host and committed providers are pinned. Hot unloading is unsupported.
Topology remains immutable after startup because native controls retain text,
callback, and page-route storage and no reliable native page-destruction
interface has been established. This bounded model keeps those lifetimes
provable without replacing objects that Elden Ring may still reference.
