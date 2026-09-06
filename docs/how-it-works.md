# How ERNativeUI works

ERNativeUI separates a mod's settings model from Elden Ring's private UI
implementation. A client mod declares what it wants to show; the shared host
turns that declaration into native pages, rows, dialogs, and input bindings.
Client mods never receive game pointers and do not install competing menu
hooks.

```text
ModClient1.dll ----\
ModClient2.dll -----+-- stable C ABI --> ERNativeUI.dll --> Elden Ring UI
ModClient3.dll ----/                     | merge providers
                                         | plan pages and pagination
                                         ` invoke client callbacks
```

There should be exactly one `ERNativeUI.dll` in the process. Every client uses
the header-only SDK to find that already-loaded host; a client must not link an
import library, call `LoadLibrary` for the host, or bundle a private host copy.

## Connect, then register

An API 1.1 C++ client starts from a worker thread after `DllMain` has returned:

```cpp
const auto connection = erui::connect();
if (!connection) {
    // Keep the gameplay mod usable without its optional settings UI.
    return;
}

erui::ProviderOptions options{};
options.provider_id = "my-mod";
options.display_name = L"My Mod";
options.owner_module = module;

const auto registration = connection.value().register_menu(
    options,
    [](erui::Menu& menu) {
        menu.root().add_toggle(
            L"Enabled",
            L"Enable this mod.",
            1,
            &enabled_changed);
    });
```

`connect()` requests the exact API 1.1 contract and waits for the host's
bounded startup-readiness process. It does not silently downgrade to API 1.0.
A successful `Connection` owns a copy of the negotiated function table, so it
is safe for normal value semantics inside the client.

Clients built with the API 1.0 SDK—whether they were compiled before API 1.1
existed or deliberately target 1.0 today—use the frozen 1.0 function-table
prefix and original registration lifecycle. Clients built with the API 1.1
SDK use the explicit `connect()` flow. Neither wrapper silently changes the
API selected by its headers.

## Registration is declarative

The builder callback creates a private provider draft. Calls such as
`add_toggle`, `add_slider`, and `add_submenu` describe a logical menu; they do
not immediately manipulate a visible Elden Ring screen. The host copies the
provider ID, text, option lists, initial values, and other descriptors while
building the draft.

When the builder succeeds, `register_menu` validates and commits the complete
provider atomically. After all startup providers have settled, the host:

1. orders providers deterministically;
2. merges their logical pages;
3. calculates reachability and pagination;
4. resolves the native facilities required by the compiled features; and
5. installs one coordinated set of game hooks.

This is why registration belongs at startup rather than in response to a page
opening. It also means an invalid or incomplete provider does not need to leave
half a menu behind.

Keep the returned `Registration` if the mod needs runtime getters, setters, or
other registration-scoped operations. The host pins a successfully committed
provider module because native rows can retain its callbacks. Hot-unloading a
registered client or the host is not supported.

## Pages are logical, not native pointers

`menu.root()` targets the shared **Game Options** page. API 1.1 can also obtain
supported built-in destinations with `menu.page(...)`, including Camera,
Display, Sound, Network, Keyboard/Mouse, and Graphics. Multiple mods may add
content to the same destination; the host merges it in a stable order.

`Page::add_submenu` creates a provider-owned logical child page. The host opens
that child through Elden Ring's native page stack, so focus, transition, and
Back behavior remain native.

Mods declare as many logical rows as they need. At runtime ERNativeUI reads the
available capacity of the concrete page and divides the logical content into
physical slices. It supplies Next and Previous rows where necessary and uses
the game's real push/pop behavior. A mod should therefore never create its own
pagination rows or assume that a page has a fixed number of visible slots.

See [Supported features](features.md) for the exact built-in destinations and
row types available in each API version.

## State and callbacks

The ownership boundary is intentionally simple:

- The host owns copied menu metadata and the native-facing values required by
  its rows.
- The client owns its gameplay state, callback code, and opaque `user_data`.
- Initial values seed the UI; callbacks tell the client about player changes.
- Registration getters and setters synchronize supported values explicitly.
- Nothing is persisted automatically. A mod may use ERNativeUI's opt-in
  provider storage or its own configuration system.

Callbacks do not all run in the same context. Native action-button callbacks
originate on Elden Ring's UI thread, while polled value changes, alert
completion, and input-binding events are delivered by the host worker. Keep
callbacks short, never let an exception cross the C ABI, and hand work to the
thread required by the mod's gameplay code. A callback value or string view
described as borrowed must be copied before the callback returns if it is
needed later.

## Optional GFX patches

ERNativeUI's behavior comes from native game interfaces; the DLL does not need
to parse a loose GFX file at startup. The supplied GFX assets and patcher are
optional presentation extensions. They can provide more visible row slots,
repair the left label of action rows, and add the character-creation-style
TextInput and ColorPicker presentation.

Without those loose assets, the host uses the capacity and widgets available
in the player's game. Pagination adapts automatically, and supported controls
retain their documented fallback presentation. A completely full built-in
panel may have no room for even a Next row until its optional capacity patch is
installed. The [feature overview](features.md) identifies such presentation
and capacity requirements.

The patcher transforms a user's own extracted GFX into a separate output; it
does not patch `eldenring.exe` or the game's archives.

## Capabilities and failure behavior

API version and feature availability are separate checks. Version negotiation
establishes the table layout. Capability queries tell a client whether the
connected host exposes an optional feature such as a newer row type. Use the
wrapper's capability-aware builders rather than assuming that a function is
usable because a similarly named feature exists in the game.

The host also validates the running Elden Ring build and the native boundaries
needed by the compiled menu. It fails closed instead of calling an unverified
address. Depending on the boundary, failure either disables only that optional
destination/feature or causes registration to report that the host could not
become ready. A well-behaved client treats its ERNativeUI integration as
optional and reports the error without disabling unrelated gameplay logic.

## Where to go next

- [Getting started for mod authors](getting-started/README.md) walks through
  setup, a first client DLL, deployment, and common integration errors.
- [Supported features](features.md) answers what is available, in which API
  version, and whether an optional GFX patch changes it.
- [Versioning](versioning.md) separates SDK/release selection from runtime API
  negotiation and compatibility.
- [Mod-author guides](guides/README.md) explain menus, controls, dialogs,
  localization, input bindings, storage, and optional presentation by task.
- [API reference](reference/README.md) records the exact lifecycle, ownership,
  threading, errors, limits, and compatibility contract.
- [Mod template](../examples/template/README.md) is the best starting point
  for a new client project.
- [Localized Greeting](../examples/localized_greeting/README.md) is a small
  connection, localization, and dialog example.
- [Tarnished UI Showcase](../examples/tarnished_ui_showcase/README.md)
  demonstrates the broader API in one buildable client.
- The [strict-C header](https://github.com/Flammrock/ERNativeUI/blob/main/include/ernativeui/erui.h) is the normative binary
  contract; the [C++17 wrapper](https://github.com/Flammrock/ERNativeUI/blob/main/include/ernativeui/ERNativeUI.hpp) provides
  the header-only client conveniences.
- The [native UI research library](research/README.md) preserves reproducible
  executable/GFX evidence, rejected probes, and production safety decisions.
