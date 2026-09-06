# Contributing to ERNativeUI

ERNativeUI welcomes bug fixes, compatibility updates, documentation,
examples, and carefully validated native-interface research.

Use the repository's normal Windows preset before submitting a change:

```powershell
cmake --preset windows-release
cmake --build --preset windows-release
ctest --preset windows-release
```

The repository [`build.bat`](https://github.com/Flammrock/ERNativeUI/blob/main/build.bat) runs the same configure, build, and test
sequence with a clean-first build, then refreshes `dist/ERNativeUI` after the
tests pass. Keep commit subjects in
[Conventional Commit](https://www.conventionalcommits.org/) form because
Release Please derives the next project release and changelog from them.

Release operators should follow the verified [release procedure](releasing.md).
That guide covers Release Please, the Windows GitHub package, and the separate
local Nexus package.

For native research contributions, begin with the
[research methodology](../research/methodology.md). Public API behavior and
compatibility claims should be backed by automated tests where possible and by
recorded in-game evidence where the game is required.

## Documentation images

Keep every documentation image below `docs/assets/images/`. Group images by
their subject without reproducing the entire documentation tree. For example,
control screenshots belong in `docs/assets/images/controls/<control>/`, dialog
screenshots in `docs/assets/images/dialogs/`, localization screenshots in
`docs/assets/images/localization/`, and menu screenshots in
`docs/assets/images/menus/`. Use descriptive lowercase kebab-case names and
relative Markdown links so the same page works on GitHub and in the installed
SDK.

Every published image needs useful alt text and a short reader-facing caption.
While an image is still missing, leave an HTML `SCREENSHOT TODO` comment in
its intended position instead of adding a broken Markdown image. Record the
exact language, resolution, UI scale, client and GFX setup, navigation path,
selected row and value, cursor state, transition delay, crop, target filename,
alt text, and caption. Remove that capture comment after the final image is
added. Use the existing control screenshots and their corresponding
`showcase_screenshot_helper` modes as composition references.
