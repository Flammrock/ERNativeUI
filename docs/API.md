# ERNativeUI public API

## Game language and localization

`ERUI_Api::get_game_language` fills an `ERUI_GameLanguageInfo` whose
`known_language` classifies every locale currently advertised for Elden Ring
on Steam. `identifier` preserves Steam's exact UTF-8 token. A successful call
may therefore return `ERUI_GAME_LANGUAGE_UNKNOWN` with a non-empty identifier
for a newer or community locale. `ERUI_NOT_SUPPORTED` means no trustworthy
runtime value was available and should select the mod's English fallback.

Initialize `ERUI_GameLanguageInfo.size` before calling. Its identifier is
immutable host-owned memory valid until process exit. ERNativeUI uses ordinary
read-only Steamworks flat-C calls and installs no Steam hook.

C++ clients normally query before registration so the provider name can also
be translated:

```cpp
const erui::LanguageInfo language = erui::query_game_language();
const Text& text = translations_for(language.known, language.identifier);

erui::ProviderOptions options{};
options.display_name = text.mod_name;
```

The owned snapshot is also available from `Menu::game_language()` during the
builder and `Registration::game_language()` afterward. See
[Localized Greeting](../examples/localized_greeting) for a compact example.

ERNativeUI exposes two header-only client layers:

```text
include/ernativeui/erui.h             strict C binary ABI
include/ernativeui/ERNativeUI.hpp     C++17 convenience wrapper
```

CMake users link the interface target appropriate to their language:

```cmake
target_link_libraries(MyCMod PRIVATE ERNativeUI::CABI)
target_link_libraries(MyCppMod PRIVATE ERNativeUI::SDK)
```

Neither target links a binary. Clients discover the already-loaded host with
`GetModuleHandleW` and `GetProcAddress`; a missing host can disable only their
optional menu instead of preventing the client DLL from loading.

## C++17 quick start

```cpp
erui::ProviderOptions options{};
options.provider_id = "com.example.my-mod";
options.display_name = L"My Mod";
options.owner_module = module;
options.root_priority = 100;

const auto result = erui::register_menu(options, [](erui::Menu& menu) {
    auto root = menu.root();
    root.add_toggle(L"Enabled", L"Enable the feature.", 1, &changed);

    erui::SliderOptions slider{};
    slider.minimum = 0;
    slider.maximum = 100;
    slider.step = 5;
    slider.initial_value = 50;
    root.add_slider(L"Strength", L"Adjust strength.", slider, &changed);

    root.add_button<&apply>(L"Apply", L"Apply settings.");
    auto child = root.add_submenu(L"Advanced", L"Open advanced settings.");
    child.add_button<&apply>(L"Apply Advanced", L"Apply advanced settings.");
});
```

`register_menu` discovers the host, requests the exact API 1.0 contract,
creates a private draft, runs the builder, and commits. A public commit waits
for the bounded startup merge and native hook installation: success therefore
means the host reached `runtime_ready`; a game-signature failure returns
`ERUI_HOST_FAILED` instead of reporting a misleading successful menu.

If a builder throws or any row operation fails, the wrapper aborts the draft.
`Page` values are builder handles. They may be copied for convenient fluent
construction, but should not be retained; escaped handles become safely
invalid when registration closes.

`RegistrationResult` contains either an `Error` or a `Registration`. Retain
the registration when programmatic values are needed:

```cpp
erui::Registration registration = result.value();
registration.set_value(toggle_row, 0);

std::uint8_t value{};
registration.get_value(toggle_row, value);
```

The host owns toggle, slider, and choice storage. `set_value` updates the
public value immediately and queues the native-facing byte for the host
worker. It is not a UI-thread transaction; avoid continuously racing it
against player input.

## Choice rows

ERNativeUI exposes two explicit presentations with identical value semantics:

- `add_inline_choice` changes the value directly with left/right input.
- `add_popup_choice` renders as an action row and opens Elden Ring's native
  selection list.

