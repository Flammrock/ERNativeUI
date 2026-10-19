# Showcase Screenshot Helper

`ShowcaseScreenshotHelper.dll` creates one small, predictable view for taking
ERNativeUI documentation screenshots. It can isolate one control or register
one example custom tab. It is also a compact example of reading a
client-specific INI through ERNativeUI's host-owned Storage service.

The helper is intentionally English-only. It never queries the game language
and all client-provided labels are fixed English strings. Set Elden Ring to
English too when taking canonical screenshots so native values such as
**On** and **OK** also appear in English.

## Select one showcase

Keep `ShowcaseScreenshotHelper.ini` beside the DLL:

```text
ShowcaseScreenshotHelper.dll
ShowcaseScreenshotHelper.ini
```

Edit its mode while the game is closed:

```ini
[showcase]
mode=button
```

The accepted values are:

| Mode | Result |
|---|---|
| `button` | **Show Greeting**, which opens **Hello, Tarnished!** |
| `submenu` | **Open Details**, leading to one small child page |
| `toggle` | **Enabled**, initially **On** |
| `slider` | **Intensity**, initially `50` in a `0..100` range |
| `numeric-slider` | **Camera Distance**, initially `x0.6` in a `0.1..2.0` range |
| `custom-slider` | **Impossible Values**, initially **hello** among seven unrelated labels |
| `inline-choice` | **Quality**, initially **Balanced** |
| `popup-choice` | **Quality Preset**, with four named values |
| `text-input` | Empty **Player Name** with **Enter a name** placeholder |
| `color-picker` | **Accent Color**, initially RGB `(171, 125, 99)` |
| `custom-tab` | Top-level **My Mod Settings** tab with a generated `rgb(107, 100, 80)` triangle icon and one toggle |

Restart Elden Ring after changing the mode. Menu topology is registered once
during startup, so editing the file while the game is running cannot replace
the current page. For the control modes, open **System -> Game Options ->
_Control_ Showcase** to see the selected control by itself. For `custom-tab`,
open **System** and select the top-level **My Mod Settings** tab.

This build uses the current API 1.3 headers, so every mode requires a host
exposing API 1.3. The `custom-tab` mode additionally requires the optional
custom-tab capability on a supported game executable.
For matching before/after screenshots, press Right once on **Camera
Distance** to change `x0.6` to `x0.7`. Press Left once on **Impossible
Values** to change **hello** to `1s`.

An absent or unreadable mode safely falls back to `button`. If the INI or its
`mode` key is missing, the example attempts to create the default entry. An
unknown value is left untouched so a spelling mistake remains visible.

## Capture and generate the slider images

Use the same game resolution, framing, and menu position as the existing
control screenshots. For Numeric Slider, save the initial `x0.6` page and the
page after one Right press as:

```text
docs/assets/images/controls/numeric-slider/
|-- numeric-slider_fullscreen_before_click.png
`-- numeric-slider_fullscreen_after_click.png
```

Then change the INI to `mode=custom-slider`, restart the game, and capture
**hello** followed by `1s` after one Left press under:

```text
docs/assets/images/controls/custom-slider/
|-- custom-slider_fullscreen_before_click.png
`-- custom-slider_fullscreen_after_click.png
```

Generate both documentation crops from the repository root:

```powershell
.\tools\generate-control-crops.ps1 `
    -ControlsRoot .\docs\assets\images\controls
```

The script uses each `before_click` image to create the matching `_row.png`
and `_control.png`. The `after_click` image remains the full-page example of
the changed value. Existing crops are skipped by default, so this command does
not re-encode documentation images that are already present.

To intentionally regenerate only these two controls, use:

```powershell
.\tools\generate-control-crops.ps1 `
    -ControlsRoot .\docs\assets\images\controls `
    -Control numeric-slider,custom-slider `
    -Force
```

## How the INI is read

The example deliberately uses the public storage API:

```cpp
auto config = menu.storage(
    erui::StorageOptions::beside_module(
        L"ShowcaseScreenshotHelper.ini"));

if (config.load() == ERUI_OK) {
    auto mode = config.section("showcase").get<std::string>("mode");
    if (mode.found()) {
        // Select the one control registered for this process.
    }
}
```

`beside_module` resolves relative to this client DLL because
`ProviderOptions::owner_module` is supplied. The default `menu.storage()` path
would instead live under `mods/<provider_id>/config.ini` beside the shared
host. Loading, changing memory, and saving are explicit operations; ERNativeUI
does not silently persist arbitrary client values.

## Build and deploy

From the repository root:

```bat
cmake --preset windows-release
cmake --build --preset windows-release --target ShowcaseScreenshotHelperStage
```

The normal full build stages the DLL and a copy-if-missing default INI under:

```text
build\preset-release\deploy\Release\examples\
```

Load the shared host before the helper:

```toml
[modengine]
external_dlls = [
    "mod\\ERNativeUI\\ERNativeUI.dll",
    "mod\\ERNativeUI\\examples\\ShowcaseScreenshotHelper.dll",
]
```

No loose GFX assets are required. ERNativeUI completes the supported TextInput
and ColorPicker presentation in memory while the settings movies load. If an
asset mod replaces either movie with an unrecognized layout, the host leaves
that movie unchanged and the controls use their documented fallback appearance.

Return to the [examples overview](../README.md) or the
[control documentation](../../docs/guides/controls/README.md). For the
`custom-tab` mode, continue with the
[Custom Configuration tabs guide](../../docs/guides/custom-tabs.md).
