// ERNativeUI API 1.3 fixed-workload custom-tab stress example.
//
// This DLL deliberately registers exactly 50 tabs, each containing 20
// independent rows. Its workload stays constant when ERNativeUI's process-wide
// capacity changes. It is a stress/integration example rather than a template
// for the usual one- or two-tab client mod.

#include <ernativeui/ERNativeUI.hpp>

#include <Windows.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>

namespace {

constexpr std::size_t kTabCount = 50;
constexpr std::size_t kRowsPerTab = 20;
constexpr std::size_t kTotalRowCount = kTabCount * kRowsPerTab;

static_assert(ERUI_CUSTOM_TAB_MAX_COUNT >= kTabCount,
    "CustomTabsStress requires capacity for at least 50 custom tabs");
static_assert(ERUI_TAB_ICON_WIDTH == 32 && ERUI_TAB_ICON_HEIGHT == 32,
    "The numbered icon renderer targets the API 1.3 32x32 icon contract");

using NumberIcon =
    std::array<std::uint8_t, ERUI_TAB_ICON_RGBA8_BYTE_COUNT>;

HMODULE g_module{};
erui::Registration g_registration{};

// Every callback receives a stable address inside this process-lifetime array.
// Atomic storage is appropriate because value callbacks run on ERNativeUI's
// worker rather than necessarily on the game thread.
struct RowState {
    std::atomic<std::uint8_t> value{0};
    std::uint16_t tab_number{};
    std::uint16_t row_number{};
};

std::array<RowState, kTotalRowCount> g_rows{};

void log(const wchar_t* message) noexcept {
    OutputDebugStringW(L"[CustomTabsStress] ");
    OutputDebugStringW(message ? message : L"(no message)");
    OutputDebugStringW(L"\n");
}

// Produce one transparent 32x32 RGBA8 icon containing a two-digit
// seven-segment number. The integer mixer is intentionally deterministic: the
// palette looks varied, but a stable tab number always receives the same color
// on every run, compiler, and machine. The number remains the primary
// identifier, so the icons are still distinguishable without relying on color
// alone.
//
// Keeping all rasterization here makes the image contract easy to audit. No
// PNG decoder, row padding, platform bitmap, global PRNG, or native game image
// object is involved. Lit segments are opaque while every background pixel is
// {0, 0, 0, 0}. The descriptor below publishes these ordinary RGBA bytes;
// ERNativeUI performs the renderer-specific conversion privately.
[[nodiscard]] constexpr NumberIcon make_number_icon(
    std::uint32_t number) noexcept {
    constexpr std::array<std::uint8_t, 10> kDigitSegments{
        0x3FU, 0x06U, 0x5BU, 0x4FU, 0x66U,
        0x6DU, 0x7DU, 0x07U, 0x7FU, 0x6FU};
    struct SegmentBounds {
        std::uint8_t left;
        std::uint8_t top;
        std::uint8_t right;
        std::uint8_t bottom;
    };
    // Inclusive bounds for top, upper-right, lower-right, bottom,
    // lower-left, upper-left, and middle, relative to one digit's origin.
    constexpr std::array<SegmentBounds, 7> kSegmentBounds{{
        {2U, 3U, 9U, 5U},
        {9U, 5U, 11U, 14U},
        {9U, 16U, 11U, 25U},
        {2U, 25U, 9U, 27U},
        {0U, 16U, 2U, 25U},
        {0U, 5U, 2U, 14U},
        {2U, 14U, 9U, 16U},
    }};

    NumberIcon pixels{};
    const std::uint32_t label = number >= 1U &&
            number <= static_cast<std::uint32_t>(kTabCount)
        ? number
        : 0U;

    // A fixed avalanche mixer supplies a vivid, random-looking accent without
    // any mutable state. Forcing one channel high prevents muddy/dark digits.
    std::uint32_t hash = label + 0x9E3779B9U;
    hash = (hash ^ (hash >> 16U)) * 0x7FEB352DU;
    hash = (hash ^ (hash >> 15U)) * 0x846CA68BU;
    hash ^= hash >> 16U;

    std::array<std::uint8_t, 3> ink{
        static_cast<std::uint8_t>(72U + ((hash >> 0U) & 0x7FU)),
        static_cast<std::uint8_t>(72U + ((hash >> 8U) & 0x7FU)),
        static_cast<std::uint8_t>(72U + ((hash >> 16U) & 0x7FU))};
    ink[hash % ink.size()] =
        static_cast<std::uint8_t>(224U + ((hash >> 24U) & 0x1FU));

    const std::array<std::uint32_t, 2> digits{
        (label / 10U) % 10U,
        label % 10U};

    for (std::uint32_t y = 0; y < ERUI_TAB_ICON_HEIGHT; ++y) {
        for (std::uint32_t x = 0; x < ERUI_TAB_ICON_WIDTH; ++x) {
            bool lit{};
            for (std::size_t digit = 0; digit < digits.size(); ++digit) {
                const std::uint32_t left = digit == 0 ? 3U : 17U;
                const std::uint8_t segments = kDigitSegments[digits[digit]];
                for (std::size_t segment = 0;
                     segment < kSegmentBounds.size();
                     ++segment) {
                    const SegmentBounds& bounds = kSegmentBounds[segment];
                    const bool enabled =
                        (segments & (1U << segment)) != 0U;
                    lit = lit || (enabled &&
                        x >= left + bounds.left &&
                        x <= left + bounds.right &&
                        y >= bounds.top && y <= bounds.bottom);
                }
            }

            const std::size_t offset =
                (static_cast<std::size_t>(y) * ERUI_TAB_ICON_WIDTH + x) * 4U;
            if (lit) {
                for (std::size_t channel = 0;
                     channel < ink.size();
                     ++channel) {
                    pixels[offset + channel] = ink[channel];
                }
                pixels[offset + 3U] = 0xFFU;
            } else {
                // Keep transparent RGB zero as well as alpha. Straight alpha
                // permits arbitrary hidden RGB, but canonical zeroes make the
                // generated payload deterministic and easier to inspect.
                pixels[offset + 0U] = 0U;
                pixels[offset + 1U] = 0U;
                pixels[offset + 2U] = 0U;
                pixels[offset + 3U] = 0U;
            }
        }
    }
    return pixels;
}

[[nodiscard]] constexpr bool has_clean_transparency(
    const NumberIcon& icon) noexcept {
    bool found_transparent{};
    bool found_opaque{};
    for (std::size_t offset = 0; offset < icon.size(); offset += 4U) {
        const std::uint8_t alpha = icon[offset + 3U];
        if (alpha == 0U) {
            found_transparent = true;
            if (icon[offset + 0U] != 0U || icon[offset + 1U] != 0U ||
                icon[offset + 2U] != 0U) {
                return false;
            }
        } else if (alpha == 0xFFU) {
            found_opaque = true;
        } else {
            return false;
        }
    }
    return found_transparent && found_opaque;
}

static_assert(has_clean_transparency(make_number_icon(1U)));
static_assert(has_clean_transparency(make_number_icon(50U)));

[[nodiscard]] std::string two_digits_ascii(std::uint32_t value) {
    std::string result{"00"};
    result[0] = static_cast<char>('0' + (value / 10U) % 10U);
    result[1] = static_cast<char>('0' + value % 10U);
    return result;
}

[[nodiscard]] std::wstring two_digits_wide(std::uint32_t value) {
    std::wstring result{L"00"};
    result[0] = static_cast<wchar_t>(L'0' + (value / 10U) % 10U);
    result[1] = static_cast<wchar_t>(L'0' + value % 10U);
    return result;
}

void ERUI_CALL strength_changed(void* user_data, std::uint8_t value) noexcept {
    auto* const state = static_cast<RowState*>(user_data);
    if (state) state->value.store(value, std::memory_order_release);
}

void build_stress_menu(erui::Menu& menu) {
    for (std::size_t tab_index = 0; tab_index < kTabCount; ++tab_index) {
        const std::uint32_t tab_number =
            static_cast<std::uint32_t>(tab_index + 1U);
        const std::string digits = two_digits_ascii(tab_number);
        const std::wstring wide_digits = two_digits_wide(tab_number);
        const std::string tab_id = "tab-" + digits;
        const std::wstring title = L"Stress Tab " + wide_digits;

        const NumberIcon icon = make_number_icon(tab_number);
        erui::TabOptions tab{};
        tab.tab_id = tab_id;
        tab.title = title;
        tab.preferred_order = static_cast<std::int32_t>(tab_number * 10U);
        tab.icon = {
            icon.data(),
            icon.size(),
            erui::TabIconFormat::rgba8};

        // Menu::add_tab copies the ID, text, descriptor, and all 4096 icon
        // bytes synchronously. Reusing this local icon buffer on the next loop
        // iteration is therefore intentional and demonstrates the API's
        // ownership boundary rather than depending on accidental lifetime.
        erui::Page page = menu.add_tab(tab);
        if (!page.valid()) return;

        for (std::size_t row_index = 0;
             row_index < kRowsPerTab;
             ++row_index) {
            RowState& state = g_rows[tab_index * kRowsPerTab + row_index];
            state.tab_number = static_cast<std::uint16_t>(tab_number);
            state.row_number = static_cast<std::uint16_t>(row_index + 1U);

            // Multiples of five exercise the complete slider range while
            // giving each logical page a visibly independent initial state.
            const std::uint8_t initial_value = static_cast<std::uint8_t>(
                ((tab_index * 7U + row_index * 3U) % 21U) * 5U);
            state.value.store(initial_value, std::memory_order_relaxed);

            erui::SliderOptions slider{};
            slider.minimum = 0;
            slider.maximum = 100;
            slider.step = 5;
            slider.initial_value = initial_value;

            const std::wstring row_digits = two_digits_wide(
                static_cast<std::uint32_t>(row_index + 1U));
            const std::wstring label = L"Tab " + wide_digits +
                L" / Row " + row_digits + L": Independent Strength";
            const std::wstring row_help =
                L"A client-owned value unique to custom tab " + wide_digits +
                L", row " + row_digits + L".";

            if (page.add_slider(
                    label,
                    row_help,
                    slider,
                    &strength_changed,
                    &state) == ERUI_INVALID_ROW) {
                return;
            }
        }
    }
}

DWORD initialize_impl() {
    // Discovery and registration must occur after DllMain returns because the
    // Windows loader lock is still held while DLL_PROCESS_ATTACH is running.
    const auto connection = erui::connect();
    if (!connection) {
        log(connection.error().message().c_str());
        return 1U;
    }
    if (!connection.value().supports(erui::Capability::custom_tabs)) {
        log(L"The connected host cannot expose API 1.3 custom tabs on this game build.");
        return 1U;
    }

    erui::ProviderOptions provider{};
    provider.provider_id = "ernativeui.example.custom-tabs-stress";
    provider.display_name = L"Custom Tabs Stress Example";
    provider.owner_module = g_module;

    const auto result = connection.value().register_menu(
        provider, &build_stress_menu);
    if (!result) {
        // A common intentional failure is ERUI_TAB_LIMIT_REACHED: this example
        // requests 50 process-wide slots and should therefore be tested
        // without another custom-tab client loaded beside it.
        log(result.error().message().c_str());
        return 1U;
    }

    g_registration = result.value();
    log(L"Registered 50 custom tabs and 1,000 independent slider rows.");
    return 0U;
}

DWORD WINAPI initialize(void*) noexcept {
    try {
        return initialize_impl();
    } catch (...) {
        // No C++ exception may escape a Windows thread entry point.
        log(L"Initialization failed with an unexpected client-side exception.");
        return 1U;
    }
}

} // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = module;
        DisableThreadLibraryCalls(module);
        if (HANDLE thread = CreateThread(
                nullptr, 0, &initialize, nullptr, 0, nullptr)) {
            CloseHandle(thread);
        } else {
            log(L"Could not create the initialization thread.");
        }
    }
    // ERNativeUI pins committed callback code. Client-mod hot unloading is not
    // supported; Mod Engine unloads the complete process at game shutdown.
    return TRUE;
}
