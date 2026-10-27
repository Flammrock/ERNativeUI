#pragma once

#include "menu_compiler.hpp"

#include <ernativeui/erui.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace erui::detail {

struct SubmenuTitleUpdate {
    ERUI_ProviderHandle provider{ERUI_INVALID_PROVIDER};
    ERUI_PageHandle page{ERUI_INVALID_PAGE};
    std::wstring title{};
    // Conservative upper bound supplied by the registry. It lets the client
    // thread prove every default " (n/t)" variant fits before publication.
    std::size_t maximum_slice_count{1};
};

class PreparedSubmenuTitleUpdates {
public:
    PreparedSubmenuTitleUpdates() noexcept;
    PreparedSubmenuTitleUpdates(PreparedSubmenuTitleUpdates&&) noexcept;
    PreparedSubmenuTitleUpdates& operator=(
        PreparedSubmenuTitleUpdates&&) noexcept;
    ~PreparedSubmenuTitleUpdates();

    PreparedSubmenuTitleUpdates(const PreparedSubmenuTitleUpdates&) = delete;
    PreparedSubmenuTitleUpdates& operator=(
        const PreparedSubmenuTitleUpdates&) = delete;

    [[nodiscard]] explicit operator bool() const noexcept;

private:
    struct State;
    std::unique_ptr<State> state_{};

    friend ERUI_Result prepare_submenu_title_updates(
        std::vector<SubmenuTitleUpdate>,
        PreparedSubmenuTitleUpdates&) noexcept;
    friend void commit_prepared_submenu_title_updates(
        PreparedSubmenuTitleUpdates&&) noexcept;
};

// Performs all validation and allocation without changing the desired
// generation. The returned token can be committed through no-throw swaps.
[[nodiscard]] ERUI_Result prepare_submenu_title_updates(
    std::vector<SubmenuTitleUpdate> updates,
    PreparedSubmenuTitleUpdates& output) noexcept;

void commit_prepared_submenu_title_updates(
    PreparedSubmenuTitleUpdates&& prepared) noexcept;

// Binds stable provider/page identities to compiled logical page indices.
// Updates accepted before compilation are preserved and validated here.
[[nodiscard]] bool configure_submenu_update_state(
    CompiledMenu& menu) noexcept;

// Publishes desired shared strings by pointer assignment only. No allocation
// or client callback occurs at the UI boundary.
void synchronize_pending_submenu_updates() noexcept;

// Formats the applied title override into caller-owned storage. Returns false
// when this page has no live override. The default pagination form exactly
// matches MenuCompiler: "Title (n/t)".
[[nodiscard]] bool format_submenu_page_title(
    std::size_t submenu_update_slot,
    std::size_t slice_index,
    std::size_t slice_count,
    wchar_t* output,
    std::size_t output_capacity,
    std::uint64_t& out_revision) noexcept;

[[nodiscard]] bool submenu_updates_pending() noexcept;

void reset_submenu_update_state() noexcept;

} // namespace erui::detail
