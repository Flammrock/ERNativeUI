# ERNativeUI

![ERNativeUI banner](assets/ernativeui-banner.png)

<p align="center">
  <strong>One native UI host. Multiple client mods. A menu that belongs in Elden Ring.</strong>
</p>

<p align="center">
  <a href="https://github.com/Flammrock/ERNativeUI/releases/latest">Download</a>
  · <a href="https://www.nexusmods.com/eldenring/mods/10767">Nexus Mods</a>
  · <a href="docs/README.md">Documentation</a>
  · <a href="docs/whats-new/1.3.0.md">What's new in 1.3</a>
  · <a href="docs/getting-started/first-mod.md">Build your first mod</a>
  · <a href="examples/README.md">Examples</a>
</p>

ERNativeUI lets mod authors add settings and interactions directly to Elden
Ring's native interface. One process-wide `ERNativeUI.dll` owns the game hooks
and merges content registered by multiple client mods through a stable C ABI
or a friendly header-only C++17 wrapper.

This is not an ImGui or DirectX overlay. ERNativeUI uses Elden Ring's own
controls, pages, dialogs, focus, help text, transitions, controller input,
mouse input, and Back stack.

> ERNativeUI is a Windows x64 mod for offline play with Easy Anti-Cheat
> disabled. Native integration is game-version-sensitive; use a release that
> explicitly supports the installed Elden Ring executable.

<p align="center">
  <img src="assets/ernativeui-separator-1.png" alt="">
</p>

## Why ERNativeUI?

- **Native by design:** menus look and behave like part of Elden Ring.
- **One shared host:** several mods can contribute content without each one
  installing a competing set of UI hooks.
- **A compiler-friendly SDK:** strict C11 ABI, header-only C++17 wrapper, and
  no client import library or shared STL ownership.
- **More than settings rows:** built-in pages, submenus, pagination, dialogs,
  localization, input bindings, and provider storage share one system.
- **Fail-safe native integration:** signatures and object boundaries are
  validated instead of assuming that an old address is still correct.
- **Open and reproducible:** the implementation, API, tests, GFX tooling, and
  reverse-engineering research are available to the community.

## <img src="docs/assets/images/badges/new.svg" alt="[NEW]" height="22"> What's new in ERNativeUI 1.3.0

<table>
  <tr>
    <td width="50%" align="center">
      <a href="docs/guides/custom-tabs.md"><img src="docs/assets/images/menus/custom-tabs-navigation.gif" alt="A custom System tab strip shifting through many client-provided tabs"></a><br>
      <strong><a href="docs/guides/custom-tabs.md">Custom System tabs</a></strong><br>
      <sub>Multiple client mods can add native tabs with their own titles, icons, content, ordering, and overflow navigation.</sub>
    </td>
    <td width="50%" align="center">
      <a href="docs/guides/title-screen-text.md"><img src="docs/assets/images/title-screen/title-screen-corners.png" alt="ERNativeUI text displayed in all four title-screen corners"></a><br>
      <strong><a href="docs/guides/title-screen-text.md">Title-screen text</a></strong><br>
      <sub>Add short version, status, or compatibility lines to any corner with the native color or an explicit RGB color.</sub>
    </td>
  </tr>
</table>

ERNativeUI 1.3.0 also introduces runtime completion for supported settings GFX,
including a configurable System tab strip with eight to ten physical slots,
and support for Elden Ring 1.17.2 / executable 2.7.2.0. New focused examples cover
[custom-tab stress testing](examples/custom_tabs_stress/README.md) and
[title-screen text](examples/title_screen_text/README.md).

