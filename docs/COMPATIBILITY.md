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

The reference native analysis covers Windows `eldenring.exe` product version
`2.7.0.0`, SHA-256
`D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134`,
PE timestamp `0x69E9C9B9`, and image size `0x5E09600`. Release notes remain the
authority for the exact game builds supported by a packaged host.

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

ERNativeUI was live-tested with Solid Uncapper 2.3 on the Elden Ring 2.7.0.0
reference executable. List Solid Uncapper first:

```toml
external_dlls = [
    "mod\\Solid Uncapper\\Solid Uncapper.dll",
    "mod\\ERNativeUI\\ERNativeUI.dll",
    "mod\\ERNativeUI\\examples\\TarnishedUIShowcase.dll"
]
```

Both DLLs complete initialization on workers, so list order alone cannot prove
which detour is installed first. ERNativeUI captures the pristine native
entries early, waits up to 20 seconds for Solid Uncapper's shared detours to
appear and stabilize, verifies that each supported absolute-indirect detour
targets executable code owned by `Solid Uncapper.dll`, then installs itself as
the next cooperative layer:

```text
Elden Ring caller
`-- ERNativeUI detour
    `-- Solid Uncapper detour
        `-- original Elden Ring function
```

The three shared entries are the Game Options root materializer, provider
subpage materializer, and native UI text resolver. ERNativeUI rejects partial,
foreign, changing, or unsupported detours rather than guessing. If Solid
Uncapper has its menu disabled and installs none of the three, ERNativeUI uses
the captured native entries normally.

The exercised matrix covered both mods' root and child pages, values, repeated
cross-navigation, ERNativeUI pagination and dialogs, persistence after restart,
and ERNativeUI again without Solid Uncapper. API 1.1 later added independent
input-binding and text-editor hooks; that complete newer feature set has not
yet been separately certified in combination with Solid Uncapper 2.3. Report
the distinction if a problem involves those features.

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
