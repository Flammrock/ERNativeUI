#include "native_tab_catalogue.hpp"

#include <algorithm>
#include <limits>
#include <new>
#include <utility>

namespace erui::detail {
namespace {

// These are the accepted [Tabs] parser bounds. Keeping the final publication
// gate strict means a hand-constructed or corrupted configuration cannot feed
// unbounded timing values into the native adapter.
constexpr std::uint32_t kMaximumNavigationMargin = 3;
constexpr std::uint64_t kMinimumRepeatDelayMs = 20;
constexpr std::uint64_t kMaximumDwellDelayMs = 5000;

struct ValidationFailure final {
    NativeTabCatalogueError error{NativeTabCatalogueError::none};
    std::size_t tab_index{invalid_compiled_index};

    [[nodiscard]] explicit operator bool() const noexcept {
        return error != NativeTabCatalogueError::none;
    }
};

[[nodiscard]] constexpr ValidationFailure fail(
    NativeTabCatalogueError error,
    std::size_t tab_index = invalid_compiled_index) noexcept {
    return {error, tab_index};
}

[[nodiscard]] bool valid_navigation_settings(
    const TabNavigationSettings& settings) noexcept {
    const TabPointerDwellTiming& timing = settings.mouse_dwell_timing;
    return settings.navigation_margin <= kMaximumNavigationMargin &&
        settings.minimum_tab_slots >= kDefaultMinimumTabSlots &&
        settings.minimum_tab_slots <=
            kMaximumConfigurableMinimumTabSlots &&
        timing.initial_delay_ms <= kMaximumDwellDelayMs &&
        timing.repeat_delay_ms >= kMinimumRepeatDelayMs &&
        timing.repeat_delay_ms <= kMaximumDwellDelayMs &&
        timing.repeat_decay_ms <= kMaximumDwellDelayMs &&
        timing.minimum_repeat_delay_ms >= kMinimumRepeatDelayMs &&
        timing.minimum_repeat_delay_ms <= timing.repeat_delay_ms;
}

[[nodiscard]] bool valid_configuration(
    const TabConfiguration& configuration) noexcept {
    if (!valid_navigation_settings(configuration.settings) ||
        configuration.order_overrides.size() >
            kMaximumTabOrderOverrides) {
        return false;
    }
    for (const auto& [stable_key, override] :
         configuration.order_overrides) {
        std::string_view provider_id{};
        std::string_view tab_id{};
        if (!split_tab_stable_key(
                stable_key, provider_id, tab_id) ||
            override.stable_key != stable_key ||
            override.provider_id != provider_id ||
            override.tab_id != tab_id) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool is_stock_category(std::uint8_t category) noexcept {
    return std::find(
        native_stock_tab_categories.begin(),
        native_stock_tab_categories.end(),
        category) != native_stock_tab_categories.end();
}

[[nodiscard]] ValidationFailure validate_builtin_pages(
    const CompiledMenu& menu) noexcept {
    if (menu.pages.empty() ||
        menu.root_page_index >= menu.pages.size() ||
        menu.pages[menu.root_page_index].kind != PageKind::root ||
        !menu.page_reachable(menu.root_page_index) ||
        menu.builtin_page_index(0) != menu.root_page_index ||
        menu.pages[menu.root_page_index].builtin_page !=
            BuiltinPage::game_options) {
        return fail(NativeTabCatalogueError::invalid_root_page);
    }

    for (std::uint8_t category = 0;
         category < menu.builtin_page_indices.size();
         ++category) {
        const std::size_t page_index = menu.builtin_page_index(category);
        if (!is_stock_category(category)) {
            if (page_index != invalid_compiled_index) {
                return fail(NativeTabCatalogueError::invalid_builtin_page);
            }
            continue;
        }
        // Controller Settings (category 6) has no ERNativeUI row
        // destination. Its stock page is still represented in the logical
        // catalogue, but it must never alias a compiled provider page.
        if (category == 6u) {
            if (page_index != invalid_compiled_index) {
                return fail(NativeTabCatalogueError::invalid_builtin_page);
            }
            continue;
        }
        if (page_index == invalid_compiled_index) {
            if (category == 0u) {
                return fail(NativeTabCatalogueError::invalid_root_page);
            }
            continue;
        }
        if (page_index >= menu.pages.size() ||
            menu.pages[page_index].kind != PageKind::root ||
            !menu.page_reachable(page_index) ||
            !valid_builtin_page(menu.pages[page_index].builtin_page) ||
            native_category_id(menu.pages[page_index].builtin_page) !=
                category) {
            return fail(NativeTabCatalogueError::invalid_builtin_page);
        }
    }
    return {};
}

[[nodiscard]] bool valid_tab_icon(const TabIconSpec& icon) noexcept {
    if (icon.source == TabIconSource::fallback) {
        return icon.rgba8_pixels.empty();
    }
    if (icon.source != TabIconSource::rgba8 ||
        icon.rgba8_pixels.size() != ERUI_TAB_ICON_RGBA8_BYTE_COUNT) {
        return false;
    }
    return true;
}

[[nodiscard]] bool valid_tab_title(
    const CompiledMenu& menu,
    std::size_t page_index,
    TextId title_id) noexcept {
    if (title_id == native_owned_tab_title_id ||
        title_id > static_cast<TextId>(
            (std::numeric_limits<int>::max)())) {
        return false;
    }
    const wchar_t* const title = menu.texts.lookup(
        static_cast<int>(title_id));
    const TextBinding* const binding = menu.text_binding(title_id);
    return title && title[0] != L'\0' && binding &&
        std::char_traits<wchar_t>::length(title) <=
            ERUI_CUSTOM_TAB_TITLE_MAX_CODE_UNITS &&
        binding->role == TextRole::page_title &&
        binding->page_index == page_index;
}

[[nodiscard]] ValidationFailure validate_custom_tabs(
    const CompiledMenu& menu,
    std::vector<TabOrderCandidate>& candidates) {
    if (menu.tabs.size() != menu.tab_page_indices.size()) {
        return fail(NativeTabCatalogueError::tab_index_list_mismatch);
    }
    if (menu.tabs.size() > ERUI_CUSTOM_TAB_MAX_COUNT) {
        return fail(NativeTabCatalogueError::custom_tab_limit_exceeded);
    }

    candidates.reserve(menu.tabs.size());
    for (std::size_t index = 0; index < menu.tabs.size(); ++index) {
        const CompiledTab& tab = menu.tabs[index];
        if (tab.page_index != menu.tab_page_indices[index] ||
            tab.page_index >= menu.pages.size()) {
            return fail(
                NativeTabCatalogueError::invalid_tab_page_index, index);
        }
        for (std::size_t previous = 0; previous < index; ++previous) {
            if (menu.tabs[previous].page_index == tab.page_index) {
                return fail(
                    NativeTabCatalogueError::duplicate_tab_page_index,
                    index);
            }
        }

        const CompiledPage& page = menu.pages[tab.page_index];
        if (page.kind != PageKind::tab ||
            valid_builtin_page(page.builtin_page)) {
            return fail(
                NativeTabCatalogueError::tab_page_kind_mismatch, index);
        }
        if (!menu.page_reachable(tab.page_index)) {
            return fail(
                NativeTabCatalogueError::tab_page_unreachable, index);
        }
        if (!tab.metadata) {
            return fail(
                NativeTabCatalogueError::missing_tab_metadata, index);
        }
        const TabMetadata& metadata = *tab.metadata;
        if (!is_valid_tab_identifier(metadata.provider_id) ||
            !is_valid_tab_identifier(metadata.tab_id)) {
            return fail(
                NativeTabCatalogueError::invalid_tab_identifier, index);
        }
        for (std::size_t previous = 0; previous < index; ++previous) {
            const std::optional<TabMetadata>& previous_metadata =
                menu.tabs[previous].metadata;
            if (previous_metadata &&
                previous_metadata->provider_id == metadata.provider_id &&
                previous_metadata->tab_id == metadata.tab_id) {
                return fail(
                    NativeTabCatalogueError::duplicate_tab_identifier,
                    index);
            }
        }
        if (!valid_tab_title(menu, tab.page_index, page.title_id)) {
            return fail(NativeTabCatalogueError::invalid_tab_title, index);
        }
        if (!valid_tab_icon(metadata.icon)) {
            return fail(NativeTabCatalogueError::invalid_tab_icon, index);
        }
        candidates.push_back({
            .provider_id = metadata.provider_id,
            .tab_id = metadata.tab_id,
            .preferred_order = metadata.preferred_order,
            .source_index = index,
        });
    }
    return {};
}

[[nodiscard]] ValidationFailure validate_resolved_order(
    const CompiledMenu& menu,
    const TabOrderResolution& resolution) noexcept {
    if (!resolution.complete ||
        resolution.ordered_tabs.size() != menu.tabs.size()) {
        return fail(NativeTabCatalogueError::tab_order_mismatch);
    }
    for (std::size_t index = 0;
         index < resolution.ordered_tabs.size();
         ++index) {
        const ResolvedTabOrderEntry& resolved =
            resolution.ordered_tabs[index];
        if (resolved.source_index != index ||
            resolved.position != index + 1 ||
            !menu.tabs[index].metadata) {
            return fail(
                NativeTabCatalogueError::tab_order_mismatch, index);
        }
        const TabMetadata& metadata = *menu.tabs[index].metadata;
        if (metadata.provider_id != resolved.provider_id ||
            metadata.tab_id != resolved.tab_id ||
            metadata.preferred_order != resolved.preferred_order ||
            metadata.player_order != resolved.player_override ||
            metadata.effective_order != resolved.effective_order) {
            return fail(
                NativeTabCatalogueError::tab_order_mismatch, index);
        }
    }
    return {};
}

[[nodiscard]] NativeTabCatalogueBuildResult failure_result(
    ValidationFailure failure) noexcept {
    return {
        .catalogue = {},
        .error = failure.error,
        .tab_index = failure.tab_index,
    };
}

} // namespace

NativeTabCatalogueBuildResult build_native_tab_catalogue(
    const CompiledMenu& menu,
    const TabConfiguration& configuration) noexcept {
    try {
        if (!valid_configuration(configuration)) {
            return failure_result(fail(
                NativeTabCatalogueError::invalid_configuration));
        }
        if (!menu.texts.frozen()) {
            return failure_result(fail(
                NativeTabCatalogueError::menu_not_frozen));
        }
        if (const ValidationFailure failure =
                validate_builtin_pages(menu)) {
            return failure_result(failure);
        }

        std::vector<TabOrderCandidate> candidates{};
        if (const ValidationFailure failure =
                validate_custom_tabs(menu, candidates)) {
            return failure_result(failure);
        }
        const TabOrderResolution resolution = resolve_tab_order(
            candidates, configuration);
        if (const ValidationFailure failure =
                validate_resolved_order(menu, resolution)) {
            return failure_result(failure);
        }

        std::unique_ptr<NativeTabCatalogue> catalogue(
            new NativeTabCatalogue());
        catalogue->entries_.reserve(
            native_stock_tab_count + menu.tabs.size());
        catalogue->navigation_settings_ = configuration.settings;

        for (const std::uint8_t category :
             native_stock_tab_categories) {
            const std::size_t page_index = category == 6u
                ? invalid_compiled_index
                : menu.builtin_page_index(category);
            catalogue->entries_.push_back({
                .kind = NativeTabKind::stock,
                .native_category = category,
                .title_id = native_owned_tab_title_id,
                .page_index = page_index,
            });
        }
        for (const CompiledTab& tab : menu.tabs) {
            const TabMetadata& metadata = *tab.metadata;
            catalogue->entries_.push_back({
                .kind = NativeTabKind::custom,
                .native_category = invalid_native_category,
                .title_id = menu.pages[tab.page_index].title_id,
                .page_index = tab.page_index,
                .provider_id = metadata.provider_id,
                .tab_id = metadata.tab_id,
                .preferred_order = metadata.preferred_order,
                .player_order = metadata.player_order,
                .effective_order = metadata.effective_order,
                .icon = metadata.icon,
            });
        }

        NativeTabCatalogueBuildResult result{};
        result.catalogue = std::move(catalogue);
        return result;
    } catch (const std::bad_alloc&) {
        return failure_result(fail(
            NativeTabCatalogueError::allocation_failure));
    } catch (...) {
        return failure_result(fail(
            NativeTabCatalogueError::unexpected_exception));
    }
}

const char* native_tab_catalogue_error_name(
    NativeTabCatalogueError error) noexcept {
    switch (error) {
    case NativeTabCatalogueError::none: return "none";
    case NativeTabCatalogueError::invalid_configuration:
        return "invalid_configuration";
    case NativeTabCatalogueError::menu_not_frozen:
        return "menu_not_frozen";
    case NativeTabCatalogueError::invalid_root_page:
        return "invalid_root_page";
    case NativeTabCatalogueError::invalid_builtin_page:
        return "invalid_builtin_page";
    case NativeTabCatalogueError::tab_index_list_mismatch:
        return "tab_index_list_mismatch";
    case NativeTabCatalogueError::custom_tab_limit_exceeded:
        return "custom_tab_limit_exceeded";
    case NativeTabCatalogueError::invalid_tab_page_index:
        return "invalid_tab_page_index";
    case NativeTabCatalogueError::duplicate_tab_page_index:
        return "duplicate_tab_page_index";
    case NativeTabCatalogueError::tab_page_kind_mismatch:
        return "tab_page_kind_mismatch";
    case NativeTabCatalogueError::tab_page_unreachable:
        return "tab_page_unreachable";
    case NativeTabCatalogueError::missing_tab_metadata:
        return "missing_tab_metadata";
    case NativeTabCatalogueError::invalid_tab_identifier:
        return "invalid_tab_identifier";
    case NativeTabCatalogueError::duplicate_tab_identifier:
        return "duplicate_tab_identifier";
    case NativeTabCatalogueError::invalid_tab_title:
        return "invalid_tab_title";
    case NativeTabCatalogueError::invalid_tab_icon:
        return "invalid_tab_icon";
    case NativeTabCatalogueError::tab_order_mismatch:
        return "tab_order_mismatch";
    case NativeTabCatalogueError::allocation_failure:
        return "allocation_failure";
    case NativeTabCatalogueError::unexpected_exception:
        return "unexpected_exception";
    }
    return "unknown";
}

} // namespace erui::detail
