# Research: Graphics carrier V2 with Network parking

This build-only probe supersedes both the two-bank experiment and the
category-4 Brightness carrier. It does not change production `src/` files and
must not be shipped as API 1.3.

## Model

- Category 8 / selector 11 is the only recyclable content carrier.
- Logical native Graphics and all fifty custom tabs are routes through that
  same carrier. At most one category-8 panel, SceneObjProxy wrapper, and
  Scaleform payload exists at a time.
- Category 5 / Network is only a synchronous parking target while the current
  category-8 panel is retired. ERNativeUI never retires or modifies Network.
- Native Graphics reconstruction retains its original row callback
  `0x95C050` and Graphics-only callback `0x95A820`.
- A custom reconstruction replaces only the exact validated row callback and
  zeros the exact `0x95A820` callback and its 32-bit metadata field, leaving
  the descriptor's four padding bytes unobserved and untouched. Selector 11 therefore
  takes its generic post-initialization branch instead of installing display
  settings apply/rollback state on client rows.

## Exact 2.7.1 seams

- Graphics wrapper: `0x93CEF0`
- Graphics callback locals: `0x93CF20` and `0x93CF32`
- Graphics selector immediate: 11 at `0x93CF84`
- Graphics pre-hook: `0x93CF89`
- Graphics native row materializer: `0x95C050`
- Graphics-only secondary callback: `0x95A820`
- Network wrapper: `0x93CE10`, selector 7
- Composite switch: `0x93C900`
- Composite teardown: `0x93C1A0`
- Panel retirement manager/deactivate: `0xE1C080` / `0x953520`

Every executable entry and wrapper-local contract is byte-checked before the
hook is enabled. The V2 probe exports a protocol-version function in addition
to V2-named prepare/verify functions so stale host/probe pairs are rejected.

## Transaction

For every route change involving native Graphics or a custom tab:

1. The selection callback creates a pending transaction even when the visible
   viewport mapping did not move.
2. If icons/records did not change, the commit performs a content-only path:
   no tab-layer resolution, selector relocation, record rewrite, or icon
   repaint occurs.
3. If category 8 is current, the host switches synchronously to Network with
   native state-copy suppression and verifies Network identity.
4. The old category-8 carrier must be at least one completed frame old. Its
   panel/wrapper/payload, cache slot, current pointer, native manager,
   deactivate function, allocator vtable, queue function, and function bytes
   must all match the allowlist. Both Graphics and Network identities must be
   panel/wrapper/payload-distinct from every other populated cache slot.
5. Before destruction, retained dialog/root page atomics are cleared only if
   they still equal the retiring panel, any impossible pending subpage request
   is cancelled, and color-preview bindings for that page are detached.
   Native deactivation and allocator queueing then run exactly once, cache
   slot 8 is nulled, and the carrier epoch advances.
6. The requested route is published before the native category-8 factory
   runs. Native Graphics calls its original materializer; custom routes inject
   their own row model.
7. The host switches back to category 8 and verifies the exact new identity,
   route, cache, and current pointer before the transaction commits.

Parking, retirement, reconstruction, and return occur within one composite
frame callback. No render/frame boundary exists while Network is current.
State-copy suppression is restricted to the internal category 8 -> 5 -> 8
parking sequence and transitions involving a true custom logical tab. Native
Graphics transitions to or from ordinary stock tabs retain vanilla state-copy
behavior.

## Failure policy

Any identity, age, allocator, cache, callback, state-copy, or same-frame proof
failure permanently freezes further carrier transitions for the process.
Unlike the superseded probe, this does not replace the composite input gate
with zero: Elden Ring continues receiving live native input, so Back/O and
ordinary menu teardown remain available. A failed content-only selection
attempts an exact selector restoration; a failure after a committed viewport
repaint publishes that candidate mapping before freezing to avoid an internal
mapping/selector split.

## Remaining live proof gates

The build proves compilation and pure viewport/dwell/configuration tests only.
A live run must still prove:

- selector-11 custom routes contain rows only, with no Graphics apply/rollback
  behavior;
- native Graphics is reconstructed byte-for-byte functionally after custom
  routes;
- Network never flashes and retains its original content/state;
- content-only forward/reverse changes, wraps, held L1/R1, mouse dwell, and
  close/reopen remain stable;
- final native teardown clears category 8 exactly once;
- custom values remain isolated across repeated recycling.

Do not enable the production API 1.3 capability until those gates pass.
