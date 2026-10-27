#include "menu.hpp"
#include "menu_compiler.hpp"
#include "row_visibility_state.hpp"
#include "test_assertions.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace {

constexpr ERUI_ProviderHandle kProvider = 41;
constexpr ERUI_RowHandle kRootFirst = 101;
constexpr ERUI_RowHandle kRootSecond = 102;
constexpr ERUI_RowHandle kChildFirst = 201;
constexpr ERUI_RowHandle kChildSecond = 202;

void pressed(void*) noexcept {}

std::unique_ptr<erui::detail::CompiledMenu> make_menu() {
    erui::Menu menu(L"Host", L"Help");
    volatile std::uint8_t first = 0;
    volatile std::uint8_t second = 1;

    // An internal Core row has no public identity and must remain visible.
    menu.root().add_toggle(L"Internal", L"", first);
    menu.root().add_toggle(L"First", L"", first);
    menu.root().identify_last_row(kProvider, kRootFirst, true);
    menu.root().add_slider(L"Second", L"", second, {0, 2, 1});
    menu.root().identify_last_row(kProvider, kRootSecond, true);

    erui::Page& child = menu.root().add_submenu(
        L"Child", L"", L"Child", L"");
    child.add_button(L"Child first", L"", {&pressed, nullptr});
    child.identify_last_row(kProvider, kChildFirst, true);
    child.add_button(L"Child second", L"", {&pressed, nullptr});
    child.identify_last_row(kProvider, kChildSecond, true);

    auto result = erui::detail::MenuCompiler::compile(menu);
    return result;
}

void expect_projection(
    std::size_t page,
    std::size_t source_count,
    const std::vector<std::size_t>& expected) {
    const erui::detail::RowVisibilityProjection projection =
        erui::detail::row_visibility_projection(page);
    ERUI_TEST_CHECK(projection);
    ERUI_TEST_CHECK(projection.source_row_count == source_count);
    ERUI_TEST_CHECK(projection.visible_row_count == expected.size());
    for (std::size_t index = 0; index < expected.size(); ++index) {
        std::size_t source{};
        ERUI_TEST_CHECK(erui::detail::map_projected_row_to_source(
            page, projection.revision, index, source));
        ERUI_TEST_CHECK(source == expected[index]);
        std::size_t projected{};
        ERUI_TEST_CHECK(erui::detail::map_source_row_to_projected(
            page, projection.revision, source, projected));
        ERUI_TEST_CHECK(projected == index);
    }
}

} // namespace

