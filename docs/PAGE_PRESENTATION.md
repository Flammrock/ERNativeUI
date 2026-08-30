# Page titles and presentation

ERNativeUI treats a logical page title and Elden Ring's surrounding menu title
as different presentation layers. This distinction matters because the game
renders them through different Scaleform fields even though its native code
reaches both through similarly named runtime paths.

## Default physical-slice title

A submenu declares one logical base title. If it fits on one native page, that
title is displayed unchanged. If pagination creates more than one physical
slice, ERNativeUI formats every slice as:

```text
Title (n/t)
```

`n` is the one-based physical page number and `t` is the total number of
physical slices. For example, the three slices of `Advanced Settings` are
shown as:

```text
Advanced Settings (1/3)
Advanced Settings (2/3)
Advanced Settings (3/3)
```

Pagination remains host-owned. A client supplies the logical title or an
optional formatter; it never creates separate logical pages merely to change
the suffix.

## Outer-title policy

The surrounding title identifies who owns the current page:

| Page being shown | Outer title |
| --- | --- |
| Existing shared Controller Settings root | Elden Ring's `Configuration` |
| Continuation of the shared ERNativeUI root | `ERNativeUI` |
| A provider submenu | Provider display name by default, or that page's explicit override |

The shared root deliberately keeps `Configuration`: it is still Elden Ring's
Controller Settings page and may contain rows from several providers. A root
continuation is a host-owned physical page, so no individual mod may claim its
outer title. A provider-owned submenu may use a more specific outer title when
that makes navigation clearer.

Root-page presentation cannot be overridden through a provider draft. This
prevents registration order from deciding shared host chrome.

## Custom title formatters

The C++17 wrapper exposes `erui::PagePresentation`,
`erui::PageTitleFormatter`, and `Page::set_presentation(...)`. A callback that
does not need `user_data` can use the
`Page::set_presentation<&formatter>(menu_title, page_title)` template overload.
`erui::write_page_title` copies a completed `std::wstring_view` into the
provided output buffer.

The strict-C equivalents are `ERUI_PagePresentationDesc`,
`ERUI_PageTitleFormatter`, `ERUI_PageTitleFormatContext`, and
`ERUI_Api::set_page_presentation`. `ERUI_PAGE_TITLE_BUFFER_CAPACITY` is the
maximum advertised scratch capacity.

The formatter receives the provider and logical-page handles, the borrowed
effective base title, and the one-based page number and page count. The host
provides a fixed UTF-16 output buffer. A successful formatter writes the title
and reports its length; a terminator is not required.

Formatters run once per physical slice during startup compilation. ERNativeUI
copies and caches the result before any page is shown. If a formatter rejects
the context, fails, produces invalid output, or exceeds the host buffer, the
host falls back to the safe default title instead of failing menu publication.

Formatter safety rules:

- Treat the context, base-title view, and output buffer as borrowed for that
  invocation only. Never retain their addresses.
- Write no more than the advertised capacity and report a length in UTF-16
  code units. Do not include an embedded NUL.
- Keep the callback deterministic, quick, and independent of Elden Ring UI
  state. It executes during startup compilation, not when a player opens the
  page.
- Do not re-enter ERNativeUI registration or value APIs from the formatter.
- Callback code and `user_data` must remain valid until `commit_provider` (or
  the C++ `register_menu` call) returns. The host invokes the formatter only
  during compilation and retains only its copied text result.
- No C++ exception may cross the C ABI. A C++ wrapper callback must be
  `noexcept`.

Because the host owns the output buffer and copies the result, no allocator or
STL object crosses the DLL boundary.

## Why there are two title layers

Two user-owned PC menu assets participate in the observed flow:

- `02_040_optionsetting.gfx` owns the outer options/menu presentation,
  including the Controller Settings root and the visible outer
  `MenuTitle/StaticText_101003` field used for `Configuration`.
- `02_042_pc_graphicsetting.gfx` owns the graphics-style native subpage. Its
  visible inner heading is `GraphicOption/StaticText_111114`.

The runtime path resolver was observed receiving the inputs
`MenuTitle/Text` and `MenuTitle/Text_0`. Those are resolver inputs used by game
code; they are not names of the two visible fields above. Treating the names
as interchangeable was the key early mistake.

The identification process was:

1. A bounded, rate-limited path observation recorded the resolver input only
   while a known ERNativeUI subpage request was pending.
2. Both `MenuTitle/Text` and `MenuTitle/Text_0` were observed from the native
   page-construction path. The returned Scaleform value wrapper was also
   captured, and the game's own setter behavior showed that its UTF-16 value
   object is addressed at wrapper plus `0x08`.
3. An early write through the captured object succeeded but changed the outer
   `Configuration` heading. That successful wrong-target experiment proved the
   setter and object lifetime were correct while also proving that the chosen
   visible field was not the page heading.
4. The tag graphs in the user's extracted `02_040` and `02_042` files were
   compared. This separated `MenuTitle/StaticText_101003` from the desired
   `GraphicOption/StaticText_111114` field.
5. Resolution is redirected only during construction of a pending custom
   physical page, after which the compiled and cached inner title is applied.
   A separate native temporary resolves and sets the outer field, so the
   page-owned inner result is never overwritten. The temporary's nested value
   is destroyed exactly like the game's own one-shot resolver call sites.
6. Live runs independently verified the inner redirection and setter, the
   outer field/setter, and native Next/Previous/Back navigation. The production
   bridge combines those bounded findings while leaving the game-owned root
   untouched.

The internal resolver and setter locations are game-version-sensitive native
implementation details, not public API or stable modder contracts. Maintenance
must rediscover and byte-validate them after an Elden Ring update; client mods
should only use the page-presentation API.
