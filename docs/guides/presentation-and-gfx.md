# Runtime GFX completion and presentation

ERNativeUI completes Elden Ring's supported settings and title movies in
memory while the game loads them. A normal installation needs only the host
DLL: players do not need to install loose `menu/win/*.gfx` files for expanded
rows, TextInput, ColorPicker, or API 1.3 title-screen text presentation.

This happens after the game or mod loader selects the effective GFX file and
before Scaleform consumes it. The input may therefore be the vanilla movie or
a structurally compatible movie supplied by another mod.

## What the host completes

For `02_040_optionsetting.gfx`, the host ensures these visual capacities:

| Settings page | Completed capacity |
|---|---:|
| Game Options | 13 rows |
| Camera Options | 13 rows |
| Display | 13 rows |
| Sound | 13 rows |
| Network | 10 rows |
| Keyboard/Mouse Settings | 13 rows |
| Graphics | 13 rows |

Network deliberately stops at ten rows because the game's conditional
restriction warning occupies the lower part of that panel. ERNativeUI uses
ordinary pagination when more content is needed. Controller Settings is a
specialized input-binding screen and is not changed into an ordinary settings
page.

The runtime transformation also:

- supplies the native-style left label required by affected action rows;
- completes the character-name-style TextInput presentation; and
- adds the standalone ColorPicker presentation used for the bracketed live
  color swatch.

`02_042_pc_graphicsetting.gfx` already contains 15 Advanced Settings row
placements. ERNativeUI leaves that capacity intact and completes its
TextInput and ColorPicker presentation when necessary.

These changes affect presentation and physical capacity only. Client mods
still declare logical controls and pages through the public API, and the host
still owns merging, callbacks, values, and pagination.

### Title-screen text

For `05_000_title.gfx`, the host places registered API 1.3 text in the
explicitly selected top-left, top-right, bottom-left, or bottom-right corner.
It derives the stage bounds, native text metrics, transforms, alignment, and
safe corner insets from the effective movie instead of assuming a fixed screen
resolution.

The bottom-right corner shares space with Elden Ring's native DLC, online
state, application-version, and calibration-version fields. ERNativeUI checks
whether Steam reports Shadow of the Erdtree as currently installed before it
builds the immutable title-text plan:

- when installed, the native DLC line and its authored blank row remain
  reserved;
- when absent and the effective `05_000_title.gfx` exactly matches the
  validated Elden Ring 2.7.1.0 / App 1.17.1 native profile, the empty
  `DLCVersion_0` placement and its authored blank row are excluded from
  collision occupancy, so custom lines sit at normal line spacing directly
  above `OFFLINE`;
- when the effective title movie is structurally compatible but does not
  match that exact native profile, ERNativeUI preserves the DLC row and gap;
  and
- when the state cannot be determined, ERNativeUI reserves the DLC line and
  gap rather than risk covering native text.

The native `DLCVersion_0` field is never removed or rewritten. The decision
only changes which empty native geometry is available while placing copied
provider lines.

On Elden Ring 2.7.1.0, two live observations establish the underlying layout:

| Installed content | Observed native title layout |
|---|---|
| Shadow of the Erdtree absent, earlier run | `DLCVersion_0` was empty; retaining its placement and authored spacer produced a visible extra gap above `OFFLINE`. |
| Shadow of the Erdtree present, current run | `Shadow of the Erdtree` occupied the DLC field, followed by the authored blank row and then `OFFLINE`; preserving that gap produced the intended layout. |

The adaptive installed/absent branch is covered by structural and unit tests,
but its final same-build live A/B validation with the Steam DLC checkbox
disabled and re-enabled is still pending. The two observations above should
not be reported as that final adaptive-branch validation.

## Modified GFX compatibility

ERNativeUI does not assume that the loaded movie is byte-for-byte vanilla. It
parses the effective file, identifies supported panels and widgets by their
structure, and applies only missing transformations. A compatible movie that
already contains some or all of the additions is accepted; a complete movie
is left unchanged.

The transformation is deliberately conservative:

- only the exact supported settings- and title-movie paths are considered;
- required hosts, named children, row sequences, and placement contracts must
  be recognized;
- unrelated definitions and authored data are preserved; and
- output is serialized and parsed again before it can be published to
  Scaleform.

If a modified movie is malformed or its relevant layout is not recognized,
ERNativeUI keeps the effective input bytes unchanged instead of guessing. Enable
`ERNativeUI.log` when diagnosing such a conflict; the log records whether each
target movie was transformed, already complete, or rejected.

For a modified title movie, the transformer additionally requires an
unambiguous title information hierarchy, usable stage geometry, positive
axis-aligned placement transforms, and non-overlapping corner regions. Existing
compatible direct text fields are preserved and participate in collision
checks. Even when that structure is compatible, DLC-absent gap reuse is
limited to the exact validated Elden Ring 2.7.1.0 / App 1.17.1 native profile;
other movies preserve the DLC row and authored gap. If any required
relationship is ambiguous, no provider title text is inserted and the
original movie continues unchanged. The fallback cannot make an unrelated
custom title hierarchy compatible, and it never guesses around unknown
authored animation or transforms.

Because Mod Engine 2 still chooses one effective file for each game path, two
mods cannot independently merge arbitrary binary GFX replacements through
load order. ERNativeUI can complete a structurally compatible winning movie,
but it cannot make unrelated or mutually destructive edits compatible.

Restart the game after changing any loose GFX file. Scaleform may cache a
movie for the lifetime of the process.

## Runtime layers

| Layer | Owned by | Purpose |
|---|---|---|
| Logical menu | Client mod through the public API | Pages, controls, text, callbacks, and initial values |
| Native integration | One `ERNativeUI.dll` | Game hooks, provider merge, pagination, values, input, and lifetime |
| Runtime GFX completion | `ERNativeUI.dll` at the validated file-opening boundary | Missing row placements, supported widget presentation, and registered title-screen text |
| Optional replacement GFX | Another asset mod or a developer's experiment | Additional presentation changes outside ERNativeUI's normal runtime additions |

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

## Offline transformer for contributors

`ERNativeUIGfxPatcher.exe` exposes the same structural transformation engine
as an offline analysis and reproducibility tool. It is useful for inspecting
extracted movies, reviewing a transformation, and maintaining tests. It is
not a player installation step and its output is not required for normal
ERNativeUI use.

The complete command-line and structural reference remains in the
[GFX patcher README](../../tools/gfx_patcher/README.md).

The [GFX presentation case study](../research/case-studies/gfx-presentation.md)
records the display-tree evidence, failed prototypes, structural contracts,
and reproduction work behind the runtime transformation.
