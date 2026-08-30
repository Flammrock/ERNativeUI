# ERNativeUI host locales

ERNativeUI selects `locales/<Steam identifier>.ini` once during startup. Files
must be UTF-8 (with or without a BOM). Their basename is the exact identifier
reported by Steam, such as `french`, `koreana`, `schinese`, `brazilian`, or
`latam`.

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
text for the affected value. Unknown sections and keys are ignored. Restart
the game after editing a locale.

These files translate only text owned by the ERNativeUI host. Each client mod
remains responsible for its own menu translations through the language API.
