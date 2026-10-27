# ERNativeUI feature status

This page is the authoritative modder-facing summary of what ERNativeUI
supports, which public API first exposes each feature, and how runtime GFX
completion affects its presentation. For the host/client model and lifecycle, read
[How ERNativeUI works](how-it-works.md). The task-oriented
[mod-author guides](guides/README.md) explain normal use, while the
[strict-C header](https://github.com/Flammrock/ERNativeUI/blob/main/include/ernativeui/erui.h) and
[C++17 wrapper](https://github.com/Flammrock/ERNativeUI/blob/main/include/ernativeui/ERNativeUI.hpp) are the normative public
contracts.

## API status

| Label | Meaning |
|---|---|
| **Current API 1.4 pre-release** | The feature scope is complete for release preparation. It adds typed row references plus runtime text/style, availability, visibility, and repagination updates. The contract is not frozen or published yet. |
| **Latest published API 1.3** | First shipped by ERNativeUI 1.3.0. It adds custom top-level Configuration tabs and title-screen status text. |
| **Frozen APIs 1.0 through 1.3** | Already-compiled clients remain supported by compatible 1.x hosts. |
| **Future research** | Not implemented in the public API. Do not design a client around it yet. |

ERNativeUI release versions and public API versions are independent. A package
version must not be inferred from the API number. See [Versioning](versioning.md)
and the permanent [API migration guides](migrations/README.md).

The “minimum API” column names the C ABI version in which a feature first
appears. The API 1.4 `ERNativeUI.hpp` requests API 1.4 exactly; wrappers shipped
in the API 1.0 through 1.3 SDKs continue to request their frozen tables.
Choosing an older SDK is valid; a client does not silently upgrade its runtime
API when the player installs a newer compatible host.

## Features introduced in API 1.0

| Feature | What the player sees | Minimum API and capability | GFX presentation | Important limits and examples |
|---|---|---|---|---|
| Shared host and provider merge | Rows from several client mods appear together in Elden Ring's native **System → Game Options** page. | 1.0; core registration has no separate capability bit. | No. | One process-wide `ERNativeUI.dll`; provider topology is fixed after startup. See [How it works](how-it-works.md). |
| [Toggle](guides/controls/toggle.md) | A native on/off row. | 1.0; `ERUI_CAP_TOGGLE` | Uses a stock widget; when runtime patching is enabled, completion increases supported page capacity. | Value is a host-owned byte; player changes notify the client. |
| [Slider](guides/controls/slider.md) | A native byte-valued slider. | 1.0; `ERUI_CAP_SLIDER` | Uses a stock widget; when runtime patching is enabled, completion increases supported page capacity. | Bounds, step, initial value, and enabled state are declared at registration. |
| [Button](guides/controls/button.md) | A native action row invoking client code. | 1.0; `ERUI_CAP_BUTTON` | When runtime patching is enabled, completion supplies the matching left-label field on affected action rows. | Runs synchronously on Elden Ring's UI thread; keep callbacks short. |
| [Submenu](guides/controls/submenu.md) | A native row opening a provider-owned settings page with native focus, transitions, and Back behavior. | 1.0; `ERUI_CAP_SUBMENU` | No; the shared action-row/capacity improvements above still apply. | A disabled submenu is omitted because the native action-row constructor has no proven disabled presentation. |
| [Inline choice](guides/controls/inline-choice.md) | A named value changed in place with left/right input. | 1.0; `ERUI_CAP_INLINE_CHOICE` | No. | 1–32 non-empty labels; callbacks and getters use a zero-based index. Choice rows have no supported disabled state. |
| [Popup choice](guides/controls/popup-choice.md) | An action row opening Elden Ring's native selection list. | 1.0; `ERUI_CAP_POPUP_CHOICE` | When runtime patching is enabled, completion supplies affected action-row presentation. | Same 1–32 labels and zero-based value model as inline choice. [Showcase](../examples/tarnished_ui_showcase/README.md). |
| Automatic pagination | Native Next/Previous rows when logical content exceeds the current panel capacity. | 1.0; `ERUI_CAP_PAGINATION` | When runtime patching is enabled, completion expands supported ordinary pages before live capacity is measured. | The host always paginates against live capacity and subtracts every row already materialized by the game or an earlier compatible hook. Clients add logical content only; they must not create their own pagination rows. |
| Page presentation | Provider submenu headings and base titles, default `Title (n/t)` pagination suffixes, and an optional startup formatter. | 1.0; `ERUI_CAP_PAGE_PRESENTATION` | No. | The shared Game Options heading remains game-owned. Formatters run during startup compilation, not when a page opens. |
| [Native modal alerts](guides/native-dialogs.md) | Bottom or centered native messages with dismiss-only, OK, CANCEL, YES, NO, OK+CANCEL, or YES+NO responses. | 1.0; `ERUI_CAP_ALERT` | No. | Requests share a bounded FIFO. Completion is asynchronous; only one owned modal is presented at a time. [Localized example](../examples/localized_greeting/README.md). |
| [Game language and localization](guides/localization.md) | Client text can follow Elden Ring's selected Steam game language or one explicit host-wide locale; unknown identifiers remain available for forward-compatible localization. | 1.0; `ERUI_CAP_GAME_LANGUAGE` | No. Host-owned Previous/Next translations use separate locale INI files. | `Language = auto` uses Steam; an explicit identifier affects the host and every client. Fall back when unavailable or when the client has no translation for the exact identifier. [Localized example](../examples/localized_greeting/README.md). |
| Host-owned row values | A client can read or programmatically update committed toggle, slider, and choice values. | 1.0; `ERUI_CAP_HOST_OWNED_VALUES` | No. | Programmatic writes do not synthesize player-change callbacks and reach the native presentation asynchronously. |

## Features introduced in API 1.1

| Feature | What the player sees | Minimum API and capability | GFX presentation | Important limits and examples |
|---|---|---|---|---|
| Explicit connection lifecycle | No new widget; clients wait for host startup and language selection before registering through one negotiated connection. | 1.1; no separate capability bit. | No. | `erui::connect()` runs on a worker after `DllMain`, requests the exact API selected by its SDK header, and never silently downgrades. |
| [Text input](guides/controls/text-input.md) | A native editable text row with a placeholder and confirmed value. | 1.1; `ERUI_CAP_TEXT_INPUT` | When runtime patching is enabled, completion supplies the red empty state and bracketed character-name frame; otherwise the functional fallback remains. | Fixed per-row limit of 1–35 UTF-16 code units; default 16. Only a changed confirmation notifies the client. [Template](../examples/template/README.md). |
| [RGB color picker](guides/controls/color-picker.md) | A row opening Elden Ring's native character-creation color editor. | 1.1; `ERUI_CAP_COLOR_PICKER` | When runtime patching is enabled, completion supplies the standalone bracketed live color swatch; otherwise the functional action-row fallback remains. | RGB only; no public alpha or packed game value. One editor may be open process-wide. [Showcase](../examples/tarnished_ui_showcase/README.md). |
| Additional built-in pages | Ordinary ERNativeUI rows can appear in Camera Options, Display, Sound, Network, Keyboard/Mouse Settings, and Graphics as well as Game Options. | 1.1; `ERUI_CAP_BUILTIN_PAGES` | When runtime patching is enabled, completion expands supported ordinary pages to their safe capacities before materialization. | Each destination resolves and paginates independently against live capacity. Network uses ten rows to preserve its warning when completion is enabled; Controller Settings is intentionally not an ordinary destination. |
| [Native input bindings](guides/input-bindings.md) | Provider sections and actions appear in Button Settings and/or Keyboard/Mouse Settings according to their supported controller, keyboard, and mouse slots. | 1.1; `ERUI_CAP_INPUT_BINDINGS` | No additional GFX transformation. | One alternative per device family; global rising-edge observation does not consume game input. Modifier chords, Escape, and F1–F12 are not exposed. [Template](../examples/template/README.md). |
| [Live binding updates](guides/input-bindings.md#8-how-do-i-query-or-change-a-live-action) | Player assignment/Clear updates both the native rows and action dispatch; clients may bind, unbind, query, or reset an action programmatically. | 1.1; `ERUI_CAP_INPUT_BINDINGS` | No. | Programmatic changes are silent. Player changes use one optional provider-wide batch callback. |
| [Provider storage](guides/storage.md) | Mods may explicitly load, mutate, and atomically save typed INI data, including semantic input assignments. | 1.1; `ERUI_CAP_STORAGE` | No. | Host-backed: it requires a successful API 1.1 connection and active provider registration to open. Nothing loads or saves automatically. The default directory uses the provider ID when that cannot create a Windows path alias; otherwise it uses an isolated `~<sha256>` name. |
| Header-only assignment codec | Mods with another configuration backend can parse and format stable controller/keyboard/mouse names without loading the host. | API 1.1 header utility; no runtime capability required for the codec itself. | No. | Preserves the difference between unsupported, supported-unbound, and bound slots. Runtime application still requires Input Bindings. See [Input values and codec](reference/input-values-and-codec.md). |

## Features introduced in API 1.2

| Feature | What the player sees | Minimum API and capability | GFX presentation | Important limits and examples |
|---|---|---|---|---|
| [Numeric Slider](guides/controls/numeric-slider.md) | A native slider displaying signed or decimal values with optional prefix and suffix text, such as `x0.6`, `-0.5`, or `3.0s`. | 1.2; `ERUI_CAP_NUMERIC_SLIDER` | No. | Exact base-10 arithmetic, 0–9 decimal places, and 2–256 logical positions. Callbacks expose the friendly number, exact scaled integer, and logical index. |
| [Custom Slider](guides/controls/custom-slider.md) | A native slider whose positions can display completely unrelated labels. | 1.2; `ERUI_CAP_CUSTOM_SLIDER` | No. | 2–256 copied labels, each at most 64 UTF-16 code units. The C++ formatter runs only during registration; callbacks expose the selected index. |

The API 1.2 prefix remains frozen. It appends the two slider blocks after the
frozen API 1.1 prefix.

## Features introduced in API 1.3

| Feature | What the player sees | Minimum API and capability | GFX presentation | Important limits and examples |
|---|---|---|---|---|
| [Custom Configuration tabs](guides/custom-tabs.md) | A client-owned title and optional custom icon in the top-level System tab strip, with native selection, overflow and page content. | 1.3; `ERUI_CAP_CUSTOM_TABS` | No. | Exact 32x32 RGBA8 icon or host fallback; 8 physical slots by default, configurable from 8 through the hard native ceiling of 10; 1,024 committed logical custom tabs process-wide through the shifting viewport. Returned pages use ordinary controls and automatic pagination. [Screenshot helper](../examples/showcase_screenshot_helper/README.md); [stress example](../examples/custom_tabs_stress/README.md). |
| [Title-screen status text](guides/title-screen-text.md) | One or more copied provider status/version lines in an explicitly selected screen corner, optionally using an opaque RGB color. | 1.3; `ERUI_CAP_TITLE_SCREEN_TEXT`; runtime GFX patching must be enabled. | Runtime transformation of the effective title GFX. | Mandatory top-left, top-right, bottom-left, or bottom-right corner; 1..255 UTF-16 code units per line; 16 committed lines process-wide; inherited native color or explicit RGB. [Example](../examples/title_screen_text/README.md). |

The API 1.3 C function table appends `add_tab` and
`add_title_screen_text` after the frozen API 1.2 prefix. Capability checks
describe the negotiated host surface. Custom tabs and title-screen text remain
independently gated: an API 1.3 connection can succeed while either capability
is absent when its required native boundary has not been validated. Disabling
runtime GFX patching additionally removes the title-screen-text capability but
does not remove custom tabs.

## Features introduced in API 1.4

| Feature | What the player sees | Minimum API and capability | GFX presentation | Important limits and examples |
|---|---|---|---|---|
| Typed C++ row references | No visual change by itself; retained controls expose only the value and edit operations valid for their row type. | API 1.4 C++ wrapper; value methods retain their original capability requirements. | No. | Ordinary `Page::add_*` builders return typed references that remain implicitly convertible to the frozen raw `RowHandle`. Button has no canonical value; Submenu keeps its existing page-returning API. [Value guide](guides/row-updates/values.md). |
| [Runtime row text and style](guides/row-updates/text-and-styles.md) | Labels, focused help, and supported Button/Popup Choice captions can change after registration, including mixed color and underline. | 1.4; `ERUI_CAP_UPDATES` | Uses validated text fields in the effective native movie; no additional GFX transformation. | Plain text is escaped. Structured text is limited to 4,096 UTF-16 code units and 256 spans. Bold/italic remain dependent on effective font faces. |
| [Runtime row availability](guides/row-updates/availability.md) | A visible row can be skipped by focus and independently use its normal or disabled presentation. | 1.4; `ERUI_CAP_UPDATES` and `ERUI_CAP_ROW_AVAILABILITY` | No. | Supported for the nine ordinary materialized row families; Submenu and omitted construction-disabled rows are excluded. |
| [Runtime row visibility and repagination](guides/row-updates/visibility.md) | Rows appear or disappear in place; page slices, navigation rows, titles, focus, and subpage scrollbars are rebuilt without closing the page. | 1.4; `ERUI_CAP_UPDATES` and `ERUI_CAP_ROW_VISIBILITY` | No. | Supported on built-in pages, custom-tab content, and subpages for the nine ordinary materialized row families. A committed projection cannot leave a logical page empty. This does not hide or show the custom tab itself. |

The API 1.4 C table appends one provider-scoped `commit_updates` operation.
Its array batch is validated, copied, and published atomically; a failed item
leaves the previous logical generation intact. The C++ wrapper exposes
one-row `row.edit()...commit()` drafts. A C++ multi-row collector, recursive
Collapse controls, runtime custom-tab visibility, and runtime Submenu
availability/visibility are not part of API 1.4.

## Runtime GFX completion

With the default `[GFX] EnableRuntimePatching = 1`, ERNativeUI intercepts the
supported settings and title movies at their validated Scaleform file-opening
boundary. It structurally validates the effective bytes, adds only missing row
placements, widget presentation, and registered title-screen fields,
serializes and reparses the result, then publishes it in memory. No loose
ERNativeUI GFX files are normally required.

The ordinary target capacities are 13 rows for Game, Camera, Display, Sound,
Keyboard/Mouse, and Graphics, and 10 for Network so its native restriction
warning remains unobstructed. Advanced Settings retains its existing 15-row
capacity. Both supported movies receive TextInput and ColorPicker presentation
when missing.

Structurally compatible replacements supplied by other mods remain supported.
Already-complete inputs are unchanged; unrecognized inputs continue with their
original bytes. Setting `EnableRuntimePatching = 0` and restarting prevents the
hook from being installed, leaves every effective GFX file unchanged, and
disables the additions described above. Logical pages still paginate against
the live capacity, custom tabs remain available, and TextInput and ColorPicker
retain their functional fallbacks; title-screen text is not advertised. See
[runtime GFX completion and presentation](guides/presentation-and-gfx.md).

## Future API research, not currently supported

The following are investigation targets beyond API 1.4, not delivery promises. A
feature that cannot yet satisfy ERNativeUI's lifetime, input, teardown,
coexistence, and validation requirements may move to a later API.

| Feature | Intended player experience | Current status |
|---|---|---|
| Site of Grace integration | Provider actions, dialogs, or pages reachable from the native Site of Grace menu. | Future research. No public capability or supported hook path exists yet. |
| Reusable native page templates | Provider-owned pages styled and structured like other Elden Ring interfaces, such as the Crystal Tears menu. | Future research. Safe construction, data binding, input, and teardown have not been proven. |
| Runtime-loaded provider GFX | A mod authors its own GFX movie and asks ERNativeUI to load and drive it at runtime. | Future research. Movie loading, events, focus, resource ownership, paths, and cross-mod conflicts must be investigated first. |
| Ordinary rows in Controller Settings | Generic toggles/buttons mixed into the controller remapping page. | Not planned as a generic built-in page. API 1.1 uses the specialized native Input Bindings model there instead. |

## Platform and compatibility limits

- Windows x64 only; run offline with Easy Anti-Cheat disabled.
- Native signatures are Elden Ring-version-sensitive and are validated before
  use. A game update can require a host update even when the public ABI is
  unchanged.
- The host requires an MSVC-compatible ABI internally. The strict C boundary
  is designed for compiler-neutral clients and requires no import library.
  API 1.3 passes strict MinGW-w64 compilation and isolated
  MinGW-client/MSVC-host fixtures, but in-game MinGW-w64 runtime compatibility
  is not part of the 1.3.0 tested matrix.
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
- Elden Ring 2.7.1.0 title layouts and the adaptive title-spacing branch were
  live-tested both without and with Shadow of the Erdtree. Broader DLC
  compatibility is not inferred from that title-screen validation.

See [Game and mod compatibility](COMPATIBILITY.md) for tested combinations and
the reporting checklist. The exact ownership and directional API 1.0
client/new-host contract are in
[Lifecycle, errors, and limits](reference/lifecycle-errors-and-limits.md#earlier-api-binary-compatibility).

## Choose an example

| Goal | Start here |
|---|---|
| Build a reusable client-mod skeleton | [Client-mod template](../examples/template/README.md) |
| Learn connection, language fallback, and one alert | [Localized Greeting](../examples/localized_greeting/README.md) |
| Exercise the broad API 1.0 through 1.4 feature set in game | [Tarnished UI Showcase](../examples/tarnished_ui_showcase/README.md) |

Return to the [documentation home](README.md).
