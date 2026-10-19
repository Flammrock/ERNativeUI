#include <ernativeui/ERNativeUI.hpp>

#include <Windows.h>

#include <atomic>
#include <array>
#include <cstdint>
#include <cwchar>

namespace {

constexpr std::size_t kStressTabCount = 50;
constexpr std::size_t kStressRowsPerTab = 20;

struct RowState {
    std::uint16_t tab{};
    std::uint16_t row{};
    std::atomic<std::uint8_t> value{};
};

HMODULE g_module{};
erui::Registration g_registration{};
std::array<std::array<RowState, kStressRowsPerTab>, kStressTabCount>
    g_rows{};

void debug(const wchar_t* message) noexcept {
    OutputDebugStringW(L"[ERNativeUITabContentClient] ");
    OutputDebugStringW(message ? message : L"");
    OutputDebugStringW(L"\n");
}

void ERUI_CALL tab_value_changed(
    void* user_data,
    std::uint8_t value) noexcept {
    if (!user_data) return;
    auto& state = *static_cast<RowState*>(user_data);
    state.value.store(value, std::memory_order_release);
    std::array<wchar_t, 160> message{};
    swprintf_s(message.data(), message.size(),
        L"tab=%u row=%u value=%u state=%p",
        static_cast<unsigned>(state.tab),
        static_cast<unsigned>(state.row),
        static_cast<unsigned>(value), &state);
    debug(message.data());
}

DWORD WINAPI initialize(void*) noexcept {
    const auto connection = erui::connect();
    if (!connection) {
        debug(L"Could not connect to the research ERNativeUI host.");
        return 1;
    }

    erui::ProviderOptions options{};
    options.provider_id = "ernativeui-tab-content-probe";
    options.display_name = L"ERNativeUI Tab Content Probe";
    options.owner_module = g_module;
    options.root_priority = -1000;

    const auto result = connection.value().register_menu(
        options,
        [](erui::Menu& menu) {
            auto root = menu.root();

            for (std::size_t tab_index = 0;
                 tab_index < kStressTabCount;
                 ++tab_index) {
                std::array<wchar_t, 96> title{};
                swprintf_s(title.data(), title.size(),
                    L"[Probe] Client Page %02zu", tab_index + 1U);
                auto tab = root.add_submenu(title.data(),
                    L"Research-only disabled transport.", title.data(),
                    L"Twenty client-owned rows paginate through one recyclable native carrier.",
                    false);
                for (std::size_t row_index = 0;
                     row_index < kStressRowsPerTab;
                     ++row_index) {
                    auto& state = g_rows[tab_index][row_index];
                    state.tab = static_cast<std::uint16_t>(tab_index + 1U);
                    state.row = static_cast<std::uint16_t>(row_index + 1U);
                    state.value.store(static_cast<std::uint8_t>(
                        ((tab_index + 1U) * 17U +
                            (row_index + 1U) * 7U) % 101U),
                        std::memory_order_relaxed);

                    std::array<wchar_t, 96> label{};
                    std::array<wchar_t, 160> help{};
                    swprintf_s(label.data(), label.size(),
                        L"T%02zu R%02zu: Independent Strength",
                        tab_index + 1U, row_index + 1U);
                    swprintf_s(help.data(), help.size(),
                        L"Client-owned state for stress tab %02zu, row %02zu. Change it and verify it after navigation.",
                        tab_index + 1U, row_index + 1U);
                    erui::SliderOptions strength{};
                    strength.minimum = 0;
                    strength.maximum = 100;
                    strength.step = 1;
                    strength.initial_value = state.value.load(
                        std::memory_order_relaxed);
                    tab.add_slider(label.data(), help.data(), strength,
                        &tab_value_changed, &state);
                }
            }
        });

    if (!result) {
        debug(L"Research provider registration failed.");
        return 1;
    }
    g_registration = result.value();
    debug(L"Fifty independent twenty-row client tab pages registered.");
    return 0;
}

} // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
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