Both accept 1..32 non-empty option strings, copy all strings before returning,
and expose a zero-based index through callbacks and `get_value`/`set_value`.
A popup callback is emitted only when the committed index changes; closing its
list without a change does not synthesize a notification.

```cpp
const std::wstring_view values[] = {
    L"Minimal", L"Balanced", L"Detailed", L"Maximum"
};
erui::ChoiceOptions choice{};
choice.values = values;
choice.count = std::size(values);
choice.initial_index = 1;

const erui::RowHandle inline_row = root.add_inline_choice(
    L"Inline Preset", L"Change with left/right input.", choice, &changed);
const erui::RowHandle popup_row = root.add_popup_choice(
    L"Popup Preset", L"Open the native selection list.", choice, &changed);
```

Strict-C clients reuse one descriptor for either presentation:

```c
ERUI_ChoiceDesc choice = {0};
choice.size = (uint32_t)sizeof(choice);
choice.label = label;
choice.help = help;
choice.options = options;
choice.option_count = option_count;
choice.initial_index = 0;
choice.changed_callback = on_changed;

api.add_popup_choice(provider, page, &choice, &row);
```

Check `ERUI_CAP_INLINE_CHOICE` / `ERUI_CAP_POPUP_CHOICE` together with the
matching function-table pointers after negotiation. Choice rows intentionally
have no public disabled flag because the required native behavior has not been
established. The recovered native storage and list construction are documented
in [Native popup choices](NATIVE_POPUP_CHOICES.md).

## Page presentation

`add_submenu` returns the provider-owned logical page. Its declared
`page_title` is the base title; when pagination creates multiple physical
slices, ERNativeUI displays `Title (n/t)` by default. Presentation may be
overridden while the provider draft is still open:

```cpp
auto advanced = root.add_submenu(
    L"Advanced", L"Open advanced settings.", L"Advanced Settings");

advanced.set_presentation(
    L"My Mod - Advanced", // surrounding menu heading
    L"Advanced Settings"  // logical base title
);
```

The shared Controller Settings root intentionally retains Elden Ring's
`Configuration` heading and rejects provider presentation. Shared-root
continuations use `ERNativeUI`. A provider submenu uses its provider display
name by default; `menu_title` may override it for that submenu only.

For full control, supply an `erui::PagePresentation` with an
`erui::PageTitleFormatter`, or use the no-user-data template overload:

```cpp
ERUI_Result ERUI_CALL format_page_title(
    const erui::PageTitleFormatContext* context,
    std::uint16_t* output,
    std::uint32_t output_capacity,
    std::uint32_t* out_length) noexcept
{
    if (!context || context->size < sizeof(*context) ||
        !context->base_title.data || context->base_title.length == 0) {
        return ERUI_INVALID_ARGUMENT;
    }

    try {
        const auto* base = reinterpret_cast<const wchar_t*>(
            context->base_title.data);
        std::wstring title(base, context->base_title.length);
        title += L" (" + std::to_wstring(context->page_number) + L"/";
        title += std::to_wstring(context->page_count) + L")";
        return erui::write_page_title(
            title, output, output_capacity, out_length);
    } catch (...) {
        if (out_length) *out_length = 0;
        return ERUI_OUT_OF_MEMORY;
    }
}

advanced.set_presentation<&format_page_title>(
    L"My Mod - Advanced", L"Advanced Settings");
```

The C++ callback aliases are `erui::PageTitleFormatter` (with `user_data`) and
`erui::StaticPageTitleFormatter` (adapted by the template overload).
`erui::write_page_title` safely copies a non-empty `std::wstring_view` into the
host-provided buffer and reports its UTF-16 length.

