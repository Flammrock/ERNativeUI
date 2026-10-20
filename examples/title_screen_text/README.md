# Title-Screen Text Example

`TitleScreenTextExample.dll` is a focused API 1.3 client example. One provider
registers one line in each of the four title-screen corners:

- the top-left, top-right, and bottom-left lines inherit the native text color;
- the bottom-right line uses the explicit color `RGB(216, 167, 103)`.

Start with the mod-author
[Title-screen text guide](../../docs/guides/title-screen-text.md) for the API,
corner, color, localization, ordering, lifetime, and compatibility rules. This
README focuses on building and running the complete example DLL.

The calls also demonstrate that a mod may register more than one line. A
corner is mandatory on every call; ERNativeUI does not silently choose one.
Within each corner, lines from the same provider keep their call order and
provider blocks follow ERNativeUI's deterministic provider order. The example
does not add a settings row or install a game hook; it uses only ERNativeUI's
public C++17 wrapper.

The host copies each line during registration, so the client does not need to
retain its source string. Each line must be non-empty, contain no line break,
and fit within the public API limit. The process-wide limit is 16 committed
title-screen lines across all ERNativeUI clients.

The example checks `Capability::title_screen_text` before registration. If the
player has disabled runtime GFX patching in `ERNativeUI.ini`, the capability is
absent and the example exits without publishing a partial provider.

## Relevant source

The complete registration is intentionally small:

```cpp
void register_title_screen_text(erui::Menu& menu) {
    menu.add_title_screen_text(
        erui::TitleScreenCorner::top_left,
        {L"ERNativeUI top-left title text"});
    menu.add_title_screen_text(
        erui::TitleScreenCorner::top_right,
        {L"ERNativeUI top-right title text"});
    menu.add_title_screen_text(
        erui::TitleScreenCorner::bottom_left,
        {L"ERNativeUI bottom-left title text"});
    menu.add_title_screen_text(
        erui::TitleScreenCorner::bottom_right,
        {
            L"ERNativeUI bottom-right title text",
            erui::Color{216U, 167U, 103U},
        });
}
```

Omit `color` to follow Elden Ring's native title-screen text color. Set it to
an `erui::Color` when the line needs a specific opaque RGB color.

## Bottom-right DLC spacing

The bottom-right corner shares vertical space with Elden Ring's native title
information. When Steam reports Shadow of the Erdtree as installed,
ERNativeUI keeps the DLC label and the game's authored blank row before
`OFFLINE`. When the DLC is known to be absent, custom lines use normal line
spacing directly above `OFFLINE` only when the effective
`05_000_title.gfx` exactly matches the validated Elden Ring 2.7.1.0 / App
1.17.1 native profile. If the DLC state cannot be determined, or if a
structurally compatible title movie does not match that exact profile, the
host keeps the DLC row and authored gap as the safe fallback.

This placement does not edit or remove the native DLC field. Provider lines
may still be inserted into a structurally compatible modified title movie,
but its DLC row and gap remain reserved. If another mod's effective
`05_000_title.gfx` has an unrecognized or ambiguous title hierarchy,
ERNativeUI leaves that movie unchanged and omits the provider lines rather
than guessing at coordinates. See
[Runtime GFX completion and presentation](../../docs/guides/presentation-and-gfx.md#title-screen-text)
for the structural compatibility boundary and current live-validation status.

## Build inside the ERNativeUI tree

From the repository root:

```bat
cmake -S . -B build -A x64 -DERNATIVEUI_BUILD_EXAMPLES=ON
cmake --build build --config Release --target TitleScreenTextExample
```

The staged DLL is written to:

```text
build\deploy\Release\examples\TitleScreenTextExample.dll
```

## Build against an installed SDK

The standalone project deliberately has no package-version constraint because
the SDK package release and negotiated runtime API are separate versions. Its
headers must expose the API 1.3 title-screen text declarations.

```bat
cmake -S . -B build -A x64 -DCMAKE_PREFIX_PATH=C:\sdk\ERNativeUI
cmake --build build --config Release
```

## Load the example

Load the canonical host before the client DLL:

```toml
[modengine]
external_dlls = [
    "mod\\ERNativeUI\\ERNativeUI.dll",
    "mod\\ERNativeUI\\examples\\TitleScreenTextExample.dll",
]
```

After the game reaches the title screen, one example line appears in each
corner. If registration fails, the DLL reports the public SDK error through the Visual
Studio/DebugView output stream with the `[TitleScreenTextExample]` prefix.
