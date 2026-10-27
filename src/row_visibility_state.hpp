#pragma once

#include "menu_compiler.hpp"

#include <ernativeui/erui.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace erui::detail {

// A complete desired visibility value for one stable public row identity.
// Registry validation owns field-presence semantics; this layer owns atomic
// publication and the immutable compiled-row projection.
struct RowVisibilityUpdate {
    ERUI_ProviderHandle provider{ERUI_INVALID_PROVIDER};
    ERUI_RowHandle row{ERUI_INVALID_ROW};
    bool visible{true};
};

struct RowVisibilityProjection {
    std::size_t logical_page_index{invalid_compiled_index};
    std::size_t source_row_count{};
    std::size_t visible_row_count{};
    std::uint64_t revision{};

    [[nodiscard]] explicit operator bool() const noexcept {
        return logical_page_index != invalid_compiled_index && revision != 0;
    }
};

// Holds the visibility-state lock after all validation and allocation have
// completed. Registry can prepare visibility first, publish the companion
// presentation snapshot, then commit this token with no remaining failure
// path. Destroying an uncommitted token publishes nothing.
class PreparedRowVisibilityUpdates {
public:
    PreparedRowVisibilityUpdates() noexcept;
    ~PreparedRowVisibilityUpdates();
    PreparedRowVisibilityUpdates(PreparedRowVisibilityUpdates&&) noexcept;
    PreparedRowVisibilityUpdates& operator=(
        PreparedRowVisibilityUpdates&&) noexcept;
    PreparedRowVisibilityUpdates(const PreparedRowVisibilityUpdates&) = delete;
    PreparedRowVisibilityUpdates& operator=(
        const PreparedRowVisibilityUpdates&) = delete;

    [[nodiscard]] explicit operator bool() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_{};

    friend ERUI_Result prepare_row_visibility_updates(
        std::vector<RowVisibilityUpdate>,
        PreparedRowVisibilityUpdates&) noexcept;
    friend void commit_prepared_row_visibility_updates(
        PreparedRowVisibilityUpdates&&) noexcept;
};

[[nodiscard]] ERUI_Result prepare_row_visibility_updates(
    std::vector<RowVisibilityUpdate> updates,
    PreparedRowVisibilityUpdates& output) noexcept;

void commit_prepared_row_visibility_updates(
    PreparedRowVisibilityUpdates&& prepared) noexcept;

// Accepts one already-validated provider batch. The complete batch is
// published to desired state or the previous generation remains unchanged.
// It owns no native object and is safe to call from a client callback.
[[nodiscard]] ERUI_Result submit_row_visibility_updates(
    std::vector<RowVisibilityUpdate> updates) noexcept;

// Establishes stable public-row identities and fixed-capacity page
// projections after menu compilation. Updates accepted before compilation are
// consumed here. Internal rows without public identities remain visible.
[[nodiscard]] bool configure_row_visibility_state(
    const CompiledMenu& menu) noexcept;

// Publishes the latest complete desired generation at a validated UI-safe
// boundary. Page projection vectors reserve their maximum size during
// configuration, so this operation performs no allocation.
[[nodiscard]] bool synchronize_pending_row_visibility() noexcept;

[[nodiscard]] bool row_visibility_updates_pending() noexcept;
[[nodiscard]] std::size_t editable_row_visibility_count() noexcept;

// Reads one applied page projection. Mapping calls require the returned
// revision and fail closed if another UI-boundary publication superseded it.
[[nodiscard]] RowVisibilityProjection row_visibility_projection(
    std::size_t logical_page_index) noexcept;

[[nodiscard]] bool map_projected_row_to_source(
    std::size_t logical_page_index,
    std::uint64_t revision,
    std::size_t projected_row_index,
    std::size_t& source_row_index) noexcept;

[[nodiscard]] bool map_source_row_to_projected(
    std::size_t logical_page_index,
    std::uint64_t revision,
    std::size_t source_row_index,
    std::size_t& projected_row_index) noexcept;

// Resolves pagination against the applied visible projection, including the
// valid zero-row one-page case.
[[nodiscard]] bool resolve_visible_paginated_slice(
    std::size_t logical_page_index,
    std::size_t first_page_capacity,
    std::size_t continuation_capacity,
    std::size_t slice_index,
    PageSlice& output,
    std::uint64_t& projection_revision) noexcept;

// Runtime uninstall/process teardown only, after native callbacks are disarmed.
void reset_row_visibility_state() noexcept;

} // namespace erui::detail