The raw C operation is `ERUI_Api::set_page_presentation(provider, page,
description)`. Zero-initialize an `ERUI_PagePresentationDesc`, set `size`, and
optionally fill `menu_title`, `page_title`, `formatter`, and `user_data`.
`ERUI_PageTitleFormatter` receives an `ERUI_PageTitleFormatContext` containing
the provider/page handles, borrowed effective base title, and one-based
`page_number`/`page_count`. It writes at most `output_capacity` UTF-16 code
units (bounded by `ERUI_PAGE_TITLE_BUFFER_CAPACITY`) and stores the exact count
in `out_length`; no terminator is required.

Formatters run once per physical slice during startup compilation. The host
validates and caches a copy. A failure result, empty or malformed title, or
invalid length falls back to the default title instead of failing publication.
All context pointers and the output buffer are borrowed for one invocation.
Do not retain them, re-enter ERNativeUI, or throw across the C callback. The
formatter and `user_data` must remain valid until `commit_provider` (or the
C++ `register_menu` call) returns; the host does not call them after caching.

## Native modal alerts

After successful registration, a retained `erui::Registration` can queue a
native Elden Ring message with localized buttons and choose bottom or centered
presentation:

```cpp
void ERUI_CALL alert_closed(
    void*, ERUI_Result result, erui::AlertResponse response) noexcept {
    if (result == ERUI_OK && response == erui::AlertResponse::primary) {
        // The player selected YES.
    }
}

erui::AlertOptions options{};
options.buttons = erui::AlertButtons::yes_no;
options.placement = erui::AlertPlacement::center;

const ERUI_Result queued = registration.alert(
    L"Apply the recommended settings?", options, &alert_closed);
```

`AlertOptions` defaults to `AlertButtons::ok` and
`AlertPlacement::bottom`. The convenience overload without options therefore
queues an ordinary bottom OK notice.

The available layouts are `ok`, `cancel`, `yes`, `no`, `ok_cancel`,
`yes_no`, and `dismiss_only`. Their normalized successful responses are:

- A one-button alert reports `AlertResponse::primary` for every successful
  close, including Back.
- A two-button alert reports `primary` for its left button and `secondary` for
  its right button or Back.
- A no-button `dismiss_only` alert reports `dismissed`.
- An operational failure or untrustworthy native result reports `none`.

`ERUI_OK` from `alert()` means the request entered the global bounded FIFO; it
does not mean the dialog has already closed. The optional completion callback
runs later on the host worker with a separate completion result and response.
Requests from all providers are displayed one at a time and in FIFO order.
ERNativeUI never replaces a game-owned blocking dialog, and it suppresses page
actions and the page's Back path only while its exact native task owns the
blocking slot. Back remains available to the popup as a documented response.

The raw C operation is `ERUI_Api::enqueue_alert(provider, description)`.
Zero-initialize an `ERUI_AlertDesc`, set `size`, provide a non-empty UTF-16
`message`, and set `buttons` from `ERUI_AlertButtons` plus `placement` from
`ERUI_AlertPlacement`. Optionally set `callback` plus `user_data`. The callback
receives `ERUI_AlertResponse` in addition to its `ERUI_Result`. The host copies
the message before returning; callback metadata must remain valid until
completion. `user_data` must be null when `callback` is null. `flags` must be
zero.

If the current game build does not expose every validated interface required
for a safe modal alert, the operation returns `ERUI_NOT_SUPPORTED` without
disabling ordinary registered menus.

## Callback contract

```cpp
using ButtonCallback =
    void (ERUI_CALL*)(void* user_data) noexcept;

using ValueChangedCallback =
    void (ERUI_CALL*)(void* user_data, std::uint8_t value) noexcept;

using AlertCallback =
    void (ERUI_CALL*)(
        void* user_data,
        ERUI_Result result,
        erui::AlertResponse response) noexcept;
```

- Button callbacks execute on Elden Ring's UI thread.
- Value callbacks execute on the host polling worker.
- Alert callbacks execute asynchronously on the host worker after dismissal or
  presentation failure; they never execute inside `enqueue_alert`.
