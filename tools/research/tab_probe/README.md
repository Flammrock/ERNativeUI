# Configuration tab research probe

`ERNativeUITabProbe.dll` is a temporary, observation-only companion used while
researching custom top-level Configuration tabs. It is not part of the public
ERNativeUI runtime or API.

The probe supports the validated Elden Ring `2.7.0.0` and `2.7.1.0`
executables. It records:

- each native `MenuOptionCategory` append;
- the final category order and presentation descriptor for each Configuration
  instance;
- selected-index and active-panel changes through the native tab dispatcher;
- indices delivered by the native `BackTabList` callback.

It never changes a category record, selector, panel, Scaleform object, or
input value. Unknown executable identities and already-modified hook entries
fail closed.

The full probe detours the native category-append and tab-dispatch entries.
Do not load it beside an ERNativeUI research host that validates or hooks
those same entries. In particular, it prevents NativeTweaks' private host
from enabling its dedicated Mod Settings tab. Use the private host's own
diagnostic log for that comparison.

Build it explicitly; normal builds leave it disabled:

```powershell
cmake --preset windows-release -DERNATIVEUI_BUILD_TAB_RESEARCH_PROBE=ON
cmake --build --preset windows-release --target ERNativeUITabProbe
```

Load only one `ERNativeUI.dll` host, then load the probe before that host. The
probe writes `ERNativeUITabProbe.log` beside its DLL. Do not ship the probe to
players.

## Duplicate-category mutation probe

`ERNativeUITabDuplicateProbe.dll` is a separate, deliberately mutating probe
for the NativeTweaks private host. It waits until that host owns the native
category-append and tab-dispatch entries, then hooks only the top-dialog
constructor. On a completed, exactly validated nine-record list with categories
`0,1,2,3,5,6,7,8,4`, it changes record 7 from category 8 to category 4.

This temporarily replaces the Graphics page with a second route to the same
native category used by NativeTweaks. The Graphics and NativeTweaks tab
presentations remain distinct. No mutation is attempted for any other record
order, count, vtable, executable, or host state.

Do not load `ERNativeUITabProbe.dll` for this experiment. Build the narrow probe
explicitly:

```powershell
cmake --preset windows-release `
    -DERNATIVEUI_BUILD_TAB_DUPLICATE_RESEARCH_PROBE=ON
cmake --build --preset windows-release `
    --target ERNativeUITabDuplicateProbe
```

The probe writes `ERNativeUITabDuplicateProbe.log` beside its DLL. It is a
research artifact and must never be shipped to players.

## Independent client-owned tab-carrier proof

The current research build now runs the [fifty-page reusable-carrier
experiment](REUSABLE_CARRIER.md). The two-carrier description below records
the earlier bounded proof and its evidence; use the linked build and live
acceptance steps for current artifacts.

This final bounded experiment combines the proven viewport with two distinct
pages registered by a separate API 1.2 client DLL. It deliberately does
**not** define or freeze a public API 1.3 surface.

With `ERNATIVEUI_BUILD_TAB_CONTENT_RESEARCH_PROBE=ON`, the research-only host
copies exactly two disabled submenu transports from provider
`ernativeui-tab-content-probe` into its internal tab model. Client Tab A uses
the native category-4 cache bank and stock selector-6 factory; Client Tab B
uses the independent category-9 cache bank and stock selector-14 factory.
Both factory wrappers, callback locals, selector bytes, and the composite
teardown entry are validated byte-for-byte before any hook is enabled.

Each page contains exactly one stateful row: `A: Independent Toggle` and
`B: Independent Strength`. Before insertion, the host validates the native
OptionSettingDialog, its live capacity, its SceneObjProxy vtable/getter, the
managed wrapper, and payload. Once the second bank exists, its panel, wrapper,
and payload must all differ from the first bank before its row is mutated.
The ordinary native composite owns both cache entries and retires them; an
observation-only teardown detour verifies that both slots were cleared after
the original destructor returned.

This proves two independent native carriers. It does **not** prove arbitrary
carrier count: categories 4 and 9 can be revisited and alternated indefinitely
because their wrappers are cached, but they provide only two simultaneously
independent banks. Replacing or reconstructing either live cache entry would
require a separately proven mid-lifetime retirement protocol. Until then,
more than two logical pages must not be presented as independently owned by
this backend.

Static analysis of the exact Elden Ring 2.7.1.0 executable established the
ownership boundary used by this probe:

- the composite constructor at RVA `0x93C090` initializes ten cache pointers
  at `+0x68` and the current-panel pointer at `+0xB8`;
- switch routine `0x93C900` resolves an empty bank through dispatcher
  `0x93D730`, stores the returned panel in that bank, then activates it;
- category 4's wrapper at `0x93CA90` passes selector 6 and materializer
  `0x95D200` to common constructor `0x95FD30`;
- category 9's wrapper at `0x93CC50` independently passes selector 14 and
  materializer `0x959AB0` to the same constructor;
- composite teardown at `0x93C1A0` walks all ten banks, asks each panel to
  deactivate, queues it through the native retirement manager, nulls the
  cache slot, destroys its erased callback, and finally destroys the embedded
  SceneObjProxy.

Accordingly, the probe swaps only the two stack-local materializer callbacks.
It never allocates, fabricates, replaces, releases, or manually destroys a
panel wrapper.

Build the three research artifacts explicitly:

```powershell
cmake --preset windows-release `
    -DERNATIVEUI_BUILD_TAB_RESEARCH_PROBE=OFF `
    -DERNATIVEUI_BUILD_TAB_DUPLICATE_RESEARCH_PROBE=OFF `
    -DERNATIVEUI_BUILD_TAB_PRESENTATION_RESEARCH_PROBE=OFF `
    -DERNATIVEUI_BUILD_TAB_VIEWPORT_RESEARCH_PROBE=ON `
    -DERNATIVEUI_BUILD_TAB_CONTENT_RESEARCH_PROBE=ON
