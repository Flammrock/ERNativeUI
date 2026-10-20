// ERNativeUI API 1.3 title-screen text example.
//
// This DLL registers one line in each screen corner. Three inherit Elden
// Ring's native title-screen text color; one requests an explicit RGB color.
// It uses only the public header-only SDK.

#include <ernativeui/ERNativeUI.hpp>

#include <Windows.h>

namespace {

HMODULE g_module{};
erui::Registration g_registration{};

void log(const wchar_t* message) noexcept {
    OutputDebugStringW(L"[TitleScreenTextExample] ");
    OutputDebugStringW(message ? message : L"(no message)");
    OutputDebugStringW(L"\n");
}

void register_title_screen_text(erui::Menu& menu) {
    // Every call names its corner explicitly; there is no implicit default.
    menu.add_title_screen_text(
        erui::TitleScreenCorner::top_left,
        {L"ERNativeUI top-left title text"});
    menu.add_title_screen_text(
        erui::TitleScreenCorner::top_right,
        {L"ERNativeUI top-right title text"});
    menu.add_title_screen_text(
        erui::TitleScreenCorner::bottom_left,
        {L"ERNativeUI bottom-left title text"});
    menu.add_title_screen_text(
        erui::TitleScreenCorner::bottom_right,
        {
            L"ERNativeUI bottom-right title text",
            erui::Color{216U, 167U, 103U},
        });
}

DWORD initialize_impl() {
    // Do discovery and registration after DllMain releases the loader lock.
    const auto connection = erui::connect();
    if (!connection) {
        log(connection.error().message().c_str());
        return 1U;
    }
    if (!connection.value().supports(erui::Capability::title_screen_text)) {
        log(L"The connected host does not support API 1.3 title-screen text.");
        return 1U;
    }

    erui::ProviderOptions provider{};
    provider.provider_id = "ernativeui.example.title-screen-text";
    provider.display_name = L"Title-Screen Text Example";
    provider.owner_module = g_module;

    const auto result = connection.value().register_menu(
        provider, &register_title_screen_text);
    if (!result) {
        log(result.error().message().c_str());
        return 1U;
    }

    // Retain the registration for the process lifetime, like a real client.
    g_registration = result.value();
    log(L"Registered one title-screen line in each corner.");
    return 0U;
}

DWORD WINAPI initialize(void*) noexcept {
    try {
        return initialize_impl();
    } catch (...) {
        // A C++ exception must never cross a Windows thread entry point.
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
    // Committed client code is process-lifetime state; hot unloading is not
    // supported. Mod Engine unloads the DLL when the game process exits.
    return TRUE;
}