int main() {
    using erui::detail::RowVisibilityUpdate;

    erui::detail::reset_row_visibility_state();
    ERUI_TEST_CHECK(!erui::detail::row_visibility_updates_pending());

    // A pre-compilation update is copied and becomes the initial applied
    // topology rather than causing a redundant live rebuild.
    ERUI_TEST_CHECK(erui::detail::submit_row_visibility_updates({
        RowVisibilityUpdate{kProvider, kRootSecond, false},
    }) == ERUI_OK);
    ERUI_TEST_CHECK(erui::detail::row_visibility_updates_pending());
    std::unique_ptr<erui::detail::CompiledMenu> menu = make_menu();
    ERUI_TEST_CHECK(erui::detail::configure_row_visibility_state(*menu));
    ERUI_TEST_CHECK(!erui::detail::row_visibility_updates_pending());
    ERUI_TEST_CHECK(erui::detail::editable_row_visibility_count() == 4);
    expect_projection(0, 4, {0, 1, 3});
    expect_projection(1, 2, {0, 1});

    const std::uint64_t root_initial_revision =
        erui::detail::row_visibility_projection(0).revision;
    const std::uint64_t child_initial_revision =
        erui::detail::row_visibility_projection(1).revision;

    // Desired publication is atomic and does not change the applied
    // projection before the UI-safe synchronization boundary.
    ERUI_TEST_CHECK(erui::detail::submit_row_visibility_updates({
        RowVisibilityUpdate{kProvider, kRootFirst, false},
        RowVisibilityUpdate{kProvider, kRootSecond, true},
        RowVisibilityUpdate{kProvider, kChildFirst, false},
    }) == ERUI_OK);
    ERUI_TEST_CHECK(erui::detail::row_visibility_updates_pending());
    expect_projection(0, 4, {0, 1, 3});
    expect_projection(1, 2, {0, 1});
    ERUI_TEST_CHECK(erui::detail::synchronize_pending_row_visibility());
    ERUI_TEST_CHECK(!erui::detail::row_visibility_updates_pending());
    expect_projection(0, 4, {0, 2, 3});
    expect_projection(1, 2, {1});
    ERUI_TEST_CHECK(erui::detail::row_visibility_projection(0).revision !=
        root_initial_revision);
    ERUI_TEST_CHECK(erui::detail::row_visibility_projection(1).revision !=
        child_initial_revision);

    // An old projection revision can never map through a newer topology.
    std::size_t source{};
    ERUI_TEST_CHECK(!erui::detail::map_projected_row_to_source(
        0, root_initial_revision, 0, source));
    ERUI_TEST_CHECK(source == erui::detail::invalid_compiled_index);

    // Latest desired state wins when several client commits arrive before one
    // safe boundary.
    ERUI_TEST_CHECK(erui::detail::submit_row_visibility_updates({
        RowVisibilityUpdate{kProvider, kChildSecond, false},
    }) == ERUI_OK);
    ERUI_TEST_CHECK(erui::detail::submit_row_visibility_updates({
        RowVisibilityUpdate{kProvider, kChildSecond, true},
    }) == ERUI_OK);
    ERUI_TEST_CHECK(erui::detail::synchronize_pending_row_visibility());
    expect_projection(1, 2, {1});

    // Reasserting the already-desired state is a true no-op: it must not
    // schedule a destructive native rebuild or advance the projection.
    const std::uint64_t child_stable_revision =
        erui::detail::row_visibility_projection(1).revision;
    ERUI_TEST_CHECK(erui::detail::submit_row_visibility_updates({
        RowVisibilityUpdate{kProvider, kChildSecond, true},
    }) == ERUI_OK);
    ERUI_TEST_CHECK(!erui::detail::row_visibility_updates_pending());
    ERUI_TEST_CHECK(erui::detail::row_visibility_projection(1).revision ==
        child_stable_revision);

    // Preparation is a rollback token, not publication. Abandoning it must
    // release the lock without changing desired or applied state.
    const std::uint64_t root_stable_revision =
        erui::detail::row_visibility_projection(0).revision;
    {
        erui::detail::PreparedRowVisibilityUpdates abandoned{};
        ERUI_TEST_CHECK(erui::detail::prepare_row_visibility_updates({
            RowVisibilityUpdate{kProvider, kRootFirst, true},
        }, abandoned) == ERUI_OK);
        ERUI_TEST_CHECK(abandoned);
    }
    ERUI_TEST_CHECK(!erui::detail::row_visibility_updates_pending());
    ERUI_TEST_CHECK(erui::detail::row_visibility_projection(0).revision ==
        root_stable_revision);
    expect_projection(0, 4, {0, 2, 3});

    // The generic projection layer represents zero content safely. The
    // production Registry currently rejects this state because its native
    // lifecycle has not been live-proven; retaining model coverage keeps the
    // boundary explicit rather than relying on underflow behavior.
    ERUI_TEST_CHECK(erui::detail::submit_row_visibility_updates({
        RowVisibilityUpdate{kProvider, kChildFirst, false},
        RowVisibilityUpdate{kProvider, kChildSecond, false},
    }) == ERUI_OK);
    ERUI_TEST_CHECK(erui::detail::synchronize_pending_row_visibility());
    expect_projection(1, 2, {});
    erui::detail::PageSlice slice{};
    std::uint64_t slice_revision{};
    ERUI_TEST_CHECK(erui::detail::resolve_visible_paginated_slice(
        1, 15, 15, 0, slice, slice_revision));
    ERUI_TEST_CHECK(slice_revision ==
        erui::detail::row_visibility_projection(1).revision);
    ERUI_TEST_CHECK(slice.slice_count == 1);
    ERUI_TEST_CHECK(slice.row_count == 0);
    ERUI_TEST_CHECK(!slice.has_previous && !slice.has_next);
    ERUI_TEST_CHECK(!erui::detail::resolve_visible_paginated_slice(
        1, 15, 15, 1, slice, slice_revision));

    // A rejected batch never changes desired or applied state.
    ERUI_TEST_CHECK(erui::detail::submit_row_visibility_updates({
        RowVisibilityUpdate{kProvider, kRootFirst, true},
        RowVisibilityUpdate{kProvider, kRootFirst, false},
    }) == ERUI_INVALID_ARGUMENT);
    ERUI_TEST_CHECK(!erui::detail::row_visibility_updates_pending());
    ERUI_TEST_CHECK(erui::detail::submit_row_visibility_updates({
        RowVisibilityUpdate{kProvider, 9999, false},
    }) == ERUI_INVALID_HANDLE);
    ERUI_TEST_CHECK(!erui::detail::row_visibility_updates_pending());
    expect_projection(0, 4, {0, 2, 3});

    ERUI_TEST_CHECK(!erui::detail::configure_row_visibility_state(*menu));
    erui::detail::reset_row_visibility_state();
    ERUI_TEST_CHECK(!erui::detail::row_visibility_projection(0));
    ERUI_TEST_CHECK(erui::detail::editable_row_visibility_count() == 0);
    return 0;
}