cmake --build --preset windows-release --target `
    ERNativeUIHost `
    ERNativeUITabContentClient `
    ERNativeUITabViewportProbe `
    ERNativeUITabViewportTests
```

Do not copy the build automatically. For a runtime proof, manually copy only
these three newly built DLLs into isolated mod folders and load them in this
order:

```toml
[modengine]
external_dlls = [
    "mod\\DemoConfigMenu\\ERNativeUI.dll",
    "mod\\ERNativeUITabContentClient\\ERNativeUITabContentClient.dll",
    "mod\\ERNativeUITabViewportProbe\\ERNativeUITabViewportProbe.dll",
]
```

After a full game restart:

1. Cold-open System on Game Options. Verify no `[Probe transport]` row appears.
2. Navigate to **[Probe] Client Tab A**. It must contain only
   `A: Independent Toggle`. Change it from Off to On.
3. Navigate to **[Probe] Client Tab B**. It must contain only
   `B: Independent Strength`; it must not show A's row. Change its value from
   65 to a visibly different value.
4. Alternate A/B several times. Each page must retain its own row and value;
   neither page may become empty or inherit the other page's state.
5. Traverse both directions, wrap through Game Options, and use mouse hover
   including ellipsis dwell. Icons, titles, content, and controller repeat
   behavior must remain unchanged.
6. Close and reopen System twice. Repeat steps 2-5 and verify fresh carrier
   construction without stale rows or state crossover.

A successful host log has two `Research tab carrier construct` records. The
second constructed bank must report `exactPair=1`; both must report `logicalRows=1`,
`contentRows=1`, `attempted=1`, `added=1`, `identityStable=1`, `committed=1`,
and `nativeFallback=0`. The viewport log must contain `carrierSelection`
records where category 4 selects cache 4, category 9 selects cache 9, and
`distinct={panel:1 wrapper:1 payload:1 exact:1}`. Closing System must emit a
`Research tab carrier retirement` record with `nativeCleared ... exact:1`.
Any route fault, alias, fallback, missing row, wrong row, state crossover,
empty panel, or uncleared cache fails the proof.

## Presentation-swap mutation probe

`ERNativeUITabPresentationProbe.dll` tests the other half of a future tab
viewport: changing a physical tab's title and icon without changing its page
route. It waits for the validated NativeTweaks private host, then hooks the
native function that presents each `BackTabList` cell. On the exact nine-tab
layout, immediately before the cells are first rendered, it redirects each
record's non-owning display-title pointer to a process-lifetime static label and
swaps the 64-byte presentation payloads of Graphics and NativeTweaks while
leaving their category IDs unchanged. The native owned fallback strings remain
untouched.

The expected result is intentionally crossed:

- the tab presented as **NativeTweaks** opens the real Graphics page;
- the tab presented as **Graphics** opens the NativeTweaks page.

This proves presentation records can be remapped independently from the native
category used for panel routing. The probe DLL is pinned before its static
UTF-16 labels are published, so those borrowed display pointers remain valid.

The runtime result also established an important icon limitation: the stock
Graphics descriptor moves correctly, but NativeTweaks' custom descriptor does
not render in an ordinary physical tab cell. Its token `3` is backed by artwork
embedded in NativeTweaks' specialized ninth GFX cell. A general viewport must
therefore use a portable stock descriptor until custom icon assets have their
own loading and presentation path.

Do not combine this probe with either earlier tab probe. Build it explicitly:

```powershell
cmake --preset windows-release `
    -DERNATIVEUI_BUILD_TAB_PRESENTATION_RESEARCH_PROBE=ON
cmake --build --preset windows-release `
    --target ERNativeUITabPresentationProbe
