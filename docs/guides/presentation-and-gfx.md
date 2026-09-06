# Presentation and optional GFX assets

ERNativeUI's menu model and native hooks work without a loose GFX patch. The
two bundled GFX files are optional presentation assets: they add visual row
capacity and supply richer appearances for controls that Elden Ring's settings
movies do not define on their own.

## Runtime behavior versus presentation

Keep these responsibilities separate:

| Layer | Owned by | Purpose |
|---|---|---|
| Logical menu | Client mod through the public API | Pages, controls, text, callbacks, and initial values |
| Native integration | One `ERNativeUI.dll` | Game hooks, provider merge, pagination, values, input, and lifetime |
| GFX presentation | Optional loose assets | Row placements and named visual widgets used by the native layer |

`ERNativeUI.dll` does not parse, rewrite, or load these files itself during
startup. Mod Engine 2 makes a loose file replace the corresponding game asset;
the game then loads it normally. The host inspects live objects and retains
documented fallbacks when an optional widget is absent.

## What each asset changes

| File | Optional changes |
|---|---|
| `02_040_optionsetting.gfx` | Expands Game Options and Camera Options to 13 visual rows, adds the missing native-style left label for their shared button widget, and adds TextInput and ColorPicker presentations. |
| `02_042_pc_graphicsetting.gfx` | Adds the same TextInput and ColorPicker presentations to provider-owned Advanced Settings-style pages. Its existing 15 row placements are not expanded. |

Without the optional files:

- Game Options uses its native capacity and paginates sooner.
- Camera Options has no spare row on the tested native layout, so provider
  content cannot appear there until capacity is expanded.
- TextInput is still editable but uses the plain settings presentation.
- ColorPicker remains usable through an action-row fallback instead of the
  bracketed live swatch.
- Button activation still works even if a host movie lacks the additional
  left-label field.

The assets change presentation and capacity, not the public API or ownership
model.

## Install the bundled presentation

Keep the original relative paths beneath the directory registered with Mod
Engine 2:

```text
<ERNativeUI mod directory>/
`-- menu/
    `-- win/
        |-- 02_040_optionsetting.gfx
        `-- 02_042_pc_graphicsetting.gfx
```

Then include that directory in the existing mod-loader list:

```toml
[extension.mod_loader]
enabled = true
mods = [
    { enabled = true, name = "ERNativeUI", path = "mod\\ERNativeUI" },
]
```

The DLL remains an `external_dlls` entry; the mod-loader entry is only what
exposes the loose `menu/win` files. Restart the game after changing them.

To uninstall only the presentation patch, remove the two loose GFX files. Do
not modify or delete Elden Ring's packed original resources.

## Customize submenu titles

A provider-owned submenu has two title layers:

- `menu_title` is the surrounding owner heading;
- `page_title` is the logical title inside the settings page.

```cpp
auto advanced = menu.root().add_submenu(
    L"Advanced",
    L"Open advanced settings.");

advanced.set_presentation(
    L"My Mod",
    L"Advanced Settings");
```

An empty outer title uses the provider display name. If pagination creates
multiple physical slices, the default page title is `Advanced Settings (n/t)`.
The shared built-in first pages keep their game-owned titles; a provider cannot
win shared chrome through registration order.

## Customize a paginated title

Use a formatter only when the default `Title (n/t)` is unsuitable:

```cpp
ERUI_Result ERUI_CALL format_title(
    const erui::PageTitleFormatContext* context,
    std::uint16_t* output,
    std::uint32_t capacity,
    std::uint32_t* length) noexcept
{
    if (!context || context->size < sizeof(*context)) {
        return ERUI_INVALID_ARGUMENT;
    }

    try {
        const auto* data = reinterpret_cast<const wchar_t*>(
            context->base_title.data);
        std::wstring title(data, context->base_title.length);
        title += L" - page ";
        title += std::to_wstring(context->page_number);
        title += L"/";
        title += std::to_wstring(context->page_count);
        return erui::write_page_title(title, output, capacity, length);
    } catch (...) {
        return ERUI_OUT_OF_MEMORY;
    }
}

advanced.set_presentation<&format_title>(
    L"My Mod",
    L"Advanced Settings");
```

The formatter receives one-based `page_number` and `page_count`. It runs once
per physical slice during startup compilation, not when the player opens the
page. Its context and input view are borrowed for the call. The host copies
successful output into its compiled model.

Keep formatters deterministic, quick, and `noexcept`. Do not retain pointers,
re-enter registration, include embedded NUL characters, or write beyond the
advertised buffer. Failure, empty output, invalid UTF-16, and oversized output
all select the safe default rather than aborting menu publication.

## Reproduce a patch from extracted files

`ERNativeUIGfxPatcher.exe` always reads one input and writes a separate output.
It structurally validates the supported movie instead of relying on fixed
character IDs, and running the same transformation twice is byte-for-byte
idempotent.

Patch Game Options and Camera Options plus both optional control styles:

```powershell
ERNativeUIGfxPatcher.exe `
  --input 02_040_optionsetting.gfx `
  --output 02_040_optionsetting.patched.gfx `
  --game-options-rows 13 `
  --camera-options-rows 13 `
  --text-input-presentation character-name `
  --color-picker-presentation character-creation
```

Patch the Advanced Settings host:

```powershell
ERNativeUIGfxPatcher.exe `
  --input 02_042_pc_graphicsetting.gfx `
  --output 02_042_pc_graphicsetting.patched.gfx `
  --text-input-presentation character-name `
  --color-picker-presentation character-creation
```

Install each output under its original filename. The complete structural
contract, command-line behavior, idempotence rules, and known prototype
cleanup are documented in the
[GFX patcher README](../../tools/gfx_patcher/README.md).

Use files extracted from the game version being targeted. If an Elden Ring
update changes either movie, reproduce the transformation from that new
original and run the patcher's structural checks; do not blindly transplant a
patched file across unknown versions.

## Why the custom controls need added definitions

The generic settings item owns a `Widgets` container. Elden Ring selects a
named child such as `Button`, `TextInput`, `ComboBox`, or `Slider`. The patcher
adds `ColorPicker` as a real sibling, hidden by default, so native construction
can select it exactly like the existing widgets. It is not nested inside a
Button and does not rely on a permanent runtime cursor-hiding trick.

The TextInput patch adds a character-name-style frame and empty-state color to
the existing TextInput definition. The dedicated editor shown while typing is
already a separate native movie and is not modified.

The [GFX presentation case study](../research/case-studies/gfx-presentation.md)
records the display-tree evidence, failed forward-reference and Button-backed
prototypes, structural transformations, and reproduction procedure behind the
optional assets.
