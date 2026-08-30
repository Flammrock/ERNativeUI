// ERNativeUI client-mod template
//
// Rename the target/folder and, most importantly, replace the provider ID
// below with a stable reverse-DNS identifier belonging to your mod.
//
// This DLL does not link to ERNativeUI.dll. The header discovers the host with
// GetModuleHandle/GetProcAddress, so your actual mod can continue working when
// ERNativeUI is not installed.

#include <ernativeui/ERNativeUI.hpp>

#include <Windows.h>

#include <atomic>
#include <cstdint>
#include <iterator>

namespace {

HMODULE g_this_module{};
erui::Registration g_registration{};

struct LocalizedText {
    const wchar_t* display_name;
    const wchar_t* feature_enabled;
    const wchar_t* feature_help;
};

LocalizedText text_for(const erui::LanguageInfo& language) noexcept {
    // Add every locale shipped by your mod here. A valid identifier that a
    // newer/community locale introduced remains available in
    // language.identifier even when language.known is unknown.
    if (language.known == erui::GameLanguage::french) {
        return {L"Mon Mod", L"Fonction activée",
            L"Active ou désactive la fonction de ce mod."};
    }
    // English is the recommended fallback for UNKNOWN and unavailable.
    return {L"My Mod", L"Feature Enabled",
        L"Enable or disable this mod's feature."};
}

LocalizedText g_text = text_for({});

// ERNativeUI owns the byte that Elden Ring reads. Your mod keeps whatever
// application state it needs and receives changes through a noexcept callback.
std::atomic_bool g_feature_enabled{true};
std::atomic<std::uint8_t> g_strength{50};
std::atomic<std::uint8_t> g_mode{1};
std::atomic<std::uint8_t> g_popup_mode{1};

void log(const wchar_t* text) noexcept {
    OutputDebugStringW(L"[MyERNativeUIMod] ");
    OutputDebugStringW(text);
    OutputDebugStringW(L"\n");
}

// Callbacks must remain valid for the process lifetime and must never throw.
// Toggle/slider callbacks currently run on ERNativeUI's polling worker.
void ERUI_CALL enabled_changed(void*, std::uint8_t value) noexcept {
    g_feature_enabled.store(value != 0, std::memory_order_release);
    // Persist the new value to your own INI/configuration here if desired.
}

void ERUI_CALL strength_changed(void*, std::uint8_t value) noexcept {
    g_strength.store(value, std::memory_order_release);
}

void ERUI_CALL mode_changed(void*, std::uint8_t index) noexcept {
    g_mode.store(index, std::memory_order_release);
}

void ERUI_CALL popup_mode_changed(void*, std::uint8_t index) noexcept {
    g_popup_mode.store(index, std::memory_order_release);
}

// Button callbacks run synchronously on Elden Ring's UI thread. Keep them
// short; hand expensive work to your own worker when necessary.
void apply_settings() noexcept {
    log(L"Apply Settings was selected.");
}

// Alert completions are dispatched asynchronously on ERNativeUI's worker,
// never inside Registration::alert() or Elden Ring's native UI hook.
void ERUI_CALL message_closed(
    void*, ERUI_Result result, erui::AlertResponse response) noexcept {
    if (result != ERUI_OK) {
        // response is AlertResponse::none when no trustworthy native choice
        // was available because presentation failed.
        log(L"ERNativeUI could not present the native message.");
    } else if (response == erui::AlertResponse::primary) {
        log(L"The player selected YES.");
    } else {
        // For a two-button layout, the right button and Back are secondary.
        log(L"The player selected NO or pressed Back.");
    }
}

void show_native_message() noexcept {
    erui::AlertOptions options{};
    options.buttons = erui::AlertButtons::yes_no;
    options.placement = erui::AlertPlacement::center;

    const ERUI_Result result = g_registration.alert(
        L"Enable the example choice?",
        options,
        &message_closed);
    if (result != ERUI_OK) {
        log(L"The native message request was rejected.");
    }
}

DWORD WINAPI initialize_mod(void*) noexcept {
    // Do discovery and registration here, after DllMain has returned. Calling
    // other DLLs while Windows holds the loader lock is unsafe.
    const erui::LanguageInfo language = erui::query_game_language();
    g_text = text_for(language);

    erui::ProviderOptions options{};
    options.provider_id = "com.example.my-elden-ring-mod"; // CHANGE THIS.
    options.display_name = g_text.display_name;
    options.owner_module = g_this_module;
    options.root_priority = 100; // Lower-priority numbers appear first.

    const auto registration = erui::register_menu(options, [](erui::Menu& menu) {
        auto root = menu.root();

        root.add_toggle(
            g_text.feature_enabled,
            g_text.feature_help,
            g_feature_enabled.load(std::memory_order_acquire) ? 1u : 0u,
            &enabled_changed);

        erui::SliderOptions strength{};
        strength.minimum = 0;
        strength.maximum = 100;
        strength.step = 5;
        strength.initial_value = g_strength.load(std::memory_order_acquire);
        root.add_slider(
            L"Strength",
            L"Adjust this mod's effect strength.",
            strength,
            &strength_changed);

        // Both choice rows accept 1..32 custom strings and use zero-based
        // indices. The host copies this array and every string before either
        // builder call returns. Inline choices use left/right input directly.
        const std::wstring_view modes[] = {L"Off", L"Balanced", L"Strong"};
        erui::ChoiceOptions mode{};
        mode.values = modes;
        mode.count = std::size(modes);
        mode.initial_index = g_mode.load(std::memory_order_acquire);
        root.add_inline_choice(
            L"Inline Effect Mode",
            L"Change this preset directly with left and right input.",
            mode,
            &mode_changed);

        // Popup choices render as action rows and open Elden Ring's native
        // selection list. The callback runs only after a confirmed change.
        mode.initial_index = g_popup_mode.load(std::memory_order_acquire);
        root.add_popup_choice(
            L"Popup Effect Mode",
            L"Open a native list containing this mod's named presets.",
            mode,
            &popup_mode_changed);

        root.add_button<&apply_settings>(
            L"Apply Settings",
            L"Apply the current configuration immediately.");

        root.add_button<&show_native_message>(
            L"Show Native Message",
            L"Display a centered native YES/NO message and observe its response.");

        // Submenus are logical pages. Add as many rows as you need; the host
        // inserts Previous/Next rows and manages the native page stack.
        auto advanced = root.add_submenu(
            L"Advanced Settings",
            L"Open additional settings.",
            L"My Mod - Advanced",
            L"Advanced configuration for My Mod.");

        // Optional presentation overrides apply only to provider-owned
        // submenus. The outer heading becomes "My Mod - Advanced" and the
        // logical base title becomes "Advanced Settings". If pagination is
        // needed, the default physical titles are
        // "Advanced Settings (n/t)". An empty outer-heading override
        // would use options.display_name. The shared root cannot be changed.
        advanced.set_presentation(
            L"My Mod - Advanced", L"Advanced Settings");

        advanced.add_button<&apply_settings>(
            L"Apply Advanced Settings",
            L"Apply the advanced configuration.");
    });

    if (!registration) {
        // Missing host, incompatible API, duplicate IDs and closed startup
        // registration are all reported here without crashing your mod.
        log(registration.error().message().c_str());
        return 1;
    }

    g_registration = registration.value();
    log(L"ERNativeUI menu registered successfully.");
    return 0;
}

} // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_this_module = module;
        DisableThreadLibraryCalls(module);
        if (HANDLE thread = CreateThread(
                nullptr, 0, &initialize_mod, nullptr, 0, nullptr)) {
            CloseHandle(thread);
        } else {
            log(L"Could not create the initialization thread.");
        }
    }
    // Registered callback code is pinned by the host. Hot unloading is not
    // supported; let Mod Engine unload everything when the process exits.
    return TRUE;
}
