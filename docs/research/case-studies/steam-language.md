# Steam language discovery

Status: **Confirmed** on Windows with the
[reference build](../README.md#reference-build). This is a Steamworks service
case study, not executable-address research.

For client code and fallback examples, use the
[game language and localization guide](../../guides/localization.md).

## Question and scope

The research question was:

> How can a client learn Elden Ring's selected Steam language during startup
> without racing Steam initialization, discarding an unknown future token, or
> making menu registration depend on an arbitrary sleep?

The scope is the current game language reported by the already initialized
Steam client. It does not include translation, Windows locale inference,
Steam's available-language list, audio language, or changing language during
one process lifetime.

## This path has no game RVA or hook

- **Confirmed:** ERNativeUI does not scan or detour `eldenring.exe` for this
  feature.
- **Confirmed:** ERNativeUI does not hook a Steam function and does not call
  `SteamAPI_Init`. Elden Ring owns Steam initialization.
- **Confirmed:** The host resolves four ordinary flat-C exports from the
  already-loaded `steam_api64.dll` with `GetModuleHandleW` and
  `GetProcAddress`:

```text
SteamAPI_GetHSteamUser
SteamAPI_GetHSteamPipe
SteamAPI_SteamApps_v008
SteamAPI_ISteamApps_GetCurrentGameLanguage
```

The [Ghidra workflow](../tools/ghidra-workflow.md) and
[address map](../address-map/README.md) are therefore irrelevant to this
boundary. Revalidation concerns DLL exports, readiness, copying, and public
API sequencing—not AOBs or RVAs.

## Derivation

An early client queried language immediately when its DLL worker began. That
could run before Steam's process-wide context was ready, so localized examples
fell back to English even though a later host log reported French.

- **Rejected:** Sleeping four seconds in every client. It hides a startup race,
  adds latency, and still provides no readiness guarantee.
- **Rejected:** Letting the first language call double as an implicit client
  connection. Mods that do not localize should still have an explicit and
  uniform API 1.1 connection lifecycle.
- **Rejected:** Calling `SteamAPI_Init` from ERNativeUI. The game owns that
  process-wide service and its teardown.
- **Rejected:** Mapping from the Windows locale or returning English as if
  Steam had reported it. An unavailable query and an unknown valid token are
  distinct states.

The resulting host sequence is:

```text
API 1.0 compatibility registration opens
        |
host worker probes loaded Steam service every 25 ms
        |
        +-- valid exact token ----------> cache once; API 1.1 ready
        +-- retryable startup state ----> retry until bounded deadline
        `-- terminal failure/timeout ---> unavailable; API 1.1 still ready
```

Before acquiring `SteamApps008`, the probe requires positive Steam user and
pipe handles. A missing module, uninitialized handles/interface, or invalid
current token is retryable. Missing required exports and exceptions are
terminal. The configured `SteamLanguageWaitMs` deadline is 5,000 ms by
default and is bounded to 0–30,000 ms.

## Bounded live procedure

No executable instrumentation is required.

1. Enable ERNativeUI logging temporarily. Record the game executable identity,
   ERNativeUI build, Steam client state, and configured language wait.
2. Select French for Elden Ring in Steam, start the game normally, and call
   `erui::connect()` from an API 1.1 client worker after `DllMain` returns.
   Query language only after connection succeeds. Capture one readiness line
   and one public result; do not poll from the client.
3. Confirm the exact token, known enum, and localized provider text agree.
   Exit the process completely, select English in Steam, relaunch, and repeat.
4. For a startup control, log only state changes (or at most once per second)
   while the host probe is retryable. Confirm that a transient uninitialized
   state does not settle the cache.
5. Exercise failure decisions in unit tests rather than modifying or replacing
   `steam_api64.dll`: missing module, uninitialized Apps, invalid token,
   missing export, deadline, and a late result after settlement.
6. Disable the temporary log after the test; no hook or game-file cleanup is
   needed.

## Results

- **Confirmed:** With the game set to French, the live host reported
  `current='french' known=3`, and the localized examples selected French.
- **Confirmed:** After a complete restart with English selected, the game and
  examples selected English and otherwise behaved normally.
- **Confirmed:** The host accepts a nonempty token of at most 4,095 bytes; its
  bounded scan limit is 4,096. The public result never borrows Steam's
  original pointer.
- **Confirmed:** Fifteen currently advertised identifiers have convenience
  enum mappings. A valid unrecognized identifier returns
  `ERUI_GAME_LANGUAGE_UNKNOWN` while preserving the exact token.
- **Confirmed:** API 1.1 negotiation returns `ERUI_HOST_NOT_READY` while the
  language state is pending. A successful `erui::connect()` therefore means
  the state has settled to either ready or unavailable.
- **Confirmed:** Terminal unavailability does not fail the menu host. The
  language getter returns `ERUI_NOT_SUPPORTED`, allowing the client to choose
  its own English or other fallback.

The current known identifiers are `english`, `german`, `french`, `italian`,
`koreana`, `spanish`, `schinese`, `tchinese`, `russian`, `thai`, `japanese`,
`polish`, `arabic`, `brazilian`, and `latam`. These mappings are conveniences;
the copied identifier is the forward-compatible authority.

## Ownership, threading, and compatibility

The host worker alone performs the readiness probe. On first valid result or
terminal failure it settles one process-wide state under a mutex, wakes
waiters, and never mutates that state again.

- **Confirmed:** The host owns the copied token for process lifetime. In C,
  `ERUI_GameLanguageInfo.identifier` is a borrowed immutable view with that
  lifetime. The C++ wrapper copies it into `LanguageInfo`.
- **Confirmed:** Frozen API 1.0 clients keep their released startup behavior:
  their table can be obtained while language is pending, and their language
  getter waits on the same bounded host settlement.
- **Confirmed:** New API 1.1 clients use explicit connection. Their own
  connection timeout does not alter host state; a later connection may
  succeed.
- **Confirmed:** Host locale files and client localization are separate. The
  host may use its internal English fallback for ERNativeUI-owned labels, but
  the public getter still reports `ERUI_NOT_SUPPORTED` when Steam discovery is
  unavailable.

This is fail-closed: invalid or overlong text is not exposed, a transient
failure is not cached as a language, an unavailable service is not guessed,
and a late probe cannot overwrite an already published result.

## Limits and revalidation

- Only `GetCurrentGameLanguage` is queried. Available UI languages and audio
  language are intentionally not inferred.
- The cache is immutable for one process. Runtime language switching is not a
  supported contract; restart before validating another selection.
- The implementation currently depends on the named flat exports and
  `SteamApps008`. If Steam changes that interface, language discovery should
  become unavailable while the rest of ERNativeUI remains usable.
- Windows/Steam is the tested platform. This result does not establish another
  launcher or operating system boundary.
- A valid community or future identifier remains `unknown` until the enum is
  extended, but clients can match the exact token immediately.

## Current source anchors

- Export resolution, bounded copy, classification, and one-time cache:
  [`src/steam_language.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/src/steam_language.cpp)
- Internal readiness and failure model:
  [`src/steam_language.hpp`](https://github.com/Flammrock/ERNativeUI/blob/main/src/steam_language.hpp)
- Bounded startup probe and API 1.0/1.1 release ordering:
  [`src/host_dllmain.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/src/host_dllmain.cpp)
- Negotiation gate and public getter entry:
  [`src/host_api.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/src/host_api.cpp)
- Classification/readiness tests:
  [`tests/steam_language_test.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/tests/steam_language_test.cpp)
  and
  [`tests/steam_language_unavailable_test.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/tests/steam_language_unavailable_test.cpp)
- Frozen API 1.0 pending-readiness test:
  [`tests/abi/releases/v1_0/pending_readiness_lifecycle_test.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/tests/abi/releases/v1_0/pending_readiness_lifecycle_test.cpp)
- Normative public types and lifetime:
  [`include/ernativeui/erui.h`](https://github.com/Flammrock/ERNativeUI/blob/main/include/ernativeui/erui.h)

The host/client startup relationship is summarized in
[How ERNativeUI works](../../how-it-works.md), and the public ownership and
readiness rules are defined in
[Lifecycle, errors, and limits](../../reference/lifecycle-errors-and-limits.md).