```

The probe writes `ERNativeUITabPresentationProbe.log` beside its DLL. It is a
deliberately mutating research artifact and must never be shipped to players.

## Viewport mutation probe

`ERNativeUITabViewportProbe.dll` previously proved 48 logical tabs on Elden
Ring's eight homogeneous stock tab cells. The final RGBA validation gate uses
the eight stock tabs plus four independently identified custom tabs. Each
custom tab clones the stock Graphics category while presenting its own 32x32
immutable RGBA8 image through the proven runtime-Shape path.

When tabs are hidden beyond either edge, the probe replaces only that edge
slot's icon with a host-owned three-dot indicator. The slot still maps to its
real logical tab, title, category, and page. Selecting the edge shifts the
viewport and reveals the real icon; the dots are never a fake tab. A selected
real tab is never masked. IDs `90001`, `90003`, `90004`, and `90005` address
the four indexed RGBA resources; `90002` remains exclusively reserved for the
vector ellipsis. The probe does not depend on a ninth GFX cell or on
NativeTweaks' private ERNativeUI host.
Every raster uses the same proven Graphics-icon canvas and parent-local
position. This keeps scale and alignment stable even when a direct reverse
wrap exposes the different authored bounds of the Network, Controller, or
Keyboard/Mouse cells. The three-dot indicator retains the corresponding edge
cell's native bounds.
The dynamic Shape overlay accepts only the two exact 2.7.1 authored-icon display
vtable RVAs observed across the eight stock cells. Network and Keyboard/Mouse
use the second form until another presentation rebuilds their artwork; both
forms still require the same validated AS3 interface, parent, movie root,
value flags, geometry, property verification, cleanup, and rollback checks.

With navigation margin 1, moving right into a logical tab beyond the
second-to-last visible position shifts the logical window while keeping the
selection anchored at that physical position. The reverse journey shifts at
the second cell. A remap updates the eight record titles, categories, and
presentation descriptors, resolves and repaints all eight cells in both
coincident native layers (`TabList` and `BackTabList`), relocates the native
selector at the composite-frame boundary, and corrects the active page before
it receives input. Repainting both layers is required: leaving either layer on
the previous logical window produces two different icons at the same physical
position.

An earlier experiment used NativeTweaks' specialized ninth GFX cell. Repainting
that cell with a stock descriptor, including an explicit transparent-frame
pass, did not erase its slot-local custom artwork. The production fallback
must therefore restrict itself to the eight homogeneous stock cells. More
physical cells require ERNativeUI-owned, validated homogeneous GFX instead of
adapting a third-party specialized cell.

### Failed single-pixel mutation gate and current safe baseline

The image-binding callback records only the live menu epoch, layer, and
physical cell when it first presents the custom cross. It does not retain a raw
ActionScript wrapper or Movie pointer. On a later composite frame, the probe
reacquires that exact live `ItemIcon` parent and derives and validates its
current Movie. Earlier pre-composite runs proved that a transparent 2x2
`flash.display.BitmapData` can be constructed, but both immediate release and
process-lifetime wrapper retention turned the renderer black. Final-reference
destruction was therefore ruled out. Moving the same constructor-only retained
probe until after Elden Ring's native Configuration-panel frame virtual
returned remained stable, proving that the execution phase was significant.

The preceding gate advanced only the object-ownership lifecycle. At the same
validated post-native-composite boundary it constructed one transparent 2x2
`BitmapData`, assigned it to the private
`__ernativeui_tab_viewport_probe_bitmap_data` property on the freshly resolved
movie-owned `ItemIcon` parent, and immediately read that property into a
second wrapper. The readback must have the exact same AS object payload. Only
after that identity check succeeds does the probe destroy and free the
readback wrapper, then destroy and free the creator wrapper, leaving the parent
property as the sole explicit root.

The next gate added one deliberately minimal mutation after that lifecycle
was proven stable. Verification used a strict two-phase state machine. On the
first later quiescent post-native-composite frame, the probe resolves a fresh
parent wrapper, reads the private property again, and first proves the exact AS
payload identity recorded at creation. It then atomically claims a process-wide
one-shot guard and invokes exactly:

```text
BitmapData.setPixel32(0, 0, 0xA17BC3E5)
```

There is no result wrapper and the target is passed as a non-display object.
After a successful invocation, the readback wrapper is destroyed and freed and
the state machine arms a `post-mutation-identity` phase. On a later quiescent
post-native-composite frame, a second fresh parent/property readback must still
produce the same payload identity. This second phase is identity-only: it does
not invoke any method or inspect any pixel value. The probe therefore proves
that Scaleform accepted one write and that the rooted object remained alive;
it did not prove the resulting pixel color.

That gate failed visually and is now disabled. Immediately after the sole
`setPixel32` call, Elden Ring's renderer turned black. The call nevertheless
returned success with no SEH exception, the temporary wrapper was released,
and a fresh readback on the following frame recovered the identical AS payload.
The UI thread continued running and logging normally. This isolates the unsafe
boundary to the pixel mutation's renderer-visible side effect; construction,
movie-property ownership, wrapper release, and next-frame object lifetime had
already succeeded without corruption.

`kEnableBitmapDataPixelMutationProbe` is therefore `false`. The deployed safe
baseline stops after the first later-frame identity verification, releases its
temporary wrapper, and leaves the private property movie-owned. A valid safe
baseline log ends with
`bitmapDataProperty ownershipOnlyBaselineVerified=1 ... pixelMutationEnabled=0`.
The failed code remains in the research source so its exact ABI and evidence
are preserved, but it cannot execute accidentally.

The current safe baseline also contains a strictly passive VM-context probe.
From the already validated AS3 `MovieRoot`, guarded reads sample the VM pointer
at `MovieRoot+0x88`, the DrawableImage context at `VM+0x5470`, and its provider
at `VM+0x5478`. Creation logs these three addresses after the private
`BitmapData` property has been established. The ownership-only next-frame gate
reacquires and validates the `MovieRoot` through the fresh parent wrapper,
guarded-reads the same fields, and logs whether the VM, context, or provider
address changed. A missing or changed field is diagnostic only: it neither
invalidates the already independent property-identity proof nor changes wrapper
cleanup. This probe makes no new native call, installs no new hook, invokes no
ActionScript method, and performs no mutation.

The runtime result was stable and sharper than the original hypothesis. The
same `MovieRoot` and VM survived unchanged across the ownership gate, while
both `VM+0x5470` and `VM+0x5478` were readable and null. The Configuration VM
therefore has neither a pre-existing DrawableImage context nor the provider
needed to construct one. Together with the earlier successful-but-blackening
`setPixel32` call, this localizes the unsafe boundary to lazy mutable-image
backing initialization. The probe must not call that initializer directly or
perform another BitmapData pixel mutation.

The next deployed gate disables even the proven ownership-only BitmapData
constructor and observes the immutable native resource already used by the
stock icon binder. In the exact correlated pre-bind window it guarded-reads
only scalar fields from the live `CS::ScaleformImageResource` and its exposed
`Render::Image`: vtables and their game-image RVAs, reference counts, inline
delegate state, the resource kind, the image type/format/size virtual slots,
and candidate `CSTextureImage` size/sub-rectangle fields. It makes no virtual
call, performs no AddRef or Release, changes no byte, and retains no pointer
after the original binder consumes the lookup reference. Records begin with
`nativeImageResource[...] passiveOnly=1`.

No parent, Movie, or managed wrapper crosses a frame boundary. Only the scalar
payload address and scalar VM/context/provider addresses are retained for
identity and passive stability comparison. The property is neither cleared nor
disposed; the owning Configuration movie is responsible for its eventual
teardown.

The icon presenter runs while a pending viewport candidate is being repainted,
before that candidate replaces the active viewport. Scheduling must therefore
capture the pending candidate's logical mapping and generation. Capturing the
old active viewport correctly fails the generation guard but never executes the
constructor; that false-positive stable run is retained in the log history.

Every independently allocated BitmapData creator/readback wrapper used by this
fault-isolation gate starts in process-heap storage because a GFx value
participates in an intrusive list; abandoning such a wrapper on the stack after
a guarded native fault would be invalid. Successful paths destroy and free
wrappers in the order described above. Any exception, identity mismatch,
rejected mutation, or uncertain destruction fails closed and retains at most
the creator, same-frame readback, and current verification blocks for the
process lifetime. The retained-wrapper slot must be empty before either
verification phase allocates a fresh block. The probe never retries the
mutation, clears the property, or frees a possibly live wrapper after such a
failure.

The disabled mutation gate performed no width/height property reads, pixel readback,
`Bitmap` construction, `addChild`, visibility mutation, property clear, or
explicit `BitmapData.dispose()` call. Its only new operation is the one
`setPixel32` invocation. The visible cross remains the already proven vector
`Shape` path, so no visual change is expected.

A historical valid run of the disabled ownership baseline logs these records
in order:

1. `bitmapDataDeferred scheduled`
2. `bitmapDataDeferred execute phase=post-native-composite ... exactMovie=1`
3. `bitmapDataProperty create ... setMember=1 ... identity=1 ... operationSucceeded=1`
4. `bitmapDataVmContext creation ... passiveOnly=1`
5. `bitmapDataProperty rooted ... phase=pre-mutation-identity ...`
6. `bitmapDataProperty verification phase=pre-mutation-identity ... identity=1`
7. `bitmapDataVmContext verification ... passiveOnly=1 ...`
8. `bitmapDataProperty ownershipOnlyBaselineVerified=1 ... pixelMutationEnabled=0`

After a full game restart, keep the mouse below the tab strip and:

1. Open System and use R1 until Graphics is selected. The remap reveals the
   custom cross at the right edge and schedules the constructor experiment.
2. Wait five seconds on Graphics so the ownership-only identity frame can
   complete without changing the logical-to-physical mapping. No visual change
   is expected; pixel mutation is disabled in this safe baseline.
3. If still healthy, select **[Probe] Custom Tab 1**, wrap to Game Options, and
   close System. Report whether corruption begins during any of those distinct
   lifecycle boundaries.
4. If still healthy, reopen System once, traverse the custom tab again, close
   System, and exit the game normally so the log is flushed. This specifically
   exercises natural movie teardown; do not conduct a long play session with
   this diagnostic build.

A black screen, invisible UI, crash, missing page, or renderer corruption fails
the safe baseline even when all expected log records are present.

For the current immutable-resource snapshot, restart the game, open System,
and use R1 until the viewport shifts once. The cross and ellipsis must remain
the already proven vector Shapes. The log should contain paired
`nativeImageResource` records with every read flag set, followed by the normal
`iconBind` and `shapeAttach` success records; it must contain no
`bitmapDataDeferred` record.

The next gate uses a different Scaleform facility. Static analysis identified
`Scaleform::Render::RawImage::Create`, `RawImage::GetImageData`,
`ImageData::Clear`, and the intrusive `Release` slot in both supported game
executables. Unlike `BitmapData`, `RawImage` owns ordinary system-memory pixel
planes and does not ask the Configuration movie for its missing
DrawableImage provider.

The final validation experiment publishes four independent visual RawImage
assets atomically. On the first stable System frame it does the following:

1. Validate every selected RVA and entry signature for the running executable.
2. Create four 32x32, one-level, immutable `Image_R8G8B8A8` RawImages through
   the real Scaleform factory with its default heap and no update synchronizer.
3. Fill each image with a white border and a unique fully opaque asymmetric
   pattern. The first is the proven `+` channel/orientation chart. The second
   uses the same black canvas, white border, palette, and marker sizes but an
   `X` with its corner markers reversed. The other two use distinct diagonal
   and checker/center patterns. Every generated pixel has alpha 255 so this
   gate tests multi-resource identity and rebinding without alpha semantics.
4. Read every written row back and compare an independent deterministic
   checksum for each asset.
5. Clear only the copied `ImageData` view.
6. On the exact 2.7.1.0 executable, validate the native resource constructor,
   AddRef and Release helpers, deleting destructor, game heap allocation seam,
   RawImage type method, and both expected vtables before allocating anything.
7. Allocate four native `0x98`-byte resources, construct a uniquely named
   `CS::ScaleformImageResource` around each RawImage, and require every direct
   image layout and RawImage reference-count transition from `1` to `2`.
8. Release each creator RawImage reference and require the wrapper-owned
   reference count to be `1`. Only after all four entries pass does one atomic
   catalogue gate publish IDs `90001`, `90003`, `90004`, and `90005`; ID
   `90002` remains reserved for the vector overflow ellipsis. For the current
   one-run discriminator, the gate swaps the complete original asset-0 and
   asset-1 triples (`resource`, `RawImage`, and checksum) at publication. It
   does not change logical slots or ownership accounting. The pinned probe
   deliberately makes no game calls from `DllMain`.
9. Resolve the exact logical tab through the in-flight candidate viewport,
   match it to one indexed asset, and return one AddRef'd lookup reference.
   Elden Ring makes its ordinary binder-local reference and later releases the
   lookup reference normally. Resource refs must be exactly `1 -> 2` at lookup
   and `3 -> 2` at bind.
10. Create or reuse the already-proven runtime `flash.display.Shape`, pass that
    dynamic Shape to Elden Ring's unchanged image binder exactly once, retain
    it through the parent property, attach it on the same front/back layer, and
    hide the authored static Shape only after every ownership and presentation
    gate succeeds. The authored Shape is never asked to accept the RGBA image.
11. If any dynamic binder returns false after the exact `3 -> 2` resource
    transition, paint the proven vector cross into that same Shape immediately
    and disable later RGBA publication. A pre-bind validation failure consumes
    the binder-local reference with one exact native Release instead. The
    overflow ellipsis remains on the existing vector-Shape path. Any asset
    failure disables raster publication for the complete catalogue.
12. Track every front/back Shape outside ephemeral viewport state, including
    the exact asset bound to each host. Cleanup uses heap-backed GFx wrappers,
    resolves the exact dynamic Shape's `graphics` object and requires a
    successful `graphics.clear()` before `removeChild`. This synchronously
    removes the bitmap-fill command while the Shape remains reachable; merely
    removing an AS3 child is not accepted as image-release proof because the
    tracing collector may keep that detached Shape alive. Cleanup then requires
    authored visibility, both reserved properties cleared and read back as
    Undefined, successful wrapper/proxy teardown, and the unchanged exact
    RawImage refcount audit. A new movie is never mutated through an old
    fingerprint: its stale ledger is accepted only after every retained
    resource and RawImage returns to its process root refcount `1`. If the movie
    changes at the same native Configuration address while mapped records
    survive, the probe first restores all eight records from its process-stable
    pristine stock snapshot; it never treats shifted or ellipsis-masked records
    as a fresh baseline.

The test resource is not inserted into the game's repository and is independent
of a particular Configuration movie, so the same addresses must remain valid
across System close/reopen. A successful 2.7.1.0 log contains:

```text
rawImageLifecycle begin ... interfacesExact=1 exposed=0 bound=0
rawImageLifecycle object ... exact=1 ... ref=1 ...
rawImageLifecycle data ... returned=1 ... exact=1 ...
rawImageLifecycle pixels written=1 verified=1 ... exposed=0 bound=0
nativeImageResourceLifecycle begin ... interfacesExact=1 ... exposed=0 bound=0
nativeImageResourceLifecycle heap ... exact=1 ... size=0x98 arg3=null
nativeImageResourceLifecycle allocation completed=1 ... seh=0x00000000
nativeImageResourceLifecycle constructor completed=1 ... exact=1 ... rawRef=2 ... bound=0
nativeImageResourceLifecycle ownership creatorReleased=1 ... rawRef=1 exact=1
rgba8Resource staged icon=... id=... resourceRef=1 rawRef=1 checksum=... intentionalProcessLifetime=1
rgba8Catalogue publish slot=... id=... sourceAsset=... sourceId=... resourceRef=1 rawRef=1 checksum=... discriminator=fresh-vs-reused-shape
rgba8Catalogue ready count=4 alpha=opaque-only publication=atomic sourceMap={1,0,2,3} discriminator=fresh-vs-reused-shape
rawImageLifecycle cleanup ... resourceProbe={attempted:1 completed:1} visualCacheRetained=1 uncertainRetained=0 completed=1 ...
rgba8Lookup[...] epoch=... icon=... logical=... checksum=... exact=1 addRef=1 ... refs={before:1 ... after:2} ... verified=1
rgba8Shape[...] ... created=1 ... bindAttempted=1 rasterBound=1 ... refs:1/3->1/2 exact:1
rgba8Shape[...] ... properties={set:1/1 verified:1} ... customVisible:1 ...
rgba8Bind[...] epoch=... icon=... exactAsset=1 fingerprintConsistent=1 ... bridge=1 consumed=1 bindAttempted=1 ... refs={before:3 after:2 exact:1} rasterBound=1 customVisible=1 vectorFallback=0 cleanupExact=1 result=1 seh=0x00000000 authoredTarget=0 dynamicShape=1
rgba8Lifecycle[...] phase=post-present ... hosts={...} ... asset=... liveHosts=... refs={resource:1 ... raw:... rawExpected:...}
iconDetach ... fingerprint={read:1 known:1 same:1 ...} ... graphics={constructed:1 exact:1 clear:1 seh:0x00000000} ... removed=1 authoredVisible=1 propertyCleared={1,1 verified:1} cleanup={wrappers:1 proxy:1} ... result=1
```

The preceding discriminator proved that both native resources are healthy and
that a cleared Shape cannot safely accept a different raster asset. The current
gate is therefore a **two-transition detach-and-create replacement proof**.
Fully restart Elden Ring, keep the mouse away from the tab strip, open System,
and use R1 until **[Probe] Custom Tab 1** is selected. Its icon remains the
original asset-1 opaque `X` on a fresh Shape.

Press R1 exactly once to select **Custom Tab 2**, then wait five seconds without
navigating. Custom Tab 1 must remain visible with the same `X`; selected Custom
Tab 2 must show the colored `+`, not a white square. This first proof retires
the front/back physical-6 raster Shapes (`0x4040`), waits for their renderer
references, and commits target ledgers `injected=0xE1E1` and `rgba=0x6060`.
Its transaction line must end with `proof=1 completedProofs=1
futureRemapsFrozen=0 restartRequired=0`.

Now press R1 exactly once more to select **Custom Tab 3** and wait another five
seconds. This is the only remap armed after proof 1. The final strip must show,
from physical slots 4 through 7, four distinct opaque raster icons: the `X`,
the `+`, the cyan/orange diagonal target for Custom Tab 3, and the
checker/green-center target for Custom Tab 4. Physical slot 0 remains the left
`...` indicator. Because window 4 is the maximum window, slot 7 is the real
Custom Tab 4 icon rather than a right `...` marker. Titles, selected page
content, focus, and input must remain correct. A very brief authored icon
during either blocked retirement interval is harmless.

Do not navigate again, wrap, or close/reopen System during this run. After the
second five-second observation, exit Elden Ring normally without reopening
System and inspect the log. Restart the game before another probe run;
movie-epoch cleanup in the presence of renderer-retirement debt remains a
separate gate. Attempts to remap anywhere except the exact second forward
transition are deliberately restored and refused after proof 1.

For proof 1, the log must retain the already-proven `0x4040 -> 0x8181 ->
0xE1E1` lifecycle: two exact raster detachments, zero debt before repaint, and
fresh physical-6 `+` Shapes. For proof 2, source preflight must report injected
`0xE1E1`, RGBA `0x6060`, p0/p7 vector Shapes, p5 asset 1, p6 asset 2, and p4
Undefined on both layers. Phase A must first detach the two p7 vector Shapes,
then detach the four p5/p6 raster Shapes. Every `iconDetach` must report
`removed=1`, both properties verified Undefined, and exact wrapper/proxy
cleanup. The combined detach mask is `0xE0E0`, split into raster `0x6060` and
vector `0x8080`; the retirement-ready state is injected `0x0101`, RGBA zero,
and every asset-host entry zero while source records remain published.

RawImage retirement debt is tracked independently for publication assets 0
and 1 and may decrease over subsequent blocked native frames. Phase B is armed
only when every asset reports root-only references and zero debt. The target
repaint must create, never reuse, all eight front/back raster Shapes at p4-p7.
Each `rgba8Replace attach` must report its own asset, `created=1`, `reused=0`,
and `exact=1`. The stable target has injected `0xF1F1`, RGBA `0xF0F0`, vector
`0x0101`, asset entries `1,2,3,4` at p4-p7 on both layers, resource references
`1,1,1,1`, and RawImage references `3,3,3,3`. No manual RawImage release is
permitted.

The second `rgba8Replace transaction` line must report detach `0xE0E0`, fresh
attach and RGBA presentation `0xF0F0`, vector presentation `0x0101`, exact
postflight properties `0xF1F1`, `proof=1`, `completedProofs=2`,
`futureRemapsFrozen=1`, and `restartRequired=1`. Any timeout, nonzero SEH,
unexpected property state, reference outside `1 + active hosts + debt`,
incomplete mask, or `proof=0` fails the experiment, freezes later remaps, and
disables further RGBA8 publication.
Intentional `visualCacheRetained=1` is the expected process-lifetime root, not
a failure. A different-Movie epoch still requires a separate literal
`resourceRef=1/rawRef=1` audit before discarding the old host ledger;
close/reopen runtime validation remains a separate gate.

Do not combine this probe with any earlier tab probe. Clear cached probe
options when building it:

```powershell
cmake --preset windows-release `
    -DERNATIVEUI_BUILD_TAB_RESEARCH_PROBE=OFF `
    -DERNATIVEUI_BUILD_TAB_DUPLICATE_RESEARCH_PROBE=OFF `
    -DERNATIVEUI_BUILD_TAB_PRESENTATION_RESEARCH_PROBE=OFF `
    -DERNATIVEUI_BUILD_TAB_VIEWPORT_RESEARCH_PROBE=ON
