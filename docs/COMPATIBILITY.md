# Game and mod compatibility

ERNativeUI is a Windows x64, offline Elden Ring mod. Within one host
release-major line, each earlier ERUI API from that same major remains
available. A new host major retains APIs from the previous major only when its
release documentation says so. The host's private native integration remains
sensitive to the running game executable and to other DLLs that detour the
same functions. See [Versioning](versioning.md#same-major-host-compatibility-policy).

This page states tested combinations precisely. An untested combination is not
called compatible or incompatible merely because its features look unrelated.

## Required environment

- Run with Easy Anti-Cheat disabled through a compatible DLL/mod loader such
  as Mod Engine 2.
- Install one canonical `ERNativeUI.dll`; client mods should depend on it
  rather than bundle private copies.
- Use an ERNativeUI release that explicitly supports the installed Elden Ring
  executable.
- Keep the host loaded for the complete process lifetime. Hot-unloading the
  host or a committed client is unsupported.

The completed analysis and live regression matrix cover Windows
`eldenring.exe` product versions `2.7.0.0` and `2.7.1.0`. Release notes remain
the authority for the exact game builds supported by a packaged host.

## Elden Ring executable support

The ERUI API version and the Elden Ring executable version are independent.
A game compatibility patch can therefore keep API 1.1 unchanged.

| ERNativeUI host | Public APIs | Elden Ring 2.7.0.0 | Elden Ring 2.7.1.0 |
|---|---|---|---|
| `1.0.0` | API 1.0 | Supported | No declared support; use the current host |
| `1.1.0` | APIs 1.0 and 1.1 | Supported | No declared support; use the current host |
| `1.1.1` | APIs 1.0 and 1.1 | Live-validated | Live-validated |

Existing client DLLs compiled for ERUI API 1.0 do not need to be rebuilt for
the 1.1.1 host; players update the single host DLL.

The two exact runtime identities are:

| Game version | SHA-256 | PE timestamp | Image size |
|---|---|---:|---:|
| `2.7.0.0` | `D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134` | `0x69E9C9B9` | `0x5E09600` |
| `2.7.1.0` | `1A3547101327F65D0C76DA2F9190AC0AA66871EA42BAE2AECC61E11A8B597891` | `0x6A96B418` | `0x5E0DA00` |

The host identifies the loaded image from the PE timestamp **and** image size,
selects one private address profile, then validates every build-locked entry,
vtable, and data anchor before use. A timestamp-only or size-only match is not
accepted. Unknown future builds continue to fail closed for features that
depend on exact native layouts.

The [2.7.1.0 update record](research/game-updates/elden-ring-2.7.1.0.md)
documents what moved and how the new profile was derived.

## Several ERNativeUI client mods

This is the primary supported use case. Load the host before ordinary clients:

```toml
[modengine]
external_dlls = [
    "mod\\ERNativeUI\\ERNativeUI.dll",
    "mod\\ClientOne\\ClientOne.dll",
    "mod\\ClientTwo\\ClientTwo.dll"
]
```

The host accepts each unique provider, orders and merges their declarations,
and installs one coordinated hook set. Clients must use distinct stable
provider IDs. Two copies of the host or the same client listed twice are not a
supported substitute for multiple providers.

## Solid Uncapper

ERNativeUI was live-tested with Solid Uncapper 2.3, the latest version at test
time, on Elden Ring executables 2.7.0.0 and 2.7.1.0. List Solid Uncapper first:

```toml
external_dlls = [
    "mod\\Solid Uncapper\\Solid Uncapper.dll",
    "mod\\ERNativeUI\\ERNativeUI.dll",
    "mod\\ERNativeUI\\examples\\TarnishedUIShowcase.dll"
]
```

Both DLLs complete initialization on workers, so list order alone cannot prove
which detour is installed first. ERNativeUI captures the pristine native
entries early, waits up to 20 seconds for Solid Uncapper's shared menu detours
to appear and stabilize, verifies the accepted ownership and entry shapes,
then installs itself as the next cooperative layer:

```text
Elden Ring caller
`-- ERNativeUI detour
    `-- Solid Uncapper detour
        `-- original Elden Ring function
```

The three shared menu entries are the Game Options root materializer, provider
subpage materializer, and native UI text resolver. ERNativeUI rejects partial,
foreign, changing, or unsupported detours rather than guessing. If Solid
Uncapper has its menu disabled and installs none of the three, ERNativeUI uses
the captured native entries normally.

The 2.7.1.0 candidate test exposed two additional overlaps in the API 1.1
Input Bindings backend. Solid Uncapper detours `clear_key_setting` and
`write_binding_value` after ERNativeUI has loaded. A late pristine-byte check
therefore rejected an otherwise correct 2.7.1.0 profile and aborted the atomic
ERNativeUI hook install; the visible result was Solid Uncapper's row without
any ERNativeUI rows.

The 1.1.1 host patch addresses that race deliberately. It captures the complete
Input Bindings interface set before Solid Uncapper's asynchronous installer
can change it. After Solid Uncapper has initialized, every input entry is
checked again. All entries except the two known overlaps must remain pristine.
Each known overlap may also be an `FF 25` absolute-indirect detour, but only
when its resolved destination is executable memory owned by
`Solid Uncapper.dll`. ERNativeUI then hooks or calls the captured entry so the
validated Solid Uncapper layer remains in the chain. A foreign detour, a
different entry shape, or any modification to another input interface still
fails closed.

The next candidate reached native Input Bindings preparation but exposed one
more independent overlap while installing ColorPicker. Solid Uncapper also
detours the Scaleform visibility setter at game RVA `0x734190`, which
ERNativeUI calls when preflighting and presenting the standalone color swatch.
This is not a 2.7.1.0 relocation: the same RVA and pristine entry pattern are
present in both supported game profiles.

ERNativeUI now captures that one pristine entry before Solid Uncapper's worker
can modify it. After the existing stabilization wait, the entry must either
remain byte-for-byte unchanged or be an `FF 25` absolute-indirect detour into
executable memory owned by `Solid Uncapper.dll`. The accepted entry is checked
again immediately before the ColorPicker backend is published. ERNativeUI
continues to call the game entry, preserving the validated chain through Solid
Uncapper to the original setter; it does not copy, bypass, or call the foreign
detour target directly. Every other ColorPicker boundary retains its ordinary
exact-profile validation.

The compatibility regression succeeded on 2.7.0.0 and 2.7.1.0, both with
Solid Uncapper 2.3 loaded first and without it. The recognized shared menu,
Input Bindings, and ColorPicker chains remained operational when Solid
Uncapper was present, while the same entries followed their pristine paths
when it was absent.

The same patch also accounts for Solid Uncapper's already-materialized
Game Options row when selecting the root pagination plan. With the vanilla
six-row movie, four game rows plus Solid Uncapper leave one slot: an
overflowing ERNativeUI menu places only Next there. With the optional 13-row
movie, the same five existing rows leave eight slots. This prevents a provider
or navigation row from being appended beyond the movie's actual placements.
Both the original six-row layout and optional 13-row layout were
live-validated with and without Solid Uncapper.

Report the exact game and Solid Uncapper versions when describing a
compatibility result.

## Optional loose GFX and other asset mods

ERNativeUI's two `menu/win/*.gfx` files are optional presentation assets. Mod
Engine 2 can expose only one final loose replacement for a given game path, so
another mod shipping the same GFX may override ERNativeUI's capacity or custom
widget presentation depending on the configured directories.

The DLL still uses live native capacity and documented control fallbacks when
the ERNativeUI asset is absent. To combine two edits to the same movie, start
from the same game-version original and reproduce both structural changes in
one reviewed output; do not rely on load order to merge binary GFX files. See
[Presentation and optional GFX](guides/presentation-and-gfx.md).

## Seamless Co-op

ERNativeUI was live-tested alongside Seamless Co-op 2.0.1, the latest release
at test time, on the Elden Ring 2.7.0.0 reference executable. The tested setup
behaved normally and no incompatibility was observed. No ERNativeUI
compatibility patch was required for this combination.

This result applies to those exact versions. Re-test after Elden Ring,
ERNativeUI, or Seamless Co-op updates rather than inferring compatibility from
an older matrix.

## Shadow of the Erdtree

The project was developed without a separate live DLC compatibility run.
Owning Shadow of the Erdtree normally uses the same executable line, but that
is not sufficient evidence by itself: optional resources, menu states, or a
later executable can differ. Use a release that names the running game build
and report DLC ownership when describing a reproducible issue.

## After an Elden Ring update

An unchanged public ERUI API does not make old native addresses safe. The host
requires unique patterns, validates fallbacks and object identities, and fails
closed when a required boundary cannot be established. Do not disable those
checks or paste a nearby RVA from an older log.

For a useful report:

1. reproduce with ERNativeUI and one minimal client;
2. add other DLLs back one at a time;
3. enable logging and diagnostics only for the bounded reproduction;
4. include the ERNativeUI release, client versions, DLL order, game product
   version, PE timestamp and image size, and the first failed address/hook
   line; and
5. state the exact page, control, input device, and action sequence.

The [common-errors guide](getting-started/common-errors.md) explains the safe
startup failures. Native contributors should follow the
[game-update research procedure](research/methodology.md#8-maintain-the-map-after-a-game-update)
instead of treating address proximity as evidence.
