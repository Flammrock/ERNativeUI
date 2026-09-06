#include <ernativeui/ERNativeUI.hpp>

#include <Windows.h>

#include <atomic>
#include <array>
#include <optional>
#include <string>
#include <string_view>

namespace {

constexpr wchar_t kConfigFile[] = L"ShowcaseScreenshotHelper.ini";
constexpr char kConfigSection[] = "showcase";
constexpr char kModeKey[] = "mode";

enum class ShowcaseMode {
    button,
    submenu,
    toggle,
    slider,
    inline_choice,
    popup_choice,
    text_input,
    color_picker,
};

struct ModeDefinition {
    std::string_view name;
    ShowcaseMode mode;
    const wchar_t* page_title;
};

constexpr std::array<ModeDefinition, 8> kModes{{
    {"button", ShowcaseMode::button, L"Button Showcase"},
    {"submenu", ShowcaseMode::submenu, L"Submenu Showcase"},
    {"toggle", ShowcaseMode::toggle, L"Toggle Showcase"},
    {"slider", ShowcaseMode::slider, L"Slider Showcase"},
    {"inline-choice", ShowcaseMode::inline_choice, L"Inline Choice Showcase"},
    {"popup-choice", ShowcaseMode::popup_choice, L"Popup Choice Showcase"},
    {"text-input", ShowcaseMode::text_input, L"TextInput Showcase"},
    {"color-picker", ShowcaseMode::color_picker, L"ColorPicker Showcase"},
}};

constexpr std::array<std::wstring_view, 4> kQualityValues{
    L"Minimal", L"Balanced", L"Detailed", L"Maximum"};

HMODULE g_module{};
erui::Registration g_registration{};
std::atomic_bool g_registration_ready{false};

void debug(const wchar_t* message) noexcept
{
    OutputDebugStringW(L"[ShowcaseScreenshotHelper] ");
    OutputDebugStringW(message);
    OutputDebugStringW(L"\n");
}

std::string_view trim_ascii(std::string_view value) noexcept
{
    while (!value.empty() &&
        (value.front() == ' ' || value.front() == '\t')) {
        value.remove_prefix(1);
    }
    while (!value.empty() &&
        (value.back() == ' ' || value.back() == '\t')) {
        value.remove_suffix(1);
    }
    return value;
}

bool equal_mode_name(
    std::string_view value,
    std::string_view expected) noexcept
{
    value = trim_ascii(value);
    if (value.size() != expected.size()) return false;

    for (std::size_t index = 0; index < value.size(); ++index) {
        char actual = value[index];
        if (actual >= 'A' && actual <= 'Z') {
            actual = static_cast<char>(actual - 'A' + 'a');
        } else if (actual == '_') {
            actual = '-';
        }
        if (actual != expected[index]) return false;
    }
    return true;
}

std::optional<ShowcaseMode> parse_mode(std::string_view value) noexcept
{
    for (const ModeDefinition& definition : kModes) {
        if (equal_mode_name(value, definition.name)) {
            return definition.mode;
        }
    }
    return std::nullopt;
}

const ModeDefinition& definition_for(ShowcaseMode mode) noexcept
{
    for (const ModeDefinition& definition : kModes) {
        if (definition.mode == mode) return definition;
    }
    return kModes.front();
}

ShowcaseMode load_mode(erui::Menu& menu) noexcept
{
    constexpr ShowcaseMode fallback = ShowcaseMode::button;

    if (!menu.supports(erui::Capability::storage)) {
        debug(L"Storage is unavailable; using button mode.");
        return fallback;
    }

    const erui::Storage config = menu.storage(
        erui::StorageOptions::beside_module(kConfigFile));
    if (!config || config.load() != ERUI_OK) {
        debug(L"Could not load the helper INI; using button mode.");
        return fallback;
    }

    const erui::StorageSection showcase = config.section(kConfigSection);
    const auto stored = showcase.get<std::string>(kModeKey);
    if (stored.found()) {
        if (const auto parsed = parse_mode(stored.value())) {
            return *parsed;
        }
        debug(L"Unknown showcase.mode value; using button mode.");
        return fallback;
    }

    if (stored.result() == ERUI_NOT_FOUND) {
        // A missing file loads as an empty document. Creating the default here
        // makes the example self-contained even when only its DLL was copied.
        if (showcase.set(kModeKey, "button") == ERUI_OK) {
            (void)config.save();
        }
    } else {
        debug(L"Could not read showcase.mode; using button mode.");
    }
    return fallback;
}

void show_greeting() noexcept
{
    if (g_registration_ready.load(std::memory_order_acquire)) {
        (void)g_registration.alert(L"Hello, Tarnished!");
    }
}

void add_selected_control(
    erui::Menu& menu,
    erui::Page page,
    ShowcaseMode mode) noexcept
{
    switch (mode) {
    case ShowcaseMode::button:
        page.add_button<&show_greeting>(
            L"Show Greeting",
            L"Show a native Hello, Tarnished! message.");
        break;

    case ShowcaseMode::submenu: {
        erui::Page details = page.add_submenu(
            L"Open Details",
            L"Open the example child page.",
            L"Details");
        details.add_toggle(
            L"Enabled",
            L"Enable the example.",
            1);
        break;
    }

    case ShowcaseMode::toggle:
        page.add_toggle(
            L"Enabled",
            L"Turn this example on or off.",
            1);
        break;

    case ShowcaseMode::slider: {
        erui::SliderOptions intensity{};
        intensity.minimum = 0;
        intensity.maximum = 100;
        intensity.step = 5;
        intensity.initial_value = 50;
        page.add_slider(
            L"Intensity",
            L"Adjust the example intensity.",
            intensity);
        break;
    }

    case ShowcaseMode::inline_choice: {
        erui::ChoiceOptions quality{};
        quality.values = kQualityValues.data();
        quality.count = kQualityValues.size();
        quality.initial_index = 1;
        page.add_inline_choice(
            L"Quality",
            L"Change quality with left and right.",
            quality);
        break;
    }

    case ShowcaseMode::popup_choice: {
        erui::ChoiceOptions quality{};
        quality.values = kQualityValues.data();
        quality.count = kQualityValues.size();
        quality.initial_index = 1;
        page.add_popup_choice(
            L"Quality Preset",
            L"Open the complete quality list.",
            quality);
        break;
    }

    case ShowcaseMode::text_input:
        if (menu.supports(erui::Capability::text_input)) {
            erui::TextInputOptions name{};
            name.placeholder = L"Enter a name";
            name.maximum_length = 16;
            page.add_text_input(
                L"Player Name",
                L"Enter the name used by this example.",
                name);
        }
        break;

    case ShowcaseMode::color_picker:
        if (menu.supports(erui::Capability::color_picker)) {
            erui::ColorPickerOptions accent{};
            accent.initial_value = {171, 125, 99};
            page.add_color_picker(
                L"Accent Color",
                L"Choose the example accent color.",
                accent);
        }
        break;
    }
}

DWORD WINAPI initialize(void*) noexcept
{
    const auto connection = erui::connect();
    if (!connection) {
        debug(L"Could not connect to ERNativeUI.");
        return 1u;
    }

    erui::ProviderOptions options{};
    options.provider_id = "showcase-screenshot-helper";
    options.display_name = L"ERNativeUI Screenshot Helper";
    options.owner_module = g_module;

    const auto result = connection.value().register_menu(
        options,
        [](erui::Menu& menu) {
            const ShowcaseMode mode = load_mode(menu);
            const ModeDefinition& definition = definition_for(mode);

            erui::Page showcase = menu.root().add_submenu(
                definition.page_title,
                L"Open one isolated control for a documentation screenshot.",
                definition.page_title,
                L"One isolated ERNativeUI control.");
            add_selected_control(menu, showcase, mode);
        });

    if (!result) {
        debug(L"Menu registration failed.");
        return 1u;
    }

    g_registration = result.value();
    g_registration_ready.store(true, std::memory_order_release);
    return 0u;
}

} // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = module;
        DisableThreadLibraryCalls(module);
        if (HANDLE thread = CreateThread(
                nullptr, 0, &initialize, nullptr, 0, nullptr)) {
            CloseHandle(thread);
        }
    }
    return TRUE;
}
