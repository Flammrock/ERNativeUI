#include "addresses.hpp"
#include "menu_compiler.hpp"
#include "native_menu.hpp"
#include "runtime_state.hpp"

#include "menu.hpp"
#include "runtime.hpp"

#include "test_assertions.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>

namespace {

std::function<void()> g_captured_action{};
std::uint32_t g_native_constructor_calls{};
std::uint32_t g_text_destructor_calls{};
std::uint32_t g_public_action_calls{};
void* g_expected_page{};

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
    ERUI_TEST_CHECK(page == g_expected_page);
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

    alignas(8) std::array<std::byte, 0x1B00> native_page{};
    // Model the live coexistence case: 13 visual rows and five rows already
    // materialized (four native rows plus one earlier third-party row).
    constexpr std::uint32_t visual_capacity = 13;
    constexpr std::uint64_t materialized_rows = 5;
    std::memcpy(
        native_page.data() + 0xB14,
        &visual_capacity,
        sizeof(visual_capacity));
    std::memcpy(
        native_page.data() + 0x1AF0,
        &materialized_rows,
        sizeof(materialized_rows));
    g_expected_page = native_page.data();

    const erui::native::RowInjectionOutcome outcome =
        erui::native::inject_registered_rows(
            native_page.data(),
            addresses);

    ERUI_TEST_CHECK(outcome.attempted == 1);
    ERUI_TEST_CHECK(outcome.added == 1);
    ERUI_TEST_CHECK(outcome.faulted == 0);
    ERUI_TEST_CHECK(outcome.skipped == 0);
    ERUI_TEST_CHECK(outcome.root_capacity == 12);
    ERUI_TEST_CHECK(g_native_constructor_calls == 1);
    ERUI_TEST_CHECK(g_text_destructor_calls == 1);
    ERUI_TEST_CHECK(static_cast<bool>(g_captured_action));

    // Invoke only after the local std::function objects created by the bridge
    // have been destroyed, matching Elden Ring's retained callback behavior.
    g_captured_action();
    ERUI_TEST_CHECK(g_public_action_calls == 1);
    ERUI_TEST_CHECK(erui::button_action_hit_count() == 1);

    alignas(8) std::array<std::byte, 0x1B00> full_native_page{};
    constexpr std::uint32_t full_visual_capacity = 6;
    constexpr std::uint64_t full_materialized_rows = 6;
    std::memcpy(
        full_native_page.data() + 0xB14,
        &full_visual_capacity,
        sizeof(full_visual_capacity));
    std::memcpy(
        full_native_page.data() + 0x1AF0,
        &full_materialized_rows,
        sizeof(full_materialized_rows));
    const erui::native::RowInjectionOutcome full_outcome =
        erui::native::inject_registered_rows(
            full_native_page.data(),
            addresses);
    ERUI_TEST_CHECK(full_outcome.attempted == 0);
    ERUI_TEST_CHECK(full_outcome.added == 0);
    ERUI_TEST_CHECK(g_native_constructor_calls == 1);

    g_captured_action = {};
    runtime.menu.reset();
    return 0;
}