cmake --build --preset windows-release `
    --target ERNativeUITabViewportProbe ERNativeUITabViewportTests ERNativeUITabPointerDwellTests ERNativeUITabRecordEpochTests
ctest --preset windows-release `
    -R '^ERNativeUI\.(TabViewport|TabPointerDwell|TabRecordEpoch)$' --output-on-failure
```

For the clean runtime test, load the official host first and the viewport probe
second. Do not load `NativeTweaks.dll`, its private `ERNativeUI.dll`, or its GFX
override:

```toml
[modengine]
external_dlls = [
    "mod\\DemoConfigMenu\\ERNativeUI.dll",
    "mod\\ERNativeUITabViewportProbe\\ERNativeUITabViewportProbe.dll",
]

[extension.mod_loader]
enabled = true
loose_params = false
mods = [
    { enabled = true, name = "default", path = "mod" },
]
```

Remove or disable any separate mod-loader entry whose path is
`mod\\NativeTweaks`; otherwise its `menu\\win\\02_040_optionsetting.gfx` can
still replace the stock movie even when its DLL is absent. The probe validates
that the loaded host ends in `\\DemoConfigMenu\\ERNativeUI.dll`, has the known
official file size of 940544 bytes, and observes exactly the eight stock
categories `0,1,2,3,5,6,7,8` before it mutates anything.

For the first runtime test, keep the mouse away from the tab strip and navigate
with L1/R1 only. The current provenance stage observes the exact GridControl
pointer, previous-tab, and next-tab branches before they converge on the final
selection callback. It logs both that native source and the older
adjacency-based inference; it deliberately leaves viewport behavior unchanged.

The following is the already-completed 40-tab viewport stress sequence, kept as
research history rather than instructions for the current four-icon build:

1. The initial strip shows the seven stock tabs from Game through
   Keyboard/Mouse followed by `...`; Graphics is still mapped underneath the
   masked last slot.
2. Move right through Keyboard/Mouse. The next R1 selects Graphics while the
   window shifts. Graphics remains selected in the second-to-last physical
   slot, and both edge slots display `...` because tabs are hidden on both
   sides.
3. Continue with R1 through **Custom Tab 1** to **Custom Tab 39**. Each
   selected tab reveals the colored cross in the second-to-last slot while
   both edge indicators remain visible. Holding R1 should retain Elden Ring's
   native controller-repeat behavior across the long catalogue.
4. R1 selects **Custom Tab 40** in the last slot. The right dots disappear at
   the true end, while the left dots remain.
5. R1 wraps to Game and restores the initial right-overflow presentation.
6. From Game, L1 wraps to **Custom Tab 40**. Continue left through every custom
   and stock tab and confirm that the presentation is symmetric.

Check that every physical position has exactly one icon after each shift, then
check the selected title and page route along with focus, page input, visible
flicker, and mouse-hover selection across all eight cells. The current probe
also prototypes the accelerating mouse-only edge dwell described below. The
probe writes
`ERNativeUITabViewportProbe.log` beside its DLL. Successful runs contain
`Viewport probe hooks installed`, `Viewport initialized`, and `selection[...]`
or `remap[...]` records. This alias experiment must additionally produce an
`iconAlias[...]` records for `MENU_ItemIcon_90001` (cross) and
`MENU_ItemIcon_90002` (dots), each with a non-null resource address, followed
by matching `iconBind[...]` and `shapeAttach[...]` records. It is a temporary
research artifact and must never be shipped to players.

### Mouse/controller provenance run

The current path-discrimination probe uses one short sequence after a full
restart. Pause for about one second between each numbered action so the log
groups remain unambiguous:

1. Open System with the pointer below the tab strip.
2. Tap R1 once, then tap L1 once.
3. Hold R1 long enough to repeat through at least four tabs, then release it.
4. Hold L1 long enough to repeat through at least four tabs, then release it.
5. Move the mouse directly to a non-adjacent visible tab.
6. Hover one adjacent visible tab.
7. Hover the right `...` and leave the pointer stationary until the current
   prototype reaches the final custom tab.
8. From that shifted view, tap R1 once and L1 once so both controller wrap
   directions are represented.
9. Close System, reopen it once, tap R1 once, then exit the game normally.

The final provenance probe expects R1/L1 transitions to log
`source=navigation-next` / `source=navigation-previous`, including held repeat,
wrap, and the post-reopen tap. Ordinary adjacent and non-adjacent mouse tab
hovers, including the stationary repaint cascade over `...`, must log
`source=pointer`. Any user-driven transition logged as `source=unknown`, or a
direction/source mismatch, fails this stage. This proof must pass before the
mouse-only dwell/repeat policy is enabled.

### Completed native pointer-poll equivalence proof

The B880 observation proved that Elden Ring's ordinary GridControl pointer path
is motion-driven: it emits one sample when the cursor changes, but provides no
heartbeat while the cursor remains stationary. A completed observation-only
stage synchronously reused Elden Ring's own cursor conversion and GridControl
hit test once per active input frame. Across 1,530 frames, all 331 natural
samples matched the independent poll exactly, 1,199 stationary frames produced
valid fallback polls, and no coordinate mismatch, hit mismatch, fault, or SEH
occurred. The current probe builds on that proof and now mutates edge selection
for the bounded dwell experiment below.

The helper entry points are currently validated only for Elden Ring 2.7.1.0.
On 2.7.0.0 the rest of the viewport probe remains available, but the hook
banner reports `nativePoll={status:unavailable ...}` and this particular run
cannot be evaluated.

After a full game restart, use this sequence and pause for about two seconds at
each stationary step:

1. Open System with the pointer below the tab strip and leave it still.
2. Move onto an ordinary, non-selected tab and leave the pointer still.
3. Move onto the right `...` and leave it still while the existing prototype
   cascades to the final custom tab.
4. Move the pointer by a very small amount while remaining inside that same
   edge cell, then leave it still again.
5. Move down into the page content and leave it there.
6. Without moving the pointer from the page content, tap R1 once and then hold
   R1 through several tabs.
7. Exit the game normally.

A successful trace emits a `pointerPoll[...] event=summary` about once per
second even while the cursor is stationary. `fallback` should dominate those
stationary intervals, every natural comparison must keep `coordMismatch=0` and
`hitMismatch=0`, and leaving the tab strip must produce a valid `hit=-1` rather
than treating an absent natural event as a leave. `faults` and `lastSeh` must
remain zero. Controller navigation must continue to log
`source=navigation-next`; it must not be reclassified by the polling observer.

### Completed 40-tab mouse edge-dwell mutation run

The completed 2.7.1-only dwell prototype kept the first native hover on `...`
immediate, suppresses only repaint-induced reselection under the stationary
cursor, and then advances with an accelerating wall-clock cadence. The first
automatic step waits 250 ms. After it commits, successive waits are 200, 180,
160 ms, and so on, subtracting 20 ms per committed automatic step until the
configured 50 ms minimum is reached. Every later tab remains at 50 ms; the
probe never becomes instant, even across its 40 custom tabs. Genuine pointer
motion inside the edge resets the streak and starts a fresh 250 ms initial
wait. A configured decay of zero would mean a fixed 200 ms repeat cadence,
not disabled dwell. A configured minimum above the repeat delay is normalized
down to the repeat delay, so the floor cannot accidentally slow the cadence.

The probe calls the same native cell setter as Elden Ring's ordinary pointer
path immediately before the final selection comparison, so the game retains
its normal callback, focus, sound, and page-switch path. L1/R1 disarms the
dwell before dispatch and remains immediate with its native held-repeat and
wrap behavior. Mouse dwell never wraps past either catalogue endpoint.

After a full game restart:

1. Open System with the pointer below the strip. Hover the right `...` once
   and stop moving. The first shift must be immediate, the next shift should
   follow after about 250 ms, and later shifts should visibly accelerate from
   roughly 200 ms before settling at 50 ms. Continue until Custom Tab 40 is
   selected; the full stationary traversal should take roughly three seconds,
   not collapse into a same-frame burst. The final revealed real edge tab is
   selectable; the mouse must stop there without wrapping to Game Options.
2. Repeat from a clean open. During the cooldown after the first shift, move
   the pointer a very small amount while remaining inside the same edge cell.
   The next advance must wait a fresh 250 ms from that motion before the
   accelerating repeat cadence starts again.
3. While a right-edge dwell is active, tap and then hold R1. Controller input
   must take priority immediately, retain native repeat/wrap, and cancel the
   mouse dwell rather than receiving an extra delayed selection.
4. Reach the shifted end view and hover the left `...`. Keep the pointer still
   until Game Options is selected. The reverse path must advance once per
   accelerating dwell step and stop at Game Options without wrapping.
5. While either dwell is active, move onto an ordinary tab and then into page
   content before the deadline. Each move must cancel the pending repeat. Move
   directly from one edge `...` to the opposite `...`; the new edge must select
   immediately and begin its own dwell without a repaint cascade.
6. Test direct mouse jumps between physical slots 0 and 7 in both directions.
   They must resolve against the visible mapping, never be mistaken for
   controller wrap. Close and reopen System and repeat one forward/reverse run
   to verify lifecycle stability.

The log should show `pointerDwell` events in the order `begin`, `commit`,
`due`, `dispatch`, and `accepted` for each repeated step, followed by either
another `commit` or `endpoint`. Each record includes the active `delay` and
the number of committed automatic repeats. The first committed natural hover
must log `delay=250`; the `commit` records after successive accepted repeats
must log `delay=200`, then `180`, `160`, and so on until they remain at
`delay=50`. The startup banner must report `minimum:50ms`. Motion resets both
the deadline and repeat count,
leaving the edge logs `leave`, and controller input logs `controller`. A
`failed` event,
validation cancellation, source mismatch, duplicate selection, instant
cascade, endpoint wrap, blank/wrong page, or nonzero poll mismatch/fault fails
this experiment.

### Final 50-tab, 20-row and alpha RGBA8 stress run

The final pre-production fixture contains 58 logical tabs: Elden Ring's eight
stock tabs followed by 50 custom tabs. Custom Tab 1 through Custom Tab 50 use
independent immutable 32x32 RGBA8 resources with IDs `91000` through `91049`.
Each icon visibly contains its `01` through `50` number, and the publication
gate requires all 50 RawImages, resources, pointers, and checksums to be
present and pairwise distinct before any raster icon becomes available. The
overflow ellipsis remains the separate vector resource `90002`. Each custom
tab owns 20 independent sliders. The seven-row Graphics carrier renders rows
1 through 6 plus `Next Page`; the continuation renders `Previous Page` plus
rows 7 through 20.

Every icon is also an alpha-convention chart. Behind the opaque white number,
the upper half contains straight RGB with a left-to-right alpha ramp and the
lower half contains the same RGB premultiplied by that ramp. If alpha is
ignored, the upper half becomes a solid coloured rectangle. If straight alpha
is consumed, the upper half is the natural fade and the lower half is too
dark. If premultiplied alpha is consumed, the lower half is natural and the
upper half leaks bright colour. This is a research gate only: the draft public
API continues to accept opaque RGBA8 exclusively until runtime settles the
convention.

This build uses the correctness-first transition path: every remap detaches
all injected source Shapes, waits at least one native frame and for exact
renderer-reference retirement, then creates every target Shape fresh. It is
therefore a conservative upper bound for navigation cost. Verbose successful
diagnostics are disabled so file I/O does not dominate the measurement;
ownership, reference, property, epoch, and mapping gates remain enabled.

After a full game restart, keep the mouse below the tab strip and:

1. Open System and wait five seconds. Confirm the normal first window and its
   right `...` marker appear without a black frame or missing page. This first
   open constructs the complete 50-resource catalogue.
2. Select Custom Tab 1 and stop. Inspect its icon before doing any stress:
   alpha-zero pixels must reveal the native tab strip, no solid rectangular
   fallback may appear, and one of the two ramp halves must blend naturally.
   If this fails, stop the run so the alpha result remains isolated.
3. On Custom Tabs 1, 25, and 50, confirm rows 1 through 6 plus `Next Page` on
   the main carrier. Open `Next Page`, confirm `Previous Page` plus rows 7
   through 20, change rows 1 and 20, return, retire the tab by navigating away,
   and revisit it to confirm both independent values persisted.
4. Tap R1 through all stock tabs and Custom Tabs 1 through 50. Verify every
   custom icon number advances exactly once, then tap R1 once more and confirm
   the viewport wraps to a populated Game Options page.
5. From Game Options, tap L1 once. It must reverse-wrap directly to Custom Tab
   50 with icon `50`; continue with L1 through the full catalogue and back to
   Game Options.
6. Hold R1 through at least three complete wraps, then hold L1 through at
   least two. Release and make several rapid direction reversals. Native held
   repeat, focus, titles, page input, icons, and overflow markers must remain
   coherent, with no visible hitch that grows over time.
7. Close and reopen System five times. On the fifth open, complete another
   full forward and reverse traversal. Resources are process-lifetime roots;
   reopening must not rebuild, alias, whiten, or lose the catalogue.
8. Reopen at the first window and hover the right `...` without moving the
   pointer. Confirm the 250/200/180/.../50 ms dwell cadence reaches Custom Tab
   50 without wrapping. Hover the left `...` to return to Game Options. During
   another dwell, move slightly inside the cell to reset the delay, then tap
   R1 to confirm controller input cancels dwell and remains immediate.
9. Exit the game normally and inspect `ERNativeUITabViewportProbe.log`.

A successful quiet log contains `STRESS bootstrap complete=1`, one
`STRESS catalogue ready count=50 ... alpha=straight-vs-premultiplied-ramp ...
unique=1` record, a
`STRESS viewport initialized ... logical=58 physical=8` record for every
System epoch, and a `STRESS transition ... complete:1 frozen:0` record for
every remap. Host carrier records must report `logicalRows=20`, exact attempted
and added slice counts, `identityStable=1`, and `committed=1`. Catalogue timing
measures the first-menu-open resource build.
Transition timing separates queueing, Phase A detach, renderer retirement,
and Phase B repaint. Any `ERROR`, `WARNING`, invalid timing, incomplete
transition, frozen remap, incorrect numbered icon, missing wrap, empty/wrong
page, black screen, or steadily increasing latency fails the run.
