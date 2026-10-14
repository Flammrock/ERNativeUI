# ERNativeUI host locales

ERNativeUI selects `locales/<effective identifier>.ini` once during startup.
With `[Localization] Language = auto`, that identifier is Steam's current game
language. A player may instead set an explicit lowercase identifier such as
`vietnamese`; the same value is then reported to every client mod. Files must
be UTF-8 (with or without a BOM).

Identifiers contain 1–64 ASCII lowercase letters, digits, `_`, or `-`.
Examples reported by Steam include `french`, `koreana`, `schinese`,
`brazilian`, and `latam`. Restart the game after changing the language setting
or a locale file.

```ini
[Pagination]
PreviousLabel = Previous Page
PreviousHelp = Return to the preceding settings page.
NextLabel = Next Page
NextHelp = Open the next settings page.
```

Default paginated titles use the language-neutral form `Title (n/t)` and do
not require a locale key. A provider's custom formatter may replace it.

Keys are optional. A missing file, missing/empty key, invalid UTF-8 value, file
larger than 64 KiB, or unsafe identifier falls back to the embedded English
text for the affected value. Unknown sections and keys are ignored.

These files translate only text owned by the ERNativeUI host. Each client mod
remains responsible for its own menu translations through the language API.
An explicit identifier does not translate Elden Ring's own interface.
