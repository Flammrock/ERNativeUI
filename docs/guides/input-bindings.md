# Input bindings

An input binding connects one action in a mod to a controller button, keyboard
key, or mouse button that the player can change in Elden Ring's settings.

This guide creates a concrete action named **Show Test Message**. Its callback
opens the `Hello, Tarnished!` dialog. If the action is initially bound to
Space, Space opens the dialog. If the player reassigns it to R, R opens the
same dialog instead.

ERNativeUI hooks Elden Ring's native UI and input functions to:

- listen for controller, keyboard, and mouse input through Elden Ring's input
  update loop;
- add configurable mod actions to the native **Button Settings** and
  **Keyboard/Mouse Settings** screens; and
- report when the player assigns or clears a binding, so the client mod can
  save that change.

Therefore, a mod can listen for player input, expose configurable bindings,
and make the player's choices persistent.

Input bindings require API 1.1, a successful `erui::connect()`, and the
`input_bindings` capability. They do not require a binding-screen GFX patch.

![A mod section in Elden Ring's controller binding screen](../assets/images/input-bindings/controller-bindings-section.png)

*Controller actions appear in **Button Settings** under **Controller
Settings**.*

![A mod section in Elden Ring's keyboard and mouse binding screen](../assets/images/input-bindings/keyboard-mouse-bindings-section.png)

*Keyboard and mouse actions appear in **Keyboard/Mouse Settings**.*

The examples use the header-only C++17 wrapper. Strict-C clients can learn the
same concepts here, then use the [Strict-C map](#strict-c-map).

## Contents

- [Before you start](#before-you-start)
- [1. How do I create my first binding?](#1-how-do-i-create-my-first-binding)
- [2. How do I choose devices and defaults?](#2-how-do-i-choose-devices-and-defaults)
- [3. How do I know when the player presses it?](#3-how-do-i-know-when-the-player-presses-it)
- [4. Which keys and buttons can I use?](#4-which-keys-and-buttons-can-i-use)
- [5. Can I add several actions and sections?](#5-can-i-add-several-actions-and-sections)
- [6. How do I save bindings with ERNativeUI Storage?](#6-how-do-i-save-bindings-with-ernativeui-storage)
- [7. How do I receive binding changes or use my own storage?](#7-how-do-i-receive-binding-changes-or-use-my-own-storage)
- [8. How do I query or change a live action?](#8-how-do-i-query-or-change-a-live-action)
- [API names at a glance](#api-names-at-a-glance)
- [Strict-C map](#strict-c-map)
- [Runtime rules](#runtime-rules)
- [How this feature was found](#how-this-feature-was-found)

## Before you start

Complete the [Hello, Tarnished! guide](../getting-started/first-mod.md) first.
Keep its `DllMain`, worker thread, `erui::connect()`, `ProviderOptions`,
`show_greeting`, `g_registration`, and `g_registration_ready` code.

The first example below replaces only that guide's inline menu-builder lambda
with a named `build_erui_content` function.

## 1. How do I create my first binding?

Add these functions after the existing `show_greeting` function and before
`initialize_mod`:

```cpp
// ERNativeUI calls this function when Show Test Message is pressed.
void show_test_message(
    const erui::ActionActivation&) noexcept
{
    // Reuse the native dialog from the Hello, Tarnished! guide.
    show_greeting();
}

// ERNativeUI passes menu to this function during register_menu().
void build_erui_content(erui::Menu& menu) noexcept
{
    // Keep the Game Options button from the first guide.
    menu.root().add_button<&show_greeting>(
        L"Hello, Tarnished!",
        L"Show a native greeting dialog.");

    if (!menu.supports(erui::Capability::input_bindings)) {
        return;
    }

    // Entry point for this provider's input sections and actions.
    erui::InputBindings bindings = menu.input_bindings();

    // Visible heading in Elden Ring's binding screen.
    erui::InputSection section = bindings.add_section(L"My Mod");

    // This action supports keyboard input and starts on Space.
    erui::ActionInputs default_inputs =
        erui::inputs::keyboard(erui::KeyboardKey::space);

    // Add the configurable Show Test Message row under My Mod.
    erui::InputAction action =
        section.add_action<&show_test_message>(
            "show-test-message",
            L"Show Test Message",
            default_inputs);

    if (!action) {
        OutputDebugStringW(L"[My Mod] Could not add the input action.\n");
    }
}
```

Then replace the first guide's `register_menu(..., lambda)` statement with:

```cpp
const erui::RegistrationResult registration =
    connection.value().register_menu(options, &build_erui_content);
```

Keep the success check and the assignments to `g_registration` and
`g_registration_ready` that follow it in the first guide.

Run the game, open **System > Keyboard/Mouse Settings**, and find the **My
Mod** section. Press Space: the `Hello, Tarnished!` dialog should open. Change
the row to another key in Elden Ring, then confirm that the new key opens it.

This is the call that creates the binding:

```cpp
erui::InputAction action =
    section.add_action<&show_test_message>(
        "show-test-message",
        L"Show Test Message",
        default_inputs);
```

| Part | Required type | Meaning |
|---|---|---|
| `show_test_message` | `void(const erui::ActionActivation&) noexcept` | Function ERNativeUI calls when the action is pressed. Put its name between `<` and `>` without `()`. |
| `"show-test-message"` | `std::string_view` | Stable provider-wide action ID used by code and saved configuration. Do not localize it. |
| `L"Show Test Message"` | `std::wstring_view` | Label visible to the player. It may be localized. |
| `default_inputs` | `const erui::ActionInputs&` | Supported devices and initial assignments. |
| returned `action` | `erui::InputAction` | Live handle used to check creation or change the binding later. |

An action ID must contain 1 through 255 ASCII letters, digits, `.`, `_`, or
`-`. It must be unique inside the provider and should remain stable between
mod releases.

## 2. How do I choose devices and defaults?

`ActionInputs` answers two questions for one action:

1. Which devices does this action support?
2. What is its initial assignment on each supported device?

Each recipe below replaces the `default_inputs` and `add_action` block inside
`build_erui_content` from section 1. The existing `section` and
`show_test_message` callback are reused.

### Controller only

```cpp
erui::ActionInputs default_inputs =
    erui::inputs::controller(erui::ControllerButton::face_north);

erui::InputAction action =
    section.add_action<&show_test_message>(
        "show-test-message", L"Show Test Message", default_inputs);
```

The action appears only in **Button Settings**. Its initial assignment is Y on
an Xbox controller or Triangle on a PlayStation controller.

### Keyboard only, with no mouse binding

```cpp
erui::ActionInputs default_inputs =
    erui::inputs::keyboard(erui::KeyboardKey::space);

erui::InputAction action =
    section.add_action<&show_test_message>(
        "show-test-message", L"Show Test Message", default_inputs);
```

The action appears in **Keyboard/Mouse Settings**. It has no editable mouse
assignment.

### Mouse only

```cpp
erui::ActionInputs default_inputs =
    erui::inputs::mouse(erui::MouseButton::button4);

erui::InputAction action =
    section.add_action<&show_test_message>(
        "show-test-message", L"Show Test Message", default_inputs);
```

The action appears in **Keyboard/Mouse Settings** with Mouse Button 4 and no
keyboard assignment.

### Keyboard and mouse

```cpp
erui::ActionInputs default_inputs =
    erui::inputs::keyboard_mouse(
        erui::KeyboardKey::space,
        erui::MouseButton::button4);

erui::InputAction action =
    section.add_action<&show_test_message>(
        "show-test-message", L"Show Test Message", default_inputs);
```

Either Space or Mouse Button 4 runs the same callback.

### Other combinations

Build combinations by chaining the device methods, then pass the completed
value to `add_action`:

```cpp
// Controller + keyboard; mouse is not supported.
erui::ActionInputs default_inputs{};
default_inputs.controller(erui::ControllerButton::face_north);
default_inputs.keyboard(erui::KeyboardKey::space);

erui::InputAction action =
    section.add_action<&show_test_message>(
        "show-test-message", L"Show Test Message", default_inputs);
```

| Desired devices | Build `default_inputs` with |
|---|---|
| Controller + keyboard | `.controller(button).keyboard(key)` |
| Controller + mouse | `.controller(button).mouse(mouse_button)` |
| Controller + keyboard + mouse | `.controller(button).keyboard(key).mouse(mouse_button)` |
| All three supported but initially unbound | `erui::ActionInputs::all()` |

`button`, `key`, and `mouse_button` in the table mean the matching public enum
value, such as `ControllerButton::face_north`, `KeyboardKey::space`, and
`MouseButton::button4`.

### Absent, unbound, and bound

Every device slot has one of three states:

| State | How to declare it | Result |
|---|---|---|
| Absent | Do not call that device method. | The action does not support that device. |
| Unbound | Call it without a value, such as `inputs.mouse()`. | The device is supported and starts as `----`. |
| Bound | Pass a value, such as `inputs.keyboard(KeyboardKey::space)`. | The device is supported with that initial assignment. |

This example contains all three states:

```cpp
erui::ActionInputs default_inputs{};
default_inputs.keyboard(erui::KeyboardKey::space); // Bound to Space.
default_inputs.mouse();                            // Supported, but ----.
// No controller() call: controller is absent and unsupported.

erui::InputAction action =
    section.add_action<&show_test_message>(
        "show-test-message", L"Show Test Message", default_inputs);
```

The devices used by every action in a section determine where that section is
shown:

| A section contains at least one action supporting... | It appears in... |
|---|---|
| Controller | **Button Settings** under **Controller Settings** |
| Keyboard or mouse | **Keyboard/Mouse Settings** |

A section supporting both groups appears in both screens. An empty section is
not displayed.

## 3. How do I know when the player presses it?

ERNativeUI calls the function registered with `add_action`. The callback means
the named action was activated; the mod does not poll Space, Y/Triangle, or a
mouse button itself. Remapping changes which input reaches the same callback.

The minimal callback from section 1 ignores its event argument:

```cpp
void show_test_message(
    const erui::ActionActivation&) noexcept
{
    show_greeting();
}
```

### Find which device activated it

Use `ActionActivation` only when the callback cares whether the controller,
keyboard, or mouse assignment fired:

```cpp
void show_test_message(
    const erui::ActionActivation& activation) noexcept
{
    if (activation.includes(erui::InputDevice::controller)) {
        OutputDebugStringW(L"Activated by a controller.\n");
    }
    if (activation.includes(erui::InputDevice::keyboard)) {
        OutputDebugStringW(L"Activated by the keyboard.\n");
    }
    if (activation.includes(erui::InputDevice::mouse)) {
        OutputDebugStringW(L"Activated by the mouse.\n");
    }

    show_greeting();
}
```

This reports the device family, not a raw key code. The action may have been
remapped; its current exact assignment can be read through `InputAction` in
[section 8](#8-how-do-i-query-or-change-a-live-action).

### Access your mod state

Use the stateful overload when the callback needs a mod-owned object:

```cpp
struct ModState {
    std::atomic_bool action_requested{false};
};

ModState g_mod_state{};

void request_test_message(
    ModState& state,
    const erui::ActionActivation&) noexcept
{
    // Another part of the mod can consume this request safely.
    state.action_requested.store(true, std::memory_order_release);
}

// Inside build_erui_content(), after creating section:
erui::ActionInputs default_inputs =
    erui::inputs::keyboard(erui::KeyboardKey::space);

erui::InputAction action =
    section.add_action<&request_test_message>(
        "request-test-message",
        L"Request Test Message",
        default_inputs,
        g_mod_state);
```

The stateful callback must have this shape:

```cpp
void callback(
    ModState& state,
    const erui::ActionActivation& activation) noexcept;
```

ERNativeUI stores a pointer to the state passed as the last argument. That
object must remain alive for the process lifetime; do not pass a local variable
from `initialize_mod`. Callbacks run on the ERNativeUI worker, so shared state
must be thread-safe.

## 4. Which keys and buttons can I use?

Use these exhaustive API 1.1 tables:

- [controller buttons](../reference/input-values-and-codec.md#controller-buttons);
- [keyboard keys](../reference/input-values-and-codec.md#keyboard-keys); and
- [mouse buttons and wheel inputs](../reference/input-values-and-codec.md#mouse-buttons).

If a value is absent from those tables, it is not supported by API 1.1.

Controller face-button values describe positions, allowing Elden Ring to show
the correct glyph for the player's controller:

| Position | Xbox | PlayStation | C++ value |
|---|---|---|---|
| Lower | A | Cross | `erui::ControllerButton::face_south` |
| Right | B | Circle | `erui::ControllerButton::face_east` |
| Left | X | Square | `erui::ControllerButton::face_west` |
| Upper | Y | Triangle | `erui::ControllerButton::face_north` |

Use the named `KeyboardKey`, `MouseButton`, or `ControllerButton` values. Do
not copy Elden Ring's private numeric tokens into client code.

## 5. Can I add several actions and sections?

Yes. Call `add_section` for every visible group and `add_action` for every
operation the player may configure:

```cpp
void quick_heal_pressed(const erui::ActionActivation&) noexcept
{
    // Queue the mod's Quick Heal operation.
}

void toggle_overlay_pressed(const erui::ActionActivation&) noexcept
{
    // Queue the mod's Toggle Overlay operation.
}

void add_mod_bindings(erui::Menu& menu) noexcept
{
    if (!menu.supports(erui::Capability::input_bindings)) {
        return;
    }

    erui::InputBindings bindings = menu.input_bindings();

    erui::InputSection gameplay =
        bindings.add_section(L"My Mod - Gameplay");
    erui::InputAction quick_heal =
        gameplay.add_action<&quick_heal_pressed>(
            "quick-heal",
            L"Quick Heal",
            erui::inputs::controller(
                erui::ControllerButton::dpad_down));

    erui::InputSection interface_section =
        bindings.add_section(L"My Mod - Interface");
    erui::InputAction toggle_overlay =
        interface_section.add_action<&toggle_overlay_pressed>(
            "toggle-overlay",
            L"Toggle Overlay",
            erui::inputs::keyboard(erui::KeyboardKey::space));

    if (!quick_heal || !toggle_overlay) {
        OutputDebugStringW(L"[My Mod] Could not add every input action.\n");
    }
}
```

Each nonempty section becomes one visible group in every applicable native
screen. Its actions decide which screens contain it. Action IDs must be unique
across the whole provider, even when the actions belong to different sections.

## 6. How do I save bindings with ERNativeUI Storage?

Without persistence, player edits last only until the process ends. The
simplest solution uses ERNativeUI's host-owned Storage service.

Put this block in `build_erui_content` after the `bindings` and `action` values
from section 1 have been created:

```cpp
if (menu.supports(erui::Capability::storage)) {
    // Open this provider's default config file.
    // Its normal path is mods/<provider_id>/config.ini.
    erui::Storage config = menu.storage();

    // Read the file into the host's in-memory document.
    // A missing file loads successfully as an empty document.
    ERUI_Result load_result = config.load();
    if (load_result == ERUI_OK) {
        // This means the [bindings] INI section. It is unrelated to the
        // visible InputSection named "My Mod" in Elden Ring's UI.
        erui::StorageSection saved_bindings =
            config.section("bindings");

        // Load this action using the same stable ID passed to add_action().
        erui::StorageRead<erui::ActionInputs> saved =
            saved_bindings.get<erui::ActionInputs>("show-test-message");

        if (saved.found()) {
            // Restore the native row and runtime assignment.
            // bind() changes memory; it does not write the INI file.
            action.bind(saved.value());
        }

        // This DOES NOT save anything immediately.
        // It installs one handler for FUTURE player edits. When the player
        // assigns or clears any action, the handler updates [bindings] under
        // that action's stable ID and saves config.ini.
        bindings.persist_assignments_to(saved_bindings);
    }
}
```

The order matters because loading and watching solve different problems:

```text
Mod starts:          load file -> read saved value -> bind live action
Player edits later:  installed handler -> update [bindings] -> save file
```

One `persist_assignments_to(saved_bindings)` call covers every input action
registered by this provider. Call it once, after adding the actions. Do not
call it once per action or call it manually after each change.

After the player changes Space to R, the file can contain:

```ini
[bindings]
show-test-message=keyboard:key-r
```

Read the separate [Storage guide](storage.md) for paths, other value types,
errors, and manual save control. If the mod already owns an INI, JSON, or
database system, use the next section instead.

## 7. How do I receive binding changes or use my own storage?

There are two different callbacks:

| Event | Meaning |
|---|---|
| Action callback passed to `add_action` | The player **pressed** the configured action. |
| Callback passed to `on_assignments_changed` | The player **edited** an assignment in Elden Ring's binding screen. |

Choose exactly one provider-wide change strategy:

- `persist_assignments_to(...)` for automatic ERNativeUI Storage persistence;
  or
- `on_assignments_changed(...)` to inspect changes or save with another
  configuration backend.

### Receive assignment changes

This callback reports which stable action changed, which device slots changed,
and the assignments before and after the edit:

```cpp
void assignments_changed(
    const erui::AssignmentsChangedEvent& event) noexcept
{
    for (const erui::AssignmentChange change : event.changes()) {
        std::string_view action_id = change.action_id();
        erui::ActionInputs before = change.previous();
        erui::ActionInputs after = change.current();

        if (change.controller_changed()) {
            OutputDebugStringW(L"Controller assignment changed.\n");
        }
        if (change.keyboard_changed()) {
            OutputDebugStringW(L"Keyboard assignment changed.\n");
        }
        if (change.mouse_changed()) {
            OutputDebugStringW(L"Mouse assignment changed.\n");
        }

        // action_id is borrowed, but before and after are safe value copies.
        if (action_id == "show-test-message" && before != after) {
            OutputDebugStringW(L"Show Test Message was reassigned.\n");
        }
    }
}

// Inside build_erui_content(), after adding every action:
ERUI_Result handler_result =
    bindings.on_assignments_changed<&assignments_changed>();

if (handler_result != ERUI_OK) {
    OutputDebugStringW(L"[My Mod] Could not watch binding changes.\n");
}
```

`change.reason()` distinguishes `player_assignment` from `player_clear`.
`reset_to_defaults` is reserved for a native reset path. Programmatic calls
such as `action.bind()` and `action.reset_to_defaults()` are silent and do not
invoke this callback.

Only one assignments-changed handler may be installed per provider.

### Persist assignments with another configuration backend

Keep the mod's existing storage system and use ERNativeUI's header-only codec
to translate `ActionInputs` to and from stable, readable text. The codec does
not read or write files.

At startup, after `add_action` has returned `action`:

```cpp
// my_config represents the mod's existing INI/JSON API.
std::optional<std::string> saved_text =
    my_config.get("bindings", "show-test-message");

if (saved_text.has_value()) {
    std::optional<erui::ActionInputs> saved_inputs =
        erui::parse_action_inputs(saved_text.value());

    if (saved_inputs.has_value()) {
        // Apply the saved assignments to the native row and dispatcher.
        action.bind(saved_inputs.value());
    }
}
```

To save later player edits, use one custom change callback instead of
`persist_assignments_to(...)`:

```cpp
void save_assignment_changes(
    MyConfig& config,
    const erui::AssignmentsChangedEvent& event) noexcept
{
    for (const erui::AssignmentChange change : event.changes()) {
        std::optional<std::string> text =
            erui::format_action_inputs(change.current());

        if (text.has_value()) {
            // config.set() must copy action_id because the view is temporary.
            config.set(
                "bindings",
                change.action_id(),
                text.value());
        }
    }

    config.save();
}

// Inside build_erui_content(), after adding every action:
ERUI_Result handler_result =
    bindings.on_assignments_changed<&save_assignment_changes>(my_config);

if (handler_result != ERUI_OK) {
    OutputDebugStringW(L"[My Mod] Could not watch binding changes.\n");
}
```

`MyConfig`, `get`, `set`, and `save` are placeholders for the mod's own
configuration API. `my_config` must remain alive for the provider lifetime,
must be safe to use on the ERNativeUI worker, and must not let exceptions
escape this `noexcept` callback. The serialized value is ordinary text:

```ini
[bindings]
show-test-message=keyboard:key-r
```

This custom example stores the complete current assignment snapshot. Restoring
that snapshot later overrides the corresponding defaults declared by the new
mod version. If code changes or resets an action programmatically, save that
change explicitly because programmatic operations do not emit player-edit
events.

The codec functions work without `ERNativeUI.dll`; native rows, callbacks, and
`action.bind()` still require a successful host connection. See the
[codec reference](../reference/input-values-and-codec.md#canonical-text-format)
for the exact text grammar and result-preserving overloads.

## 8. How do I query or change a live action?

`InputAction` is the live handle returned by `add_action`. After registration
commits successfully, the action remains registered if the wrapper is
discarded. Retain it only when the mod needs to inspect or change that action
later.

For example, query its mouse slot:

```cpp
erui::ActionInputs current_inputs{};
ERUI_Result query_result = action.query_inputs(current_inputs);

if (query_result == ERUI_OK) {
    if (!current_inputs.mouse_supported()) {
        // Mouse is absent: this action has no editable mouse slot.
    } else {
        std::optional<erui::MouseButton> mouse =
            current_inputs.mouse_input();

        if (!mouse.has_value()) {
            // Mouse is supported but currently shown as ----.
        } else if (mouse.value() == erui::MouseButton::button4) {
            // Mouse Button 4 is currently assigned.
        }
    }
}
```

`MouseButton` is an enum naming one input; it is not a mouse object and has no
method such as `.click()`. `std::optional<MouseButton>` contains a value only
when that slot is bound.

The action created in section 1 supports only keyboard, so change that
supported slot through the same handle:

```cpp
ERUI_Result bind_result =
    action.devices().keyboard().bind(erui::KeyboardKey::space);

if (bind_result != ERUI_OK) {
    OutputDebugStringW(L"[My Mod] Could not change the keyboard binding.\n");
}
```

The matching `.controller()` and `.mouse()` accessors work only when that
device was declared as supported in the action's original `ActionInputs`.

| Operation | Effect |
|---|---|
| `action.query_inputs(output)` | Copies every current assignment into `output`. |
| `action.query_default_inputs(output)` | Copies the defaults declared by the mod. |
| `action.bind(inputs)` | Applies every present slot in `inputs`; absent slots remain unchanged. |
| `action.unbind()` | Clears every supported slot. |
| `action.reset_to_defaults()` | Restores all declared defaults. |
| `action.devices().keyboard()` | Reads or changes only the keyboard slot. Controller and mouse accessors work the same way. |

A successful programmatic change updates the native row and runtime input
dispatcher. It does not invoke the assignments-changed callback, so save it
explicitly if it should persist.

## API names at a glance

These names describe different parts of one workflow:

| Type | Plain meaning | When you need it |
|---|---|---|
| `InputBindings` | Registration entry point from `menu.input_bindings()`. | To add sections or choose one change handler. |
| `InputSection` | One visible heading in the native binding screens. | To add actions below that heading. |
| `ActionInputs` | Data describing supported devices and their assignments. | For defaults, saved values, queries, or updates. It contains no callback. |
| `InputAction` | Live handle returned by `add_action`. | To verify creation or inspect/change the registered action later. |
| `ActionActivation` | Temporary event passed when an action is pressed. | To identify the activating device family. |
| `AssignmentsChangedEvent` | Temporary batch passed when the player edits bindings. | To inspect or persist those edits. |

```text
ActionInputs defaults -> add_action -> InputAction live handle
Player presses action                 -> ActionActivation callback
Player edits binding screen           -> AssignmentsChangedEvent callback
```

## Strict-C map

Strict-C clients use the same model through the API 1.1 function table:

| C++ operation | Strict-C equivalent |
|---|---|
| `menu.input_bindings().add_section(...)` | `api->add_input_section(...)` with `ERUI_InputSectionDesc` |
| `section.add_action(...)` | `api->add_input_action(...)` with `ERUI_InputActionDesc` |
| Action callback | `ERUI_InputActionActivatedCallback` |
| `bindings.on_assignments_changed(...)` | `api->set_assignments_changed_handler(...)` |
| `action.query_inputs(...)` | `api->get_action_inputs(...)` |
| `action.bind(...)` | `api->set_action_inputs(...)` |
| Codec helpers | `ERUI_FormatActionInputs` and `ERUI_ParseActionInputs` |

Strict-C callbacks use `void* user_data`. Zero-initialize descriptors, set
their `size`, leave flags and reserved fields zero, and respect borrowed-view
lifetimes. The [strict-C header](https://github.com/Flammrock/ERNativeUI/blob/main/include/ernativeui/erui.h)
is the ABI contract; the
[Strict-C input reference](../reference/input-values-and-codec.md#strict-c-representation)
shows the data structures and codec.

## Runtime rules

- Activation and assignment-change callbacks run on the ERNativeUI worker.
  Keep them short, `noexcept`, and thread-safe.
- Activations are global: they can occur during gameplay, menus, the title
  flow, or character creation.
- ERNativeUI observes but does not consume the underlying input.
- Dispatch is suppressed while the player assigns a native binding, edits
  tracked native text, or uses an ERNativeUI-owned modal dialog.
- API 1.1 supports one controller, one keyboard, and one mouse assignment per
  action. Chords are not supported.
- Device support is fixed by the `ActionInputs` passed to `add_action`.
- Duplicate physical assignments are allowed across actions and providers.
- One provider may install one assignments-changed handler.
- `InputBindings` and `InputSection` are builder values. A retained
  `InputAction` remains usable after successful registration.
- Callback functions and callback state must remain valid for the process
  lifetime. Hot-unloading a committed provider is unsupported.

For exact limits and error behavior, read
[Lifecycle, errors, and limits](../reference/lifecycle-errors-and-limits.md#important-api-11-limits).

## How this feature was found

The [native input-bindings case study](../research/case-studies/input-bindings.md)
documents the native list builders, controller/keyboard/mouse token probes,
input-manager update hook, runtime dispatch, remap capture, and input-focus
suppression behind this API.

Return to the [mod-author guide index](README.md) or the
[documentation home](../README.md).
