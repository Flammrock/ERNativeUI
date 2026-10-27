#pragma once

#include "menu.hpp"
#include "menu_compiler.hpp"
#include "native_tab_catalogue.hpp"

#include <ernativeui/erui.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace erui::detail {

// Complete field values for one custom-tab mutation. Registry owns public
// descriptor validation and provider ownership; this layer owns atomic
// generation construction and stable runtime projection.
struct TabRuntimeUpdate final {
    ERUI_ProviderHandle provider{ERUI_INVALID_PROVIDER};
    ERUI_PageHandle page{ERUI_INVALID_PAGE};
    ERUI_TabUpdateFields fields{};
    std::wstring title{};
    TabIconSpec icon{};
    bool visible{true};
    std::int32_t preferred_order{};
};

struct TabRuntimeEntry final {
    ERUI_ProviderHandle provider{ERUI_INVALID_PROVIDER};
    ERUI_PageHandle page{ERUI_INVALID_PAGE};
    std::size_t stable_index{invalid_compiled_index};
    std::size_t page_index{invalid_compiled_index};
    std::string provider_id{};
    std::string tab_id{};
    std::wstring title{};
    // The selector always owns title.  The page heading only becomes
    // authoritative here after an explicit rich TITLE edit; untouched tabs
    // retain their registered PagePresentation or formatter contract.
    bool title_overridden{};
    TabIconSpec icon{};
    bool visible{true};
    std::int32_t preferred_order{};
    std::optional<std::int32_t> player_order{};
    std::int32_t effective_order{};
    std::uint64_t icon_revision{1};
};

// Immutable process-owned generation. entries never change stable identity or
// route; visible_order alone determines the current selector projection.
struct TabRuntimeGeneration final {
    std::vector<TabRuntimeEntry> entries{};
    std::vector<std::size_t> visible_order{};
    std::uint64_t revision{};
};

class PreparedTabRuntimeUpdates final {
public:
    PreparedTabRuntimeUpdates() noexcept;
    ~PreparedTabRuntimeUpdates();
    PreparedTabRuntimeUpdates(PreparedTabRuntimeUpdates&&) noexcept;
    PreparedTabRuntimeUpdates& operator=(
        PreparedTabRuntimeUpdates&&) noexcept;
    PreparedTabRuntimeUpdates(const PreparedTabRuntimeUpdates&) = delete;
    PreparedTabRuntimeUpdates& operator=(
        const PreparedTabRuntimeUpdates&) = delete;

    [[nodiscard]] explicit operator bool() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_{};

    friend ERUI_Result prepare_tab_runtime_updates(
        std::vector<TabRuntimeUpdate>,
        PreparedTabRuntimeUpdates&) noexcept;
    friend void commit_prepared_tab_runtime_updates(
        PreparedTabRuntimeUpdates&&) noexcept;
};

// Performs all allocations and ordering work while retaining the state lock.
// Destroying an uncommitted token leaves the complete prior generation intact.
[[nodiscard]] ERUI_Result prepare_tab_runtime_updates(
    std::vector<TabRuntimeUpdate> updates,
    PreparedTabRuntimeUpdates& output) noexcept;

void commit_prepared_tab_runtime_updates(
    PreparedTabRuntimeUpdates&& prepared) noexcept;

// Binds public provider/page identities to immutable catalogue stable indices.
// Registration-time updates are folded into the first applied generation.
[[nodiscard]] bool configure_tab_update_state(
    const CompiledMenu& menu,
    const NativeTabCatalogue& catalogue) noexcept;

[[nodiscard]] bool tab_updates_pending() noexcept;

[[nodiscard]] std::shared_ptr<const TabRuntimeGeneration>
acquire_applied_tab_generation() noexcept;

// The native tab-strip adapter prepares a complete projection from this
// immutable snapshot before publishing it.  A revision is acknowledged only
// after the native records, selector geometry, title storage, and page route
// have all reached that same generation.
[[nodiscard]] std::shared_ptr<const TabRuntimeGeneration>
acquire_desired_tab_generation() noexcept;

[[nodiscard]] bool publish_applied_tab_generation(
    const std::shared_ptr<const TabRuntimeGeneration>& generation) noexcept;

// Formats an explicit title override owned by the applied native tab
// generation. Returns false for untouched tabs so their registered static or
// formatter-driven PagePresentation remains authoritative. Both the selector
// and tab-main/tab-continuation headings therefore observe the same revision.
// Pagination matches MenuCompiler's "Title (n/t)" form.
[[nodiscard]] bool format_applied_tab_page_title(
    std::size_t logical_page_index,
    std::size_t slice_index,
    std::size_t slice_count,
    wchar_t* output,
    std::size_t output_capacity,
    std::uint64_t& out_revision) noexcept;

void reset_tab_update_state() noexcept;

} // namespace erui::detail