- Callbacks execute without the registry mutex held.
- Row callback code and `user_data` must remain valid until process exit.
- Alert callback code and `user_data` must remain valid until completion.
- Callback functions must belong to `owner_module`; row callbacks are
  validated at commit and alert callbacks when queued.
- No exception may cross the C ABI.
- A successfully committed provider module is pinned until process exit.

## Raw C workflow

1. Find `ERNativeUI.dll` with `GetModuleHandleW`; do not call `LoadLibrary`.
2. Resolve the sole export, `ERUI_GetApi`, with `GetProcAddress`.
3. Zero an `ERUI_Api`, set `size` to `sizeof(ERUI_Api)`, and request the
   version compiled into the header.
4. Verify the returned `api_version`, capabilities, and required pointers.
5. Call `register_provider` to obtain a provider draft and root page.
6. Add rows and child pages with zero-initialized, size-prefixed descriptors.
7. Call `commit_provider`, or call `abort_provider` after any error.

API 1.0 requires at least `ERUI_API_V1_0_SIZE` bytes. The host writes no more
than the caller-provided table size. The current host accepts the exact
`ERUI_API_VERSION_1_0` request; it does not infer compatibility or clamp an
unknown same-major version. Put the returned 1.0 version in
`ERUI_ProviderDesc.api_version`.

`commit_provider` may block for the remainder of the configured startup
window plus native installation. Never call it from `DllMain`; use a worker.
If the absolute deadline closes an in-flight draft, mutation/commit returns
`ERUI_REGISTRATION_CLOSED`, but `abort_provider` remains available for cleanup.

## Descriptors and ownership

`ERUI_StringView` is UTF-8 for stable machine identifiers.
`ERUI_Utf16View` is `uint16_t` UTF-16 for visible text. Views do not need a
terminator; the host copies them synchronously. Embedded NUL and malformed or
oversized input are rejected.

Every v1 descriptor must include its complete v1 prefix. Unknown appended tail
fields are ignored. Flags and reserved fields must be zero. All input handles
are globally unique, opaque 64-bit values; zero is always invalid.

The `enabled` field is passed to Elden Ring for toggles/sliders, which remain
visible but disabled. Choice descriptors have no enabled field. The native
action-row constructor has no disabled state, so buttons and submenus with
`enabled == 0` are omitted from materialization and pagination.

## Results

```text
ERUI_OK
ERUI_INVALID_ARGUMENT
ERUI_HOST_NOT_READY
ERUI_HOST_FAILED
ERUI_UNSUPPORTED_VERSION
ERUI_DUPLICATE_PROVIDER_ID
ERUI_INVALID_HANDLE
ERUI_ALREADY_COMMITTED
ERUI_REGISTRATION_CLOSED
ERUI_OUT_OF_MEMORY
ERUI_CALLBACK_REJECTED
ERUI_INTERNAL_ERROR
ERUI_QUEUE_FULL
ERUI_NOT_SUPPORTED
```

An absent DLL or missing export is necessarily a wrapper-side error because no
host exists to return a C result. `ERUI_QUEUE_FULL` means the bounded global
alert FIFO cannot accept another request. `ERUI_NOT_SUPPORTED` means an
optional operation such as native alerts is unavailable for the current game
build; registered menu rows remain usable.

## Ordering, bounds, and publication

Committed providers are ordered by ascending `root_priority`, then provider
ID, then registration ordinal. Rows retain provider-local insertion order.

ABI v1 bounds one process to 256 providers, 256 pages per provider, 4096 rows
per provider, 255 provider-ID bytes, and 4096 UTF-16 units per text field.

The registry freezes when provider activity has been quiet with no open draft
for `RegistrationQuietMs`, or at `RegistrationMaxWaitMs`. Later topology
changes are rejected. Host-owned row values remain readable and writable after
publication unless native installation failed.

See [ABI_STABILITY.md](ABI_STABILITY.md),
[ARCHITECTURE.md](ARCHITECTURE.md), and the
[client template](../examples/template).
