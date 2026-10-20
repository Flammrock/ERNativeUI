# Elden Ring 2.7.2.0 native-profile update

Status: **statically revalidated and live-validated in game**.

This record describes the ERNativeUI compatibility update for Elden Ring
product version `2.7.2.0` (App Ver. `1.17.2`). The public ERUI API did not need
to change.

## Compared executables

| Property | Elden Ring 2.7.1.0 | Elden Ring 2.7.2.0 |
|---|---:|---:|
| App version | `1.17.1` | `1.17.2` |
| File size | `87,042,128` bytes | `86,973,008` bytes |
| SHA-256 | `1A3547101327F65D0C76DA2F9190AC0AA66871EA42BAE2AECC61E11A8B597891` | `F9118FEA2CFD8FA3901E8060BFADB1552CB2471771DED8A5B662C4E298FFB328` |
| PE timestamp | `0x6A96B418` | `0x6AABF7E7` |
| Image size | `0x5E0DA00` | `0x5DFD600` |

The static comparison used a saved, complete 2.7.1.0 executable rather than
assuming that addresses moved by one common amount. The 2.7.2.0 image is
smaller, and the native boundaries did not share a safe blanket RVA delta.

## Porting method

The update followed the same fail-closed policy as the previous native-profile
port:

1. Identify the executable from its exact PE timestamp and image size.
2. Compare each required function, call target, data anchor, vtable, and local
   validator against the saved 2.7.1.0 executable.
3. Add an explicit 2.7.2.0 profile for every build-locked subsystem.
4. Preserve the 2.7.1.0 profile unchanged rather than replacing its RVAs.
5. Reject an unknown or hybrid identity instead of selecting the nearest known
   build.

This produced separate per-build mappings for the ordinary menu runtime,
TextInput, ColorPicker, Input Bindings, the intercepted GFX loader, and the
custom-tab runtime. The custom-tab carrier also has a 2.7.2.0-specific set of
validated allocator call sites. No subsystem relies on a global address
adjustment between 2.7.1.0 and 2.7.2.0.

## Validation

The Release build completed successfully and the automated suite passed
`87/87` tests. Live validation on 2.7.2.0 then covered:

- ordinary settings tabs, rows, Graphics, wrapping, mouse selection, Back,
  close, and reopen behavior;
- title-screen text and the runtime GFX interception path; and
- the 50-custom-tab stress client, including icons, navigation, wrapping,
  mouse selection, teardown, and menu reopen.

The validation log contained no `WARN`, `ERROR`, or `FATAL` entries. These
results establish support for the exact 2.7.2.0 identity recorded above while
retaining the existing 2.7.1.0 path.

Solid Uncapper was **not** part of the 2.7.2.0 live-validation matrix. Its
historical 2.7.1.0 compatibility evidence remains recorded in the
[2.7.1.0 update](elden-ring-2.7.1.0.md), but that result must not be inferred
for the new executable.
