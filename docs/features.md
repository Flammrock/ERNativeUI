# ERNativeUI feature status

This page is the authoritative modder-facing summary of what ERNativeUI
supports, which public API first exposes each feature, and whether a loose GFX
asset changes its presentation. For the host/client model and lifecycle, read
[How ERNativeUI works](how-it-works.md). The task-oriented
[mod-author guides](guides/README.md) explain normal use, while the
[strict-C header](https://github.com/Flammrock/ERNativeUI/blob/main/include/ernativeui/erui.h) and
[C++17 wrapper](https://github.com/Flammrock/ERNativeUI/blob/main/include/ernativeui/ERNativeUI.hpp) are the normative public
contracts.

## API status

| Label | Meaning |
|---|---|
| **Current API 1.1** | The current client contract. ERNativeUI 1.1.0 is the first release line that packages it. |
| **Supported API 1.0** | The frozen original contract. Any client built with the API 1.0 SDK remains supported by compatible 1.x hosts. |
| **Future research** | Not implemented in the public API. Do not design a client around it yet. |

ERNativeUI release versions and public API versions are independent. The
ERNativeUI 1.1.0 release happens to introduce API 1.1, but that matching number
does not couple their future evolution. See [Versioning](versioning.md) and the
permanent [API migration guides](migrations/README.md).

The “minimum API” column names the C ABI version in which a feature first
appears. The current `ERNativeUI.hpp` deliberately connects to API 1.1 exactly;
the wrapper shipped in the API 1.0 SDK continues to use the frozen API 1.0
table. Choosing either SDK is valid; a client does not silently upgrade its
runtime API when the player installs a newer compatible host.

## Features introduced in API 1.0

| Feature | What the player sees | Minimum API and capability | Optional GFX | Important limits and examples |
|---|---|---|---|---|
| Shared host and provider merge | Rows from several client mods appear together in Elden Ring's native **System → Game Options** page. | 1.0; core registration has no separate capability bit. | No. | One process-wide `ERNativeUI.dll`; provider topology is fixed after startup. See [How it works](how-it-works.md). |
| [Toggle](guides/controls/toggle.md) | A native on/off row. | 1.0; `ERUI_CAP_TOGGLE` | No. The `02_040` patch only changes available capacity. | Value is a host-owned byte; player changes notify the client. |
| [Slider](guides/controls/slider.md) | A native byte-valued slider. | 1.0; `ERUI_CAP_SLIDER` | No. The `02_040` patch only changes available capacity. | Bounds, step, initial value, and enabled state are declared at registration. |
| [Button](guides/controls/button.md) | A native action row invoking client code. | 1.0; `ERUI_CAP_BUTTON` | Not required for activation. The optional `02_040` patch supplies the matching left-label field on Game/Camera action rows. | Runs synchronously on Elden Ring's UI thread; keep callbacks short. |
| [Submenu](guides/controls/submenu.md) | A native row opening a provider-owned settings page with native focus, transitions, and Back behavior. | 1.0; `ERUI_CAP_SUBMENU` | No; the shared action-row/capacity improvements above still apply. | A disabled submenu is omitted because the native action-row constructor has no proven disabled presentation. |
| [Inline choice](guides/controls/inline-choice.md) | A named value changed in place with left/right input. | 1.0; `ERUI_CAP_INLINE_CHOICE` | No. | 1–32 non-empty labels; callbacks and getters use a zero-based index. Choice rows have no supported disabled state. |
| [Popup choice](guides/controls/popup-choice.md) | An action row opening Elden Ring's native selection list. | 1.0; `ERUI_CAP_POPUP_CHOICE` | No; action-row presentation can benefit from `02_040`. | Same 1–32 labels and zero-based value model as inline choice. [Showcase](../examples/tarnished_ui_showcase/README.md). |
| Automatic pagination | Native Next/Previous rows when logical content exceeds the current panel capacity. | 1.0; `ERUI_CAP_PAGINATION` | No. The optional 13-row GFX reduces how often Game Options must paginate. | The host subtracts every row already materialized by the game or an earlier compatible hook. Clients add logical content only; they must not create their own pagination rows. |
| Page presentation | Provider submenu headings and base titles, default `Title (n/t)` pagination suffixes, and an optional startup formatter. | 1.0; `ERUI_CAP_PAGE_PRESENTATION` | No. | The shared Game Options heading remains game-owned. Formatters run during startup compilation, not when a page opens. |
| [Native modal alerts](guides/native-dialogs.md) | Bottom or centered native messages with dismiss-only, OK, CANCEL, YES, NO, OK+CANCEL, or YES+NO responses. | 1.0; `ERUI_CAP_ALERT` | No. | Requests share a bounded FIFO. Completion is asynchronous; only one owned modal is presented at a time. [Localized example](../examples/localized_greeting/README.md). |
| [Game-language discovery](guides/localization.md) | Client text can follow Elden Ring's selected Steam language; unknown identifiers remain available for forward-compatible localization. | 1.0; `ERUI_CAP_GAME_LANGUAGE` | No. Host-owned Previous/Next translations use separate locale INI files. | Fall back to English when unavailable or unknown. [Localized example](../examples/localized_greeting/README.md). |
| Host-owned row values | A client can read or programmatically update committed toggle, slider, and choice values. | 1.0; `ERUI_CAP_HOST_OWNED_VALUES` | No. | Programmatic writes do not synthesize player-change callbacks and reach the native presentation asynchronously. |

## Features introduced in API 1.1

| Feature | What the player sees | Minimum API and capability | Optional GFX | Important limits and examples |
|---|---|---|---|---|
| Explicit connection lifecycle | No new widget; clients wait for host startup and language discovery before registering through one negotiated connection. | 1.1; no separate capability bit. | No. | `erui::connect()` runs on a worker after `DllMain`, requests 1.1 exactly, and never silently downgrades. |
| [Text input](guides/controls/text-input.md) | A native editable text row with a placeholder and confirmed value. | 1.1; `ERUI_CAP_TEXT_INPUT` | Optional `02_040`/`02_042` character-name presentation adds the red empty state and bracketed frame. Editing still works without it. | Fixed per-row limit of 1–35 UTF-16 code units; default 16. Only a changed confirmation notifies the client. [Template](../examples/template/README.md). |
| [RGB color picker](guides/controls/color-picker.md) | A row opening Elden Ring's native character-creation color editor. | 1.1; `ERUI_CAP_COLOR_PICKER` | Optional `02_040`/`02_042` standalone widget adds the bracketed live color swatch. Without it, the row remains a functional action-row fallback. | RGB only; no public alpha or packed game value. One editor may be open process-wide. [Showcase](../examples/tarnished_ui_showcase/README.md). |
| Additional built-in pages | Ordinary ERNativeUI rows can appear in Camera Options, Display, Sound, Network, Keyboard/Mouse Settings, and Graphics as well as Game Options. | 1.1; `ERUI_CAP_BUILTIN_PAGES` | Usually no. On the tested native layout Camera Options is already full, so `02_040` must expand that panel before it has room for an ERNativeUI row or Next row. | Each destination resolves and paginates independently. Controller Settings is intentionally not an ordinary destination. |
| [Native input bindings](guides/input-bindings.md) | Provider sections and actions appear in Button Settings and/or Keyboard/Mouse Settings according to their supported controller, keyboard, and mouse slots. | 1.1; `ERUI_CAP_INPUT_BINDINGS` | No binding-screen GFX patch. | One alternative per device family; global rising-edge observation does not consume game input. Modifier chords, Escape, and F1–F12 are not exposed. [Template](../examples/template/README.md). |
| [Live binding updates](guides/input-bindings.md#8-how-do-i-query-or-change-a-live-action) | Player assignment/Clear updates both the native rows and action dispatch; clients may bind, unbind, query, or reset an action programmatically. | 1.1; `ERUI_CAP_INPUT_BINDINGS` | No. | Programmatic changes are silent. Player changes use one optional provider-wide batch callback. |
| [Provider storage](guides/storage.md) | Mods may explicitly load, mutate, and atomically save typed INI data, including semantic input assignments. | 1.1; `ERUI_CAP_STORAGE` | No. | Host-backed: it requires a successful API 1.1 connection and active provider registration to open. Nothing loads or saves automatically. The default directory uses the provider ID when that cannot create a Windows path alias; otherwise it uses an isolated `~<sha256>` name. |
| Header-only assignment codec | Mods with another configuration backend can parse and format stable controller/keyboard/mouse names without loading the host. | API 1.1 header utility; no runtime capability required for the codec itself. | No. | Preserves the difference between unsupported, supported-unbound, and bound slots. Runtime application still requires Input Bindings. See [Input values and codec](reference/input-values-and-codec.md). |

The API 1.1 C function table is the current public contract. Capability checks
describe the negotiated host surface; game-version validation can still
disable an individual built-in destination or make a required compiled feature
fail safely when its native boundary cannot be proven.

## Optional presentation assets

ERNativeUI's native integration does not require the DLL to parse or modify GFX
at runtime. Release packages include two optional loose movies:

| Asset | Effect |
|---|---|
| `menu/win/02_040_optionsetting.gfx` | Expands Game Options and Camera Options to 13 visual rows, supplies the left label field for their shared action widget, and carries the optional TextInput and ColorPicker presentations. |
| `menu/win/02_042_pc_graphicsetting.gfx` | Carries the same optional TextInput and ColorPicker presentations for provider-owned Advanced Settings pages. |

Without the assets, ERNativeUI reads the live native capacity, paginates where
there is a free slot, and uses documented widget fallbacks. A completely full
panel—Camera Options on the tested build—cannot display even a Next row until
capacity is expanded. The transformations can be reproduced from the user's
own extracted files with the
[presentation and GFX guide](guides/presentation-and-gfx.md); the structural
implementation reference is in the
[patcher README](../tools/gfx_patcher/README.md).

## Future API research, not currently supported

The following are API 1.2 investigation targets, not delivery promises. A
feature that cannot yet satisfy ERNativeUI's lifetime, input, teardown,
coexistence, and validation requirements may move to API 1.3 or later.

| Feature | Intended player experience | Current status |
|---|---|---|
| Custom top-level Configuration tabs | Provider-defined tab names and icons, navigable by mouse and shoulder buttons without a fixed logical tab limit. | API 1.2 research target. No public descriptor, icon contract, merge policy, or runtime backend exists yet. |
| Site of Grace integration | Provider actions, dialogs, or pages reachable from the native Site of Grace menu. | API 1.2 research target. No public capability or supported hook path exists yet. |
| Reusable native page templates | Provider-owned pages styled and structured like other Elden Ring interfaces, such as the Crystal Tears menu. | API 1.2 research target. Safe construction, data binding, input, and teardown have not been proven. |
| Runtime-loaded provider GFX | A mod authors its own GFX movie and asks ERNativeUI to load and drive it at runtime. | API 1.2 research target. Movie loading, events, focus, resource ownership, paths, and cross-mod conflicts must be investigated first. |
| Ordinary rows in Controller Settings | Generic toggles/buttons mixed into the controller remapping page. | Not planned as a generic built-in page. API 1.1 uses the specialized native Input Bindings model there instead. |

## Platform and compatibility limits

- Windows x64 only; run offline with Easy Anti-Cheat disabled.
- Native signatures are Elden Ring-version-sensitive and are validated before
  use. A game update can require a host update even when the public ABI is
  unchanged.
- The host requires an MSVC-compatible ABI internally. Client DLLs can use
  MSVC, clang-cl, or MinGW through the strict C boundary; no import library is
  required.
- The host and committed providers cannot be hot-unloaded. Callback code and
  long-lived callback state must remain valid until their documented lifetime
  ends.
- Input-binding callbacks are global observations. Mods decide their own game
  context policy, and the underlying Elden Ring input may also act.
- Solid Uncapper 2.3 coexistence was live-validated on Elden Ring executables
  2.7.0.0 and 2.7.1.0 when Solid Uncapper loads first. The 1.1.1 host patch
  deliberately validates and chains its shared menu, API 1.1 Input Bindings,
  and ColorPicker overlaps.
- Seamless Co-op 2.0.1 was live-validated on Elden Ring executable 2.7.0.0,
  with no incompatibility observed for that exact combination.
- DLC compatibility has not been live-validated; no incompatibility is
  currently claimed or ruled out.

See [Game and mod compatibility](compatibility.md) for tested combinations and
the reporting checklist. The exact ownership and directional API 1.0
client/new-host contract are in
[Lifecycle, errors, and limits](reference/lifecycle-errors-and-limits.md#api-10-binary-compatibility).

## Choose an example

| Goal | Start here |
|---|---|
| Build a reusable client-mod skeleton | [Client-mod template](../examples/template/README.md) |
| Learn connection, language fallback, and one alert | [Localized Greeting](../examples/localized_greeting/README.md) |
| Exercise the complete current feature set in game | [Tarnished UI Showcase](../examples/tarnished_ui_showcase/README.md) |

Return to the [documentation home](README.md).
