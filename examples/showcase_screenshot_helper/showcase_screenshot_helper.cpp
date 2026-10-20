#include <ernativeui/ERNativeUI.hpp>

#include <Windows.h>

#include <atomic>
#include <array>
#include <cstddef>
#include <cstdint>
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
    numeric_slider,
    custom_slider,
    inline_choice,
    popup_choice,
    text_input,
    color_picker,
    custom_tab,
    title_screen_text,
};

struct ModeDefinition {
    std::string_view name;
    ShowcaseMode mode;
    const wchar_t* page_title;
};

constexpr std::array<ModeDefinition, 12> kModes{{
    {"button", ShowcaseMode::button, L"Button Showcase"},
    {"submenu", ShowcaseMode::submenu, L"Submenu Showcase"},
    {"toggle", ShowcaseMode::toggle, L"Toggle Showcase"},
    {"slider", ShowcaseMode::slider, L"Slider Showcase"},
    {"numeric-slider", ShowcaseMode::numeric_slider,
        L"Numeric Slider Showcase"},
    {"custom-slider", ShowcaseMode::custom_slider,
        L"Custom Slider Showcase"},
    {"inline-choice", ShowcaseMode::inline_choice, L"Inline Choice Showcase"},
    {"popup-choice", ShowcaseMode::popup_choice, L"Popup Choice Showcase"},
    {"text-input", ShowcaseMode::text_input, L"TextInput Showcase"},
    {"color-picker", ShowcaseMode::color_picker, L"ColorPicker Showcase"},
    {"custom-tab", ShowcaseMode::custom_tab, L"My Mod Settings"},
    {"title-screen-text", ShowcaseMode::title_screen_text,
        L"Title-Screen Text Showcase"},
}};

constexpr std::array<std::wstring_view, 4> kQualityValues{
    L"Minimal", L"Balanced", L"Detailed", L"Maximum"};

constexpr std::array<std::wstring_view, 7> kImpossibleValues{
    L"-7", L"3", L"7.8", L"1s", L"hello", L"18", L"x1.3"};

using TabIconPixels =
    std::array<std::uint8_t, ERUI_TAB_ICON_RGBA8_BYTE_COUNT>;

// Generate the same transparent triangle icon shown in the custom-tab guide.
// Each pixel is four consecutive bytes: red, green, blue, then opacity.
[[nodiscard]] constexpr TabIconPixels make_triangle_tab_icon() noexcept
{
    constexpr std::uint8_t red = 107;
    constexpr std::uint8_t green = 100;
    constexpr std::uint8_t blue = 80;
    constexpr std::size_t top = 4;
    constexpr std::size_t bottom = 27;

    TabIconPixels pixels{};
    for (std::size_t y = 0; y < ERUI_TAB_ICON_HEIGHT; ++y) {
        if (y < top || y > bottom) continue;

        // The permitted distance from the centre grows on each row.
        const int maximum_distance = static_cast<int>(y - top) + 1;

        for (std::size_t x = 0; x < ERUI_TAB_ICON_WIDTH; ++x) {
            // Doubled coordinates place the centre between columns 15 and
            // 16, keeping the triangle symmetric without using floats.
            const int from_centre = static_cast<int>(x * 2U + 1U) - 32;
            const int distance =
                from_centre < 0 ? -from_centre : from_centre;
            if (distance > maximum_distance) continue;

            // A translucent one-pixel boundary softens the diagonal edges.
            const bool boundary =
                distance + 2 > maximum_distance || y == bottom;
            const std::uint8_t opacity = boundary ? 160 : 255;

            const std::size_t offset =
                (y * ERUI_TAB_ICON_WIDTH + x) * 4U;
            pixels[offset + 0U] = red;
            pixels[offset + 1U] = green;
            pixels[offset + 2U] = blue;
            pixels[offset + 3U] = opacity;
        }
    }
    return pixels;
}

inline constexpr TabIconPixels kTriangleIcon = make_triangle_tab_icon();

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

    case ShowcaseMode::numeric_slider:
        if (menu.supports(erui::Capability::numeric_slider)) {
            erui::NumericSliderOptions distance{};
            distance.minimum = 0.1;
            distance.maximum = 2.0;
            distance.step = 0.1;
            distance.initial_value = 0.6;
            distance.decimal_places = 1;
            distance.prefix = L"x";
            page.add_slider(
                L"Camera Distance",
                L"Adjust the camera distance.",
                distance);
        }
        break;

    case ShowcaseMode::custom_slider:
        if (menu.supports(erui::Capability::custom_slider)) {
            erui::CustomSliderOptions values{};
            values.value_count =
                static_cast<std::uint32_t>(kImpossibleValues.size());
            values.initial_index = 4;
            values.format_value =
                [](std::uint32_t index, std::uint32_t) -> std::wstring {
                    return index < kImpossibleValues.size()
                        ? std::wstring(kImpossibleValues[index])
                        : std::wstring{};
                };
            page.add_custom_slider(
                L"Impossible Values",
                L"Every position can display unrelated text.",
                values);
        }
        break;

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

    case ShowcaseMode::custom_tab:
        // Registered directly by add_custom_tab_showcase().
        break;

    case ShowcaseMode::title_screen_text:
        // Registered directly by add_title_screen_text_showcase().
        break;
    }
}

void add_title_screen_text_showcase(erui::Menu& menu) noexcept
{
    if (!menu.supports(erui::Capability::title_screen_text)) {
        debug(
            L"The title-screen-text capability is unavailable for this host.");
        return;
    }

    // One line deliberately omits a color so the screenshot also documents
    // Elden Ring's native presentation. The other colors are subdued enough
    // to remain legible against the title screen without looking like debug
    // overlays. Fixed text and RGB values keep canonical screenshots stable.
    menu.add_title_screen_text(
        erui::TitleScreenCorner::top_left,
        {L"Top left - native color"});
    menu.add_title_screen_text(
        erui::TitleScreenCorner::top_right,
        {
            L"Top right - muted gold",
            erui::Color{216U, 167U, 103U},
        });
    menu.add_title_screen_text(
        erui::TitleScreenCorner::bottom_left,
        {
            L"Bottom left - pale blue",
            erui::Color{132U, 168U, 192U},
        });
    menu.add_title_screen_text(
        erui::TitleScreenCorner::bottom_right,
        {
            L"Bottom right - soft rose",
            erui::Color{190U, 128U, 142U},
        });
}

void add_custom_tab_showcase(erui::Menu& menu) noexcept
{
    if (!menu.supports(erui::Capability::custom_tabs)) {
        debug(L"The custom-tab capability is unavailable for this host/game build.");
        return;
    }

    erui::TabOptions options{};
    options.tab_id = "main";
    options.title = L"My Mod Settings";
    options.preferred_order = 100;
    options.icon = {
        kTriangleIcon.data(),
        kTriangleIcon.size(),
        erui::TabIconFormat::rgba8};

    // add_tab copies the ID, title, and icon bytes before returning. The Page
    // it returns accepts the same rows as a submenu or built-in settings page.
    erui::Page page = menu.add_tab(options);
    if (!page.valid()) return;

    page.add_toggle(
        L"Enable My Feature",
        L"Turn the example feature on or off.",
        1);
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

            if (mode == ShowcaseMode::custom_tab) {
                add_custom_tab_showcase(menu);
                return;
            }
            if (mode == ShowcaseMode::title_screen_text) {
                add_title_screen_text_showcase(menu);
                return;
            }

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
