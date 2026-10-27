# Showcase Screenshot Helper

`ShowcaseScreenshotHelper.dll` creates one small, predictable view for taking
ERNativeUI documentation screenshots. It can isolate one control, register
one example custom tab, or place deterministic example text around the title
screen. With API 1.4, it can also stage one compact scene for each row-update
presentation concept: text and style, visibility, and availability. It is a
compact example of reading a client-specific INI through ERNativeUI's
host-owned Storage service.

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
| `row-text-and-styles` | **Showcase Intensity**, whose percentage updates in its colored label and focused help from red at 0% to green at 100% |
| `row-text-and-styles-2` | A colorful Button plus value-dependent Numeric Slider and Toggle labels using discrete native, red, green, and gold states |
| `row-visibility` | **Show advanced setting**, one conditional Slider, and one stable row for a clean layout-rebuild capture |
| `row-availability` | **Enable online setting**, a visible dimmed/skipped Slider, and one always-available row |
| `custom-tab` | Top-level **My Mod Settings** tab with a generated `rgb(107, 100, 80)` triangle icon and one toggle |
| `title-screen-text` | One labeled line in each title-screen corner: one native color and three subdued custom RGB colors |

Restart Elden Ring after changing the mode. Menu topology is registered once
during startup, so editing the file while the game is running cannot replace
the current page. For the control modes, open **System -> Game Options ->
_Control_ Showcase** to see the selected control by itself. For `custom-tab`,
open **System** and select the top-level **My Mod Settings** tab. For
`title-screen-text`, remain on the title screen after startup; this mode does
not add a System-menu page. For each `row-*` mode, open **System -> Game
Options** and select its page: **Text & Style Updates**, **Text & Style
Updates 2**, **Row Visibility**, or **Row Availability**.

The helper DLL is compiled with the exact-version API 1.4 wrapper, so every
mode requires a host exposing API 1.4. Both `row-text-and-styles*` modes
additionally require Updates, and the second also requires Numeric Slider.
`row-visibility` requires Updates and Row Visibility; `row-availability`
requires Updates and Row Availability.
The `custom-tab` mode additionally
requires the custom-tab capability on a supported game executable. The
`title-screen-text` mode requires the title-screen-text capability.
For matching before/after screenshots, press Right once on **Camera
Distance** to change `x0.6` to `x0.7`. Press Left once on **Impossible
Values** to change **hello** to `1s`.

An absent or unreadable mode safely falls back to `button`. If the INI or its
`mode` key is missing, the example attempts to create the default entry. An
unknown value is left untouched so a spelling mistake remains visible.

## Capture the four row-update images

For all four captures, use English, 2560x1440, default UI scale, default
runtime GFX patching, and no other client DLL. Hide the mouse cursor, wait two
seconds after the last interaction, and capture the complete game frame.

### Text and style

Set `mode=row-text-and-styles`, restart, and open **Text & Style Updates**.
Keep **Showcase Intensity (50%)** selected so its focused help is visible.
Move through the range first to verify that `0%` is red, `100%` is green, and
both the label and help update. Return to `50` and save:

```text
docs/assets/images/row-updates/colors-1.gif
```

### Text and style across controls

Set `mode=row-text-and-styles-2`, restart, and open **Text & Style Updates 2**.
Confirm that **Enemy Damage** is red below `x1.0`, native-colored at `x1.0`,
and green above it. Its semantic default is deliberately away from the visual
midpoint. Confirm that **Optional Effect** is gold while Off and native-colored
while On. Return them to `x0.8` and Off, select **Colorful Action** so its
styled help is visible, and save:

```text
docs/assets/images/row-updates/colors-2.gif
```

### Visibility

Set `mode=row-visibility`, restart, and open **Row Visibility**. Change
**Show advanced setting** from On to Off. Confirm **Advanced strength**
disappears and **Always visible** moves up without a page transition. Keep the
controlling toggle selected and save:

```text
docs/assets/images/row-updates/visibility.gif
```

### Availability

Set `mode=row-availability`, restart, and open **Row Availability**. Leave
**Enable online setting** Off. Confirm **Online strength** is dimmed and that
Down skips it in favor of **Always available**, then return focus to the
controlling toggle and save:

```text
docs/assets/images/row-updates/availability.gif
```

## Capture the title-screen text image

Set `mode=title-screen-text`, then launch Elden Ring and remain on its title
screen. For the canonical documentation image, disable
`TitleScreenTextExample.dll`, Solid Uncapper, and every other DLL or mod that
injects title-screen text. Load only ERNativeUI and this helper unless another
dependency is required to start the game. Capture the complete game frame as:

```text
docs/assets/images/title-screen/title-screen-corners.png
```

The four labels identify their requested corners and colors, so no interaction
is needed before taking the screenshot.

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
[Custom Configuration tabs guide](../../docs/guides/custom-tabs.md). For the
API 1.4 scenes, continue with the
[row-updates guides](../../docs/guides/row-updates/README.md).
