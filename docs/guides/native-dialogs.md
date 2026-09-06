# Native dialogs

`erui::Registration::alert()` displays a message through Elden Ring's native
dialog system. The game owns the visual presentation, button captions, focus,
and Back behavior; the client mod owns and supplies the message.

This guide describes the current API 1.1 C++17 surface. Alerts first appeared
in API 1.0 and remain available to any client built with the 1.0 SDK. A client
that chooses API 1.1 uses its explicit connection lifecycle. API versions and
ERNativeUI release versions are independent. See the [feature matrix](../features.md)
and [versioning guide](../versioning.md).

![A bottom-positioned ERNativeUI native alert with one OK button](../assets/images/dialogs/native-alert-ok-bottom.png)

*The simplest alert uses Elden Ring's bottom-positioned message and native OK
button.*

## Minimal example

Keep the successful `erui::Registration` returned after menu registration.
Any later callback can use it to queue a native greeting:

```cpp
#include <ernativeui/ERNativeUI.hpp>

erui::Registration g_registration{};

void show_greeting() noexcept
{
    g_registration.alert(L"Hello, Tarnished!");
}
```

The convenience overload uses one **OK** button at the bottom and no
completion callback. ERNativeUI copies the message before `alert()` returns.
The [Button guide](controls/button.md) shows how to call `show_greeting()` from
a menu row, while the [first-mod guide](../getting-started/first-mod.md) shows
where `g_registration` comes from.

For production error handling, store the returned `ERUI_Result`. `ERUI_OK`
means the request entered the queue; it does not mean the dialog is already
visible or that the player has closed it.

## Supported layouts

`erui::AlertOptions` controls the native buttons and vertical placement:

```cpp
erui::AlertOptions options{};
options.buttons = erui::AlertButtons::yes_no;
options.placement = erui::AlertPlacement::center;
```

The defaults are `AlertButtons::ok` and `AlertPlacement::bottom`.

| `AlertButtons` value | Visible buttons |
|---|---|
| `ok` | One **OK** button |
| `cancel` | One **CANCEL** button |
| `yes` | One **YES** button |
| `no` | One **NO** button |
| `ok_cancel` | Left **OK**, right **CANCEL** |
| `yes_no` | Left **YES**, right **NO** |
| `dismiss_only` | No visible button; the player can still dismiss the message |

`AlertPlacement::bottom` uses the Church-of-Vows-style lower presentation.
`AlertPlacement::center` uses the centered presentation.

There is currently no public dialog title, custom button caption, icon, text
field, arbitrary button count, or client-defined visual style. Elden Ring
localizes the native button captions. A mod must translate its own message;
see [Game language and localization](localization.md).

## Receive the player's response

Supply a completion callback when the mod needs to know how the dialog closed:

```cpp
void ERUI_CALL notice_closed(
    void*,
    ERUI_Result result,
    erui::AlertResponse response) noexcept
{
    if (result != ERUI_OK) {
        // The accepted request could not complete normally.
        return;
    }

    if (response == erui::AlertResponse::primary) {
        request_apply_on_the_mods_required_thread();
    }
}

void show_notice() noexcept
{
    erui::AlertOptions options{};
    options.buttons = erui::AlertButtons::yes_no;
    options.placement = erui::AlertPlacement::center;

    const ERUI_Result queued = g_registration.alert(
        L"Apply the recommended settings?",
        options,
        &notice_closed);

    if (queued != ERUI_OK) {
        // Log or use the mod's own non-native fallback notification.
    }
}
```

The completion callback crosses the stable C ABI boundary, so its declaration
uses `ERUI_CALL`. It runs asynchronously on the ERNativeUI host worker, not
inside `Registration::alert()` and not on Elden Ring's UI thread.

## Interpret responses by position

Responses describe positions rather than captions:

| Requested buttons | `primary` | `secondary` | Back/Cancel |
|---|---|---|---|
| `ok`, `cancel`, `yes`, or `no` | The only button | Not used | `primary` |
| `ok_cancel` | Left **OK** | Right **CANCEL** | `secondary` |
| `yes_no` | Left **YES** | Right **NO** | `secondary` |
| `dismiss_only` | Not used | Not used | `dismissed` |

![A bottom-positioned native dialog with separate OK and CANCEL buttons](../assets/images/dialogs/native-alert-ok-cancel-bottom.png)

*Two-button alerts report the left button as `primary` and the right button as
`secondary`, independently of their localized captions.*

`AlertResponse::none` accompanies an operational failure or a completion for
which no trustworthy native response was available. Inspect the callback's
`ERUI_Result` before interpreting its response.

## Queue and modal-input behavior

Requests from all providers are presented one at a time in FIFO order. An
ERNativeUI request waits instead of replacing a game-owned blocking dialog.
`ERUI_QUEUE_FULL` means the bounded process-wide queue could not accept
another request.

While an ERNativeUI dialog owns modal input, the underlying native page cannot
move or follow its Back path, and ERNativeUI input-action dispatch is
suppressed. The popup's own Confirm and Back actions remain live. This focus
contract applies at the title screen, in character creation, in settings, and
in ordinary gameplay menus; it is not implemented as a page-specific key
filter.

## Lifetime, threading, and errors

- Callback code and any supplied `user_data` must remain valid until that
  request completes. Do not point `user_data` at a short-lived stack object.
- A successfully committed provider DLL is pinned until process exit because
  the host retains callback addresses. Hot-unloading is unsupported.
- Callbacks must be `noexcept`, must not block while waiting for the dialog
  system, and must synchronize access to shared client state.
- Retry `ERUI_QUEUE_FULL` later only when that behavior makes sense for the
  mod.
- `ERUI_NOT_SUPPORTED` means the negotiated host or validated native boundary
  cannot provide alerts. There is no automatic visual fallback.
- Invalid or empty messages, invalid enums, and non-null `user_data` without a
  callback return `ERUI_INVALID_ARGUMENT`.
- Treat other non-`ERUI_OK` results as failure of this optional UI operation,
  not as a reason to disable unrelated gameplay code.

## Strict C API

The strict C equivalent uses the negotiated `ERUI_Api` table's
`enqueue_alert` member with a size-initialized `ERUI_AlertDesc`. Keep reserved
fields zero and retain the same callback and pointer lifetimes. The
[strict-C header](https://github.com/Flammrock/ERNativeUI/blob/main/include/ernativeui/erui.h)
is the normative ABI contract; the
[C++17 wrapper](https://github.com/Flammrock/ERNativeUI/blob/main/include/ernativeui/ERNativeUI.hpp)
provides the client-side conveniences shown here.

## Research trail

The public API hides Elden Ring's private builder numbers, response payloads,
task ownership, and focus hooks. The
[native dialog case study](../research/case-studies/native-dialogs.md) records
the reproducible evidence behind those boundaries. It is supporting research,
not an additional client API.

Return to the [mod-author guides](README.md) or continue with
[Game language and localization](localization.md).