[Explore everything new in ERNativeUI 1.3.0](docs/whats-new/1.3.0.md) ·
[Migrate from API 1.2](docs/migrations/1.2-to-1.3.md) ·
[Browse the What's New archive](docs/whats-new/README.md)

## Showcase

<table>
  <tr>
    <td width="50%" align="center">
      <a href="docs/guides/controls/README.md"><img src="docs/assets/images/controls/overview/game-options-controls-overview.png" alt="ERNativeUI controls integrated into Elden Ring Game Options"></a><br>
      <sub>Settings from multiple client mods, merged into Game Options.</sub>
    </td>
    <td width="50%" align="center">
      <a href="docs/guides/native-dialogs.md"><img src="docs/assets/images/dialogs/native-alert-ok-bottom.png" alt="Native greeting dialog with an OK button"></a><br>
      <sub>A localizable client message using Elden Ring's native dialog.</sub>
    </td>
  </tr>
  <tr>
    <td width="50%" align="center">
      <a href="docs/guides/controls/text-input.md"><img src="docs/assets/images/controls/text-input/text-input_fullscreen_after_set.png" alt="Native ERNativeUI text input with a confirmed player name"></a><br>
      <sub>Native text entry with placeholders, confirmed-value callbacks, and persisted values.</sub>
    </td>
    <td width="50%" align="center">
      <a href="docs/guides/controls/color-picker.md"><img src="docs/assets/images/controls/color-picker/color-picker_fullscreen_after_click.png" alt="Native Elden Ring color palette opened from an ERNativeUI color picker"></a><br>
      <sub>Preset palettes and Elden Ring's custom RGB color editor.</sub>
    </td>
  </tr>
  <tr>
    <td width="50%" align="center">
      <a href="docs/guides/menus-and-pages.md"><img src="docs/assets/images/menus/pagination-middle-page.png" alt="Paginated ERNativeUI settings page"></a><br>
      <sub>Automatic pagination, native navigation, and per-page titles.</sub>
    </td>
    <td width="50%" align="center">
      <a href="docs/guides/controls/popup-choice.md"><img src="docs/assets/images/controls/popup-choice/popup-choice-list-open.png" alt="Native popup choice list opened by ERNativeUI"></a><br>
      <sub>Native selection lists for settings with several named choices.</sub>
    </td>
  </tr>
  <tr>
    <td width="50%" align="center">
      <a href="docs/guides/input-bindings.md"><img src="docs/assets/images/input-bindings/controller-bindings-section.png" alt="ERNativeUI actions added to Elden Ring Button Settings"></a><br>
      <sub>Client-defined controller actions in Button Settings.</sub>
    </td>
    <td width="50%" align="center">
      <a href="docs/guides/input-bindings.md"><img src="docs/assets/images/input-bindings/keyboard-mouse-bindings-section.png" alt="ERNativeUI actions added to Elden Ring Key Bindings"></a><br>
      <sub>Native keyboard and mouse assignment and remapping.</sub>
    </td>
  </tr>
  <tr>
    <td colspan="2" align="center">
      <a href="docs/guides/native-dialogs.md"><img src="docs/assets/images/dialogs/native-alert-ok-cancel-bottom.png" alt="Native two-button modal dialog" width="75%"></a><br>
      <sub>Modal messages with configurable buttons and placement.</sub>
    </td>
  </tr>
</table>

<p align="center">
  <img src="assets/ernativeui-separator-2.png" alt="">
</p>

## Supported controls

Every control below has a focused guide with a minimal example, complete
options, screenshots, and links to its native implementation research.

<table>
  <tr>
    <td width="50%" align="center">
      <a href="docs/guides/controls/button.md"><img src="docs/assets/images/controls/button/button_control.png" alt="ERNativeUI native Button control"></a><br>
      <strong><a href="docs/guides/controls/button.md">Button</a></strong><br>
      <sub>Trigger a client-mod action.</sub>
    </td>
    <td width="50%" align="center">
      <a href="docs/guides/controls/submenu.md"><img src="docs/assets/images/controls/submenu/submenu_control.png" alt="ERNativeUI native Submenu control"></a><br>
      <strong><a href="docs/guides/controls/submenu.md">Submenu</a></strong><br>
      <sub>Organize settings in native child pages.</sub>
    </td>
  </tr>
  <tr>
    <td width="50%" align="center">
      <a href="docs/guides/controls/toggle.md"><img src="docs/assets/images/controls/toggle/toggle_control.png" alt="ERNativeUI native Toggle control"></a><br>
      <strong><a href="docs/guides/controls/toggle.md">Toggle</a></strong><br>
      <sub>Expose a native on/off setting.</sub>
    </td>
    <td width="50%" align="center">
      <a href="docs/guides/controls/slider.md"><img src="docs/assets/images/controls/slider/slider_control.png" alt="ERNativeUI native Slider control"></a><br>
      <strong><a href="docs/guides/controls/slider.md">Slider</a></strong><br>
      <sub>Adjust a bounded unsigned-byte value.</sub>
    </td>
  </tr>
  <tr>
    <td width="50%" align="center">
      <a href="docs/guides/controls/numeric-slider.md"><img src="docs/assets/images/controls/numeric-slider/numeric-slider_control.png" alt="ERNativeUI native Numeric Slider displaying x0.6"></a><br>
      <strong><a href="docs/guides/controls/numeric-slider.md">Numeric slider</a></strong><br>
      <sub>Adjust signed or decimal values with units.</sub>
    </td>
    <td width="50%" align="center">
      <a href="docs/guides/controls/custom-slider.md"><img src="docs/assets/images/controls/custom-slider/custom-slider_control.png" alt="ERNativeUI native Custom Slider displaying hello"></a><br>
      <strong><a href="docs/guides/controls/custom-slider.md">Custom slider</a></strong><br>
      <sub>Slide through arbitrary client-defined labels.</sub>
    </td>
  </tr>
  <tr>
    <td width="50%" align="center">
      <a href="docs/guides/controls/inline-choice.md"><img src="docs/assets/images/controls/inline-choice/inline-choice_control.png" alt="ERNativeUI native Inline Choice control"></a><br>
      <strong><a href="docs/guides/controls/inline-choice.md">Inline choice</a></strong><br>
      <sub>Cycle through named options in place.</sub>
    </td>
    <td width="50%" align="center">
      <a href="docs/guides/controls/popup-choice.md"><img src="docs/assets/images/controls/popup-choice/popup-choice_control.png" alt="ERNativeUI native Popup Choice control"></a><br>
      <strong><a href="docs/guides/controls/popup-choice.md">Popup choice</a></strong><br>
      <sub>Select an option from a native list.</sub>
    </td>
  </tr>
  <tr>
    <td width="50%" align="center">
      <a href="docs/guides/controls/text-input.md"><img src="docs/assets/images/controls/text-input/text-input_control.png" alt="ERNativeUI native TextInput control"></a><br>
      <strong><a href="docs/guides/controls/text-input.md">TextInput</a></strong><br>
      <sub>Edit text with a placeholder and configurable limit.</sub>
    </td>
    <td width="50%" align="center">
      <a href="docs/guides/controls/color-picker.md"><img src="docs/assets/images/controls/color-picker/color-picker_control.png" alt="ERNativeUI native ColorPicker control"></a><br>
      <strong><a href="docs/guides/controls/color-picker.md">ColorPicker</a></strong><br>
      <sub>Choose a color with native palette and RGB editors.</sub>
    </td>
  </tr>
</table>

## More than controls

- Give a mod its own top-level Configuration tab with a localized title and
  custom RGBA8 icon. The physical strip defaults to eight slots and, with
  runtime GFX patching enabled, can be configured up to Elden Ring's hard
  ceiling of ten, while overflow keeps up to 1,024 logical custom tabs
  reachable.
  [Learn about custom tabs](docs/guides/custom-tabs.md).
- Add short version or status lines to any title-screen corner, using Elden
  Ring's native text color or an explicit RGB color.
  [Learn about title-screen text](docs/guides/title-screen-text.md), then see
  the [buildable example](examples/title_screen_text/README.md).
- Add ordinary rows to supported built-in Configuration pages, including
  Game Options, Camera Options, Display, Sound, Network, Keyboard/Mouse, and
  Graphics. [Learn about menus and pages](docs/guides/menus-and-pages.md).
- Build nested submenus while ERNativeUI handles capacity, pagination, titles,
  Next, Previous, and the native Back stack.
- Queue native modal dialogs with configurable button combinations and bottom
  or centered placement. [Learn about dialogs](docs/guides/native-dialogs.md).
- Register controller, keyboard, and mouse actions that players can reassign
  in Elden Ring's binding screens. [Learn about input bindings](docs/guides/input-bindings.md).
- Follow the selected game language or a player-configured locale while
  preserving unknown identifiers for future and community translations.
  [Learn about localization](docs/guides/localization.md).
- Load, mutate, and explicitly save provider-owned configuration, or use the
  header-only input-assignment codec with another storage system.
  [Learn about storage](docs/guides/storage.md).
- Let the host complete supported settings GFX at runtime for expanded page
  capacity and native TextInput and ColorPicker presentation. This default-on
  behavior can be disabled globally when a player needs untouched effective
  GFX; structurally compatible GFX supplied by other mods remains supported.
  [Learn about runtime GFX completion](docs/guides/presentation-and-gfx.md).

The complete supported surface, minimum API versions, capabilities, and limits
live in the [feature matrix](docs/features.md).

<p align="center">
  <img src="assets/ernativeui-separator-2.png" alt="">
</p>

## Get started

### Players

Download ERNativeUI from [Nexus Mods](https://www.nexusmods.com/eldenring/mods/10767)
or [GitHub Releases](https://github.com/Flammrock/ERNativeUI/releases/latest),
then follow the [player installation guide](docs/getting-started/player-installation.md).
Install one canonical host and load it before ordinary ERNativeUI client mods.

### Mod authors

The GitHub Windows package contains the host, public headers, CMake package,
built and source examples, licenses, and a link to the documentation for that
exact release tag.
Client mods need only `erui.h`, or `erui.h` plus `ERNativeUI.hpp`; they do not
link an ERNativeUI import library.

1. [Install the SDK](docs/getting-started/setup.md).
2. [Build “Hello, Tarnished!”](docs/getting-started/first-mod.md).
3. Continue from the [commented template](examples/template/README.md) or
   browse the [buildable examples](examples/README.md).

The public ABI is designed for compiler-neutral Windows x64 clients. API 1.3
passes strict MinGW-w64 C11/C++17 compilation and isolated
MinGW-client/MSVC-host fixtures; full in-game MinGW-w64 runtime validation is
not part of the 1.3.0 tested compatibility matrix.

## Compatibility at a glance

- Windows x64, offline play, and Easy Anti-Cheat disabled are required.
- Mod Engine 2 is the primary supported loader.
- Load exactly one `ERNativeUI.dll` for all client mods.
- Use a release that names the installed Elden Ring executable after every
  game update.
- ERNativeUI 1.3.0's API 1.3 paths were live-validated on Elden Ring
  executables 2.7.1.0 and 2.7.2.0. Its retained 2.7.0.0 profile has the reduced
  API 1.0 through 1.2 scope documented in the compatibility matrix.
- Solid Uncapper coexistence has a documented load order and tested matrix.
- Seamless Co-op 2.0.1 was live-tested with the Elden Ring 2.7.0.0 reference
  executable, with no incompatibility observed for that exact combination.
- Elden Ring 2.7.1.0 title layouts and API 1.3's adaptive title spacing were
  live-tested with Shadow of the Erdtree both absent and present; see the
  compatibility guide for the exact scope.
- Host and committed client DLLs must remain loaded until process exit.

See [Game and mod compatibility](docs/COMPATIBILITY.md) for exact tested
versions, modified-GFX compatibility, Solid Uncapper setup, and useful bug
reports.

<p align="center">
  <img src="assets/ernativeui-separator-2.png" alt="">
</p>

## Documentation

| I want to... | Start here |
|---|---|
| Install ERNativeUI as a player | [Player installation](docs/getting-started/player-installation.md) |
| Discover the latest features | [What's new in 1.3.0](docs/whats-new/1.3.0.md) · [Archive](docs/whats-new/README.md) · [API 1.3 migration](docs/migrations/1.2-to-1.3.md) |
| Create my first client mod | [Getting started](docs/getting-started/README.md) |
| Add menus, controls, dialogs, bindings, or localization | [Mod-author guides](docs/guides/README.md) |
| Check supported features and exact API contracts | [Feature matrix](docs/features.md) · [API reference](docs/reference/README.md) |
| Understand the host/client architecture | [How ERNativeUI works](docs/how-it-works.md) |
| Reproduce the native UI research | [Research library](docs/research/README.md) |
| Build, test, contribute, or publish | [Contributing](docs/contributing/README.md) · [Release guide](docs/contributing/releasing.md) |

The [documentation home](docs/README.md) provides the complete reader-oriented
index.

## Versioning

ERNativeUI release versions and ERUI public API versions are independent.
Within a compatible `1.x` host line, earlier published `1.x` APIs remain
available, so a client built with API 1.0 continues to work with a compatible
newer 1.x host. A future host major may drop earlier-major APIs unless its
release explicitly preserves them.

Read [Versioning](docs/versioning.md), the
[changelog](CHANGELOG.md), and the [API migration guides](docs/migrations/README.md)
before declaring a client dependency.

<p align="center">
  <img src="assets/ernativeui-separator-2.png" alt="">
</p>

## Credits

- **[An Thanh Le](https://github.com/AnThanhLe)** — contributed the Vietnamese translation.

## License and contributions

ERNativeUI is my first mod. It is released under the
[MIT License](LICENSE.txt): you may use, modify, redistribute, fork, or improve
it while preserving the license terms and copyright notice.

Bug reports, compatibility updates, documentation improvements, examples, and
carefully validated native-interface research are welcome. Read the
[contribution guide](docs/contributing/README.md) or open a
[GitHub issue](https://github.com/Flammrock/ERNativeUI/issues).
