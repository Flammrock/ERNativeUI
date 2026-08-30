#include <ernativeui/ERNativeUI.hpp>

#include "localization.hpp"

#include <Windows.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <string>

namespace {

HMODULE g_module{};
std::atomic<std::uint8_t> g_enabled{1};
std::atomic<std::uint8_t> g_intensity{50};
std::atomic<std::uint8_t> g_inline_preset{1};
std::atomic<std::uint8_t> g_popup_preset{1};
std::atomic<std::uint8_t> g_popup_binary{0};
std::atomic<std::uint8_t> g_popup_extended{3};
std::array<std::uint32_t, 32> g_action_numbers{};
erui::Registration g_registration{};
erui::RowHandle g_enabled_row{};

struct PopupVariant {
    std::wstring label{};
    std::wstring help{};
    std::wstring message{};
    erui::AlertOptions options{};
};

constexpr erui::AlertOptions popup_options(
    erui::AlertPlacement placement,
    erui::AlertButtons buttons) noexcept {
    erui::AlertOptions options{};
    options.buttons = buttons;
    options.placement = placement;
    return options;
}

std::array<PopupVariant, 14> g_popup_variants{};
const showcase::Text* g_text{};

void initialize_popup_variants(const showcase::Text& text) {
    constexpr std::array<erui::AlertButtons, 7> buttons{
        erui::AlertButtons::dismiss_only, erui::AlertButtons::ok,
        erui::AlertButtons::cancel, erui::AlertButtons::yes,
        erui::AlertButtons::no, erui::AlertButtons::ok_cancel,
        erui::AlertButtons::yes_no};
    constexpr std::array<const wchar_t*, 7> names{
        nullptr, L"OK", L"CANCEL", L"YES", L"NO",
        L"OK + CANCEL", L"YES + NO"};
    for (std::size_t placement = 0; placement < 2; ++placement) {
        for (std::size_t index = 0; index < buttons.size(); ++index) {
            PopupVariant& variant = g_popup_variants[placement * buttons.size() + index];
            variant.label = placement == 0 ? text.bottom : text.center;
            variant.label += L": ";
            variant.label += index == 0 ? text.dismiss_only : names[index];
            variant.help = text.popup_help;
            variant.message = text.popup_message;
            variant.options = popup_options(
                placement == 0 ? erui::AlertPlacement::bottom
                               : erui::AlertPlacement::center,
                buttons[index]);
        }
    }
}

void debug(const wchar_t* message) noexcept {
    OutputDebugStringW(L"[TarnishedUIShowcase] ");
    OutputDebugStringW(message);
    OutputDebugStringW(L"\n");
}

void ERUI_CALL value_changed(void* context, std::uint8_t value) noexcept {
    static_cast<std::atomic<std::uint8_t>*>(context)->store(
        value, std::memory_order_release);
}

void ERUI_CALL action_pressed(void* context) noexcept {
    const auto number = context ? *static_cast<std::uint32_t*>(context) : 0u;
    wchar_t text[96]{};
    wsprintfW(text, L"Large Settings action %u was pressed.", number);
    debug(text);
}

const wchar_t* response_name(erui::AlertResponse response) noexcept {
    switch (response) {
    case erui::AlertResponse::none: return L"none";
    case erui::AlertResponse::primary: return L"primary";
    case erui::AlertResponse::secondary: return L"secondary";
    case erui::AlertResponse::dismissed: return L"dismissed";
    default: return L"unknown";
    }
}

const wchar_t* result_name(ERUI_Result result) noexcept {
    switch (result) {
    case ERUI_OK: return L"success";
    case ERUI_INVALID_ARGUMENT: return L"invalid argument";
    case ERUI_HOST_NOT_READY: return L"host not ready";
    case ERUI_HOST_FAILED: return L"host failed";
    case ERUI_UNSUPPORTED_VERSION: return L"unsupported API version";
    case ERUI_DUPLICATE_PROVIDER_ID: return L"duplicate provider ID";
    case ERUI_INVALID_HANDLE: return L"invalid handle";
    case ERUI_ALREADY_COMMITTED: return L"provider already committed";
    case ERUI_REGISTRATION_CLOSED: return L"registration closed";
    case ERUI_OUT_OF_MEMORY: return L"out of memory";
    case ERUI_CALLBACK_REJECTED: return L"callback rejected";
    case ERUI_INTERNAL_ERROR: return L"internal host error";
    case ERUI_QUEUE_FULL: return L"alert queue full";
    case ERUI_NOT_SUPPORTED: return L"not supported";
    default: return L"unknown result";
    }
}

const wchar_t* selected_action_name(
    erui::AlertButtons buttons,
    erui::AlertResponse response) noexcept {
    if (response == erui::AlertResponse::dismissed) return L"dismissed";
    if (response == erui::AlertResponse::none) return L"none";
    if (response == erui::AlertResponse::secondary) {
        switch (buttons) {
        case erui::AlertButtons::ok_cancel: return L"CANCEL";
        case erui::AlertButtons::yes_no: return L"NO";
        default: return L"unexpected secondary action";
        }
    }
    if (response != erui::AlertResponse::primary) return L"unknown";
    switch (buttons) {
    case erui::AlertButtons::ok:
    case erui::AlertButtons::ok_cancel:
        return L"OK";
    case erui::AlertButtons::cancel:
        return L"CANCEL";
    case erui::AlertButtons::yes:
    case erui::AlertButtons::yes_no:
        return L"YES";
    case erui::AlertButtons::no:
        return L"NO";
    case erui::AlertButtons::dismiss_only:
        return L"unexpected primary action";
    default:
        return L"unknown";
    }
}

void log_alert_completion(
    const wchar_t* label,
    erui::AlertButtons buttons,
    ERUI_Result result,
    erui::AlertResponse response) noexcept {
    try {
        std::wstring text = L"Alert \"";
        text += label ? label : L"<unknown>";
        text += L"\" completed: result=";
        text += result_name(result);
        text += L", response=";
        text += response_name(response);
        text += L", action=";
        text += selected_action_name(buttons, response);
        text += L".";
        debug(text.c_str());
    } catch (...) {
        debug(L"An alert completed, but its detailed log could not be allocated.");
    }
}

void log_alert_rejection(
    const wchar_t* label,
    ERUI_Result result) noexcept {
    try {
        std::wstring text = L"Alert \"";
        text += label ? label : L"<unknown>";
        text += L"\" was rejected: result=";
        text += result_name(result);
        text += L".";
        debug(text.c_str());
    } catch (...) {
        debug(L"An alert was rejected, but its detailed log could not be allocated.");
    }
}

void ERUI_CALL alert_completed(
    void*,
    ERUI_Result result,
    erui::AlertResponse response) noexcept {
    log_alert_completion(
        L"Normal OK Dialog", erui::AlertButtons::ok, result, response);
}

void show_native_alert() noexcept {
    const ERUI_Result result = g_registration.alert(
        g_text ? g_text->native_message : L"ERNativeUI",
        &alert_completed);
    if (result != ERUI_OK) {
        log_alert_rejection(L"Normal OK Dialog", result);
    }
}

void ERUI_CALL popup_variant_completed(
    void* context,
    ERUI_Result result,
    erui::AlertResponse response) noexcept {
    const auto* const variant = static_cast<const PopupVariant*>(context);
    log_alert_completion(
        variant ? variant->label.c_str() : L"Popup Variant",
        variant ? variant->options.buttons : erui::AlertButtons::ok,
        result,
        response);
}

void ERUI_CALL show_popup_variant(void* context) noexcept {
    auto* const variant = static_cast<PopupVariant*>(context);
    if (!variant) {
        debug(L"ERNativeUI received an empty popup-variant context.");
        return;
    }
    const ERUI_Result result = g_registration.alert(
        variant->message,
        variant->options,
        &popup_variant_completed,
        variant);
    if (result != ERUI_OK) {
        log_alert_rejection(variant->label.c_str(), result);
    }
}

void toggle_from_code() noexcept {
    const std::uint8_t next = g_enabled.load(std::memory_order_acquire) ? 0 : 1;
    if (g_registration.set_value(g_enabled_row, next) == ERUI_OK) {
        g_enabled.store(next, std::memory_order_release);
        debug(L"Enabled was changed through Registration::set_value().");
    }
}

DWORD WINAPI initialize(void*) noexcept {
    const erui::LanguageInfo language = erui::query_game_language();
    g_text = &showcase::text(language.known);
    initialize_popup_variants(*g_text);

    erui::ProviderOptions options{};
    options.provider_id = "io.github.ernativeui.tarnished-showcase";
    options.display_name = g_text->name;
    options.owner_module = g_module;
    options.root_priority = 100;

    auto result = erui::register_menu(options, [](erui::Menu& menu) {
        auto root = menu.root();
        g_enabled_row = root.add_toggle(
            g_text->enabled,
            g_text->row_help,
            g_enabled.load(std::memory_order_acquire),
            &value_changed,
            &g_enabled);

        erui::SliderOptions slider{};
        slider.minimum = 0;
        slider.maximum = 100;
        slider.step = 5;
        slider.initial_value = g_intensity.load(std::memory_order_acquire);
        root.add_slider(
            g_text->intensity,
            g_text->row_help,
            slider,
            &value_changed,
            &g_intensity);

        auto choice_rows = root.add_submenu(
            g_text->choices,
            g_text->choice_help,
            g_text->choices,
            g_text->choice_help);

        const std::wstring_view preset_values[] = {
            g_text->minimal, g_text->balanced, g_text->detailed, g_text->maximum
        };
        erui::ChoiceOptions preset{};
        preset.values = preset_values;
        preset.count = std::size(preset_values);
        preset.initial_index =
            g_inline_preset.load(std::memory_order_acquire);
        choice_rows.add_inline_choice(
            g_text->inline_preset,
            g_text->choice_help,
            preset,
            &value_changed,
            &g_inline_preset);

        preset.initial_index =
            g_popup_preset.load(std::memory_order_acquire);
        choice_rows.add_popup_choice(
            g_text->popup_preset,
            g_text->choice_help,
            preset,
            &value_changed,
            &g_popup_preset);

        const std::wstring_view popup_two_values[] = {
            g_text->disabled, g_text->enabled_value
        };
        erui::ChoiceOptions popup_two{};
        popup_two.values = popup_two_values;
        popup_two.count = std::size(popup_two_values);
        popup_two.initial_index =
            g_popup_binary.load(std::memory_order_acquire);
        choice_rows.add_popup_choice(
            g_text->binary_mode,
            g_text->choice_help,
            popup_two,
            &value_changed,
            &g_popup_binary);

        std::array<std::wstring, 8> popup_eight_storage{};
        std::array<std::wstring_view, 8> popup_eight_values{};
        for (std::size_t index = 0; index < popup_eight_storage.size(); ++index) {
            popup_eight_storage[index] = g_text->preset;
            popup_eight_storage[index] += L" ";
            popup_eight_storage[index] += std::to_wstring(index + 1);
            popup_eight_values[index] = popup_eight_storage[index];
        }
        erui::ChoiceOptions popup_eight{};
        popup_eight.values = popup_eight_values.data();
        popup_eight.count = std::size(popup_eight_values);
        popup_eight.initial_index =
            g_popup_extended.load(std::memory_order_acquire);
        choice_rows.add_popup_choice(
            g_text->extended_preset,
            g_text->choice_help,
            popup_eight,
            &value_changed,
            &g_popup_extended);

        root.add_button<&toggle_from_code>(
            g_text->toggle_code,
            g_text->row_help);

        auto large = root.add_submenu(
            g_text->large,
            g_text->large_help,
            g_text->large,
            g_text->large_help);

        // The built-in formatter produces the language-neutral "Title (n/t)"
        // form. Providers can still supply a custom formatter when needed.
        large.set_presentation(g_text->name, g_text->large);

        large.add_button<&show_native_alert>(
            g_text->native_dialog,
            g_text->row_help);

        for (std::uint32_t index = 0; index < g_action_numbers.size(); ++index) {
            g_action_numbers[index] = index + 1;
            const std::wstring label = std::wstring(g_text->action) + L" " +
                std::to_wstring(index + 1);
            large.add_button(
                label,
                g_text->row_help,
                &action_pressed,
                &g_action_numbers[index]);
        }

        auto popup_variants = root.add_submenu(
            g_text->popups,
            g_text->popup_help,
            g_text->popups,
            g_text->popup_help);
        popup_variants.set_presentation(
            g_text->name,
            g_text->popups);
        for (PopupVariant& variant : g_popup_variants) {
            popup_variants.add_button(
                variant.label,
                variant.help,
                &show_popup_variant,
                &variant);
        }

    });

    if (!result) {
        debug(result.error().message().c_str());
        return 1;
    }
    g_registration = result.value();
    debug(L"Menu registered successfully.");
    return 0;
}

} // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = module;
        DisableThreadLibraryCalls(module);
        if (HANDLE thread = CreateThread(nullptr, 0, &initialize, nullptr, 0, nullptr)) {
            CloseHandle(thread);
        }
    }
    return TRUE;
}
