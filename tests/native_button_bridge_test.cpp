#include "addresses.hpp"
#include "menu_compiler.hpp"
#include "native_menu.hpp"
#include "runtime_state.hpp"

#include "menu.hpp"
#include "runtime.hpp"

#include "test_assertions.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>

namespace {

std::function<void()> g_captured_action{};
std::uint32_t g_native_constructor_calls{};
std::uint32_t g_text_destructor_calls{};
std::uint32_t g_public_action_calls{};

void pressed(void*) noexcept {
    ++g_public_action_calls;
}

void* __fastcall fake_text_reference(
    void* destination,
    std::uint32_t message_id) {
    ERUI_TEST_CHECK(destination != nullptr);
    std::memset(destination, 0, 0x38);
    *static_cast<std::uint32_t*>(destination) = message_id;
    return destination;
}

void* __fastcall fake_root_button_text_references(void* destination) {
    ERUI_TEST_CHECK(destination != nullptr);
    std::memset(destination, 0, 0x70);
    return destination;
}

void __fastcall fake_add_button(
    void* page,
    void* text_references,
    void* display_text_reference,
    void* primary_action,
    void* secondary_action) {
    ERUI_TEST_CHECK(page == reinterpret_cast<void*>(0x1234));
    ERUI_TEST_CHECK(text_references != nullptr);
    ERUI_TEST_CHECK(display_text_reference != nullptr);
    ERUI_TEST_CHECK(primary_action != nullptr);
    ERUI_TEST_CHECK(secondary_action != nullptr);

    auto& primary = *static_cast<std::function<void()>*>(primary_action);
    auto& secondary = *static_cast<std::function<void()>*>(secondary_action);
    ERUI_TEST_CHECK(static_cast<bool>(primary));
    ERUI_TEST_CHECK(!secondary);

    // The real constructor clones the callback. Keeping this copy alive after
    // inject_registered_rows() returns verifies the same required lifetime.
    g_captured_action = primary;
    ++g_native_constructor_calls;
}

void __fastcall fake_destroy_text_references(void* text_references) {
    ERUI_TEST_CHECK(text_references != nullptr);
    ++g_text_destructor_calls;
}

} // namespace

int main() {
    erui::Menu menu(L"Button bridge test");
    menu.root().add_button(
        L"Action",
        L"Invoke the test callback.",
        {.callback = &pressed});

    erui::detail::RuntimeState& runtime = erui::detail::runtime_state();
    runtime.options = {};
    runtime.options.enable_custom_text = true;
    runtime.options.enable_diagnostics = false;
    runtime.button_action_hits.store(0, std::memory_order_release);
    runtime.menu = erui::detail::MenuCompiler::compile(menu);

    erui::native::GameAddresses addresses{};
    addresses.text_ref_label = &fake_text_reference;
    addresses.text_ref_help = &fake_text_reference;
    addresses.root_button_text_references =
        &fake_root_button_text_references;
    addresses.root_button_display_text = &fake_text_reference;
    addresses.add_button = &fake_add_button;
    addresses.destroy_text_references = &fake_destroy_text_references;

    const erui::native::RowInjectionOutcome outcome =
        erui::native::inject_registered_rows(
            reinterpret_cast<void*>(0x1234),
            addresses);

    ERUI_TEST_CHECK(outcome.attempted == 1);
    ERUI_TEST_CHECK(outcome.added == 1);
    ERUI_TEST_CHECK(outcome.faulted == 0);
    ERUI_TEST_CHECK(outcome.skipped == 0);
    ERUI_TEST_CHECK(g_native_constructor_calls == 1);
    ERUI_TEST_CHECK(g_text_destructor_calls == 1);
    ERUI_TEST_CHECK(static_cast<bool>(g_captured_action));

    // Invoke only after the local std::function objects created by the bridge
    // have been destroyed, matching Elden Ring's retained callback behavior.
    g_captured_action();
    ERUI_TEST_CHECK(g_public_action_calls == 1);
    ERUI_TEST_CHECK(erui::button_action_hit_count() == 1);

    g_captured_action = {};
    runtime.menu.reset();
    return 0;
}
