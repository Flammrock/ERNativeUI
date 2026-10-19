#pragma once

#include "menu_compiler.hpp"
#include "tab_configuration.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace erui::detail {

// Elden Ring's Configuration strip has eight stock logical destinations.
// Categories 4 and 9 are deliberately absent: they are native carrier banks,
// not stock tabs. Stock titles and icon presentations remain movie-owned, so
// their catalogue records use a zero TextId and a fallback icon marker.
inline constexpr std::array<std::uint8_t, 8> native_stock_tab_categories{
    0u, 1u, 2u, 3u, 5u, 6u, 7u, 8u,
};
inline constexpr std::size_t native_stock_tab_count =
    native_stock_tab_categories.size();
inline constexpr TextId native_owned_tab_title_id = 0;

enum class NativeTabKind : std::uint8_t {
    stock,
    custom,
};

struct NativeTabCatalogueEntry final {
    NativeTabKind kind{NativeTabKind::stock};
    // Valid only for stock entries. Custom tabs are assigned a native carrier
    // by the runtime and retain invalid_native_category here.
    std::uint8_t native_category{invalid_native_category};
    // Zero for native-owned stock titles; a frozen TextRegistry ID for custom
    // tabs.
    TextId title_id{native_owned_tab_title_id};
    // May be invalid_compiled_index for a stock destination to which no
    // provider rows were registered. Every custom entry owns a valid page.
    std::size_t page_index{invalid_compiled_index};
    std::string provider_id{};
    std::string tab_id{};
    // Ordering and identity fields are meaningful for custom entries. Stock
    // entries keep their fixed native order and leave these at empty/zero.
    std::int32_t preferred_order{};
    std::optional<std::int32_t> player_order{};
    std::int32_t effective_order{};
    TabIconSpec icon{};

    [[nodiscard]] bool is_stock() const noexcept {
        return kind == NativeTabKind::stock;
    }

    [[nodiscard]] bool is_custom() const noexcept {
        return kind == NativeTabKind::custom;
    }
};

enum class NativeTabCatalogueError : std::uint8_t {
    none,
    invalid_configuration,
    menu_not_frozen,
    invalid_root_page,
    invalid_builtin_page,
    tab_index_list_mismatch,
    custom_tab_limit_exceeded,
    invalid_tab_page_index,
    duplicate_tab_page_index,
    tab_page_kind_mismatch,
    tab_page_unreachable,
    missing_tab_metadata,
    invalid_tab_identifier,
    duplicate_tab_identifier,
    invalid_tab_title,
    invalid_tab_icon,
    tab_order_mismatch,
    allocation_failure,
    unexpected_exception,
};

struct NativeTabCatalogueBuildResult;

class NativeTabCatalogue final {
public:
    NativeTabCatalogue(const NativeTabCatalogue&) = delete;
    NativeTabCatalogue& operator=(const NativeTabCatalogue&) = delete;
    NativeTabCatalogue(NativeTabCatalogue&&) = delete;
    NativeTabCatalogue& operator=(NativeTabCatalogue&&) = delete;
    ~NativeTabCatalogue() = default;

    [[nodiscard]] std::span<const NativeTabCatalogueEntry> entries()
        const noexcept {
        return entries_;
    }

    [[nodiscard]] std::span<const NativeTabCatalogueEntry> stock_entries()
        const noexcept {
        return std::span<const NativeTabCatalogueEntry>(entries_)
            .first(native_stock_tab_count);
    }

    [[nodiscard]] std::span<const NativeTabCatalogueEntry> custom_entries()
        const noexcept {
        return std::span<const NativeTabCatalogueEntry>(entries_)
            .subspan(native_stock_tab_count);
    }

    [[nodiscard]] std::size_t custom_count() const noexcept {
        return entries_.size() - native_stock_tab_count;
    }

    [[nodiscard]] const TabNavigationSettings& navigation_settings()
        const noexcept {
        return navigation_settings_;
    }

private:
    NativeTabCatalogue() = default;

    std::vector<NativeTabCatalogueEntry> entries_{};
    TabNavigationSettings navigation_settings_{};

    friend struct NativeTabCatalogueBuildResult;
    friend NativeTabCatalogueBuildResult build_native_tab_catalogue(
        const CompiledMenu&,
        const TabConfiguration&) noexcept;
};

struct NativeTabCatalogueBuildResult final {
    std::unique_ptr<const NativeTabCatalogue> catalogue{};
    NativeTabCatalogueError error{NativeTabCatalogueError::none};
    // Zero-based CompiledMenu::tabs index when a custom record caused the
    // failure; invalid_compiled_index when the failure is catalogue-wide.
    std::size_t tab_index{invalid_compiled_index};

    [[nodiscard]] explicit operator bool() const noexcept {
        return catalogue != nullptr && error == NativeTabCatalogueError::none;
    }
};

// Builds an all-or-nothing immutable logical catalogue. The CompiledMenu must
// come from a completed MenuCompiler pass. This boundary intentionally catches
// allocation and unexpected exceptions so native hook installation can fail
// closed instead of unwinding across loader/runtime code.
[[nodiscard]] NativeTabCatalogueBuildResult build_native_tab_catalogue(
    const CompiledMenu& menu,
    const TabConfiguration& configuration) noexcept;

[[nodiscard]] const char* native_tab_catalogue_error_name(
    NativeTabCatalogueError error) noexcept;

} // namespace erui::detail
