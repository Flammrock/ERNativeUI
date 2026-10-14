#include "host_registry.hpp"
#include "menu_compiler.hpp"

#include "test_assertions.hpp"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace {

ERUI_StringView ascii(const char* value) noexcept {
    return {
        value,
        static_cast<std::uint32_t>(std::char_traits<char>::length(value)),
        0,
    };
}

ERUI_Utf16View utf16(std::wstring_view value) noexcept {
    return {
        reinterpret_cast<const std::uint16_t*>(value.data()),
        static_cast<std::uint32_t>(value.size()),
        0,
    };
}

struct NumericObservation {
    unsigned calls{};
    ERUI_NumericSliderChangeContext context{};
};

struct CustomObservation {
    unsigned calls{};
    ERUI_CustomSliderChangeContext context{};
};

void ERUI_CALL numeric_changed(
    void* user_data,
    const ERUI_NumericSliderChangeContext* context) noexcept {
    auto& observation = *static_cast<NumericObservation*>(user_data);
    ERUI_TEST_CHECK(context != nullptr);
    ERUI_TEST_CHECK(context->size == sizeof(*context));
    ERUI_TEST_CHECK(context->flags == 0);
    ERUI_TEST_CHECK(context->value.reserved == 0);
    ERUI_TEST_CHECK(context->reserved[0] == 0);
    ERUI_TEST_CHECK(context->reserved[1] == 0);
    ++observation.calls;
    observation.context = *context;
}

void ERUI_CALL custom_changed(
    void* user_data,
    const ERUI_CustomSliderChangeContext* context) noexcept {
    auto& observation = *static_cast<CustomObservation*>(user_data);
    ERUI_TEST_CHECK(context != nullptr);
    ERUI_TEST_CHECK(context->size == sizeof(*context));
    ERUI_TEST_CHECK(context->flags == 0);
    ERUI_TEST_CHECK(context->reserved[0] == 0);
    ERUI_TEST_CHECK(context->reserved[1] == 0);
    ++observation.calls;
    observation.context = *context;
}

} // namespace

int main() {
    erui::host::Registry registry{};
    registry.open_registration();

    ERUI_ProviderDesc legacy_provider{};
    legacy_provider.size = sizeof(legacy_provider);
    legacy_provider.api_version = ERUI_API_VERSION_1_1;
    legacy_provider.owner_module = GetModuleHandleW(nullptr);
    legacy_provider.provider_id = ascii("slider-registry-legacy");
    legacy_provider.display_name = utf16(L"Legacy slider registry");
    ERUI_ProviderHandle legacy_handle{};
    ERUI_PageHandle legacy_root{};
    ERUI_TEST_CHECK(registry.register_provider(
        ERUI_API_VERSION_1_1,
        &legacy_provider,
        &legacy_handle,
        &legacy_root) == ERUI_OK);
    ERUI_NumericSliderDesc unavailable{};
    unavailable.size = sizeof(unavailable);
    unavailable.label = utf16(L"Unavailable");
    unavailable.minimum_scaled_value = 0;
    unavailable.maximum_scaled_value = 1;
    unavailable.step_scaled_value = 1;
    unavailable.enabled = 1;
    ERUI_RowHandle untouched{UINT64_C(0xA55AA55AA55AA55A)};
    ERUI_TEST_CHECK(registry.add_numeric_slider(
        legacy_handle, legacy_root, &unavailable, &untouched) ==
        ERUI_NOT_SUPPORTED);
    ERUI_TEST_CHECK(untouched == UINT64_C(0xA55AA55AA55AA55A));
    ERUI_TEST_CHECK(registry.abort_provider(legacy_handle) == ERUI_OK);

    ERUI_ProviderDesc provider{};
    provider.size = sizeof(provider);
    provider.api_version = ERUI_API_VERSION_1_2;
    provider.owner_module = GetModuleHandleW(nullptr);
    provider.provider_id = ascii("slider-registry-current");
    provider.display_name = utf16(L"Slider registry current");
    ERUI_ProviderHandle provider_handle{};
    ERUI_PageHandle root{};
    ERUI_TEST_CHECK(registry.register_provider(
        ERUI_API_VERSION_1_2,
        &provider,
        &provider_handle,
        &root) == ERUI_OK);

    NumericObservation numeric_observation{};
    ERUI_NumericSliderDesc numeric{};
    numeric.size = sizeof(numeric);
    numeric.label = utf16(L"Signed decimal");
    numeric.help = utf16(L"Exact fixed-point slider");
    numeric.changed_callback = &numeric_changed;
    numeric.user_data = &numeric_observation;
    numeric.minimum_scaled_value = -10;
    numeric.maximum_scaled_value = 10;
    numeric.step_scaled_value = 2;
    numeric.initial_scaled_value = -5;
    numeric.decimal_places = 1;
    numeric.enabled = 1;
    numeric.prefix = utf16(L"x");
    numeric.suffix = utf16(L"s");
    ERUI_RowHandle numeric_row{};
    ERUI_TEST_CHECK(registry.add_numeric_slider(
        provider_handle, root, &numeric, &numeric_row) == ERUI_OK);

    std::array<std::wstring, 7> source_values{
        L"-7", L"3", L"7.8", L"1s", L"hello", L"18", L"x1.3"};
    std::array<ERUI_Utf16View, source_values.size()> views{};
    for (std::size_t index = 0; index < source_values.size(); ++index) {
        views[index] = utf16(source_values[index]);
    }
    CustomObservation custom_observation{};
    ERUI_CustomSliderDesc custom{};
    custom.size = sizeof(custom);
    custom.label = utf16(L"Crazy values");
    custom.help = utf16(L"Arbitrary display strings");
    custom.changed_callback = &custom_changed;
    custom.user_data = &custom_observation;
    custom.values = views.data();
    custom.value_count = static_cast<std::uint32_t>(views.size());
    custom.initial_index = 4;
    custom.enabled = 1;
    ERUI_RowHandle custom_row{};
    ERUI_TEST_CHECK(registry.add_custom_slider(
        provider_handle, root, &custom, &custom_row) == ERUI_OK);

    /* Exercise the complete signed domain and the native 256-position
       ceiling. UINT64_MAX is exactly divisible by 255. */
    constexpr std::int64_t full_range_step =
        static_cast<std::int64_t>(UINT64_MAX / 255u);
    ERUI_NumericSliderDesc full_range{};
    full_range.size = sizeof(full_range);
    full_range.label = utf16(L"Full signed range");
    full_range.minimum_scaled_value =
        (std::numeric_limits<std::int64_t>::min)();
    full_range.maximum_scaled_value =
        (std::numeric_limits<std::int64_t>::max)();
    full_range.step_scaled_value = full_range_step;
    full_range.initial_scaled_value =
        (std::numeric_limits<std::int64_t>::max)();
    full_range.enabled = 1;
    ERUI_RowHandle full_range_row{};
    ERUI_TEST_CHECK(registry.add_numeric_slider(
        provider_handle, root, &full_range, &full_range_row) == ERUI_OK);

    std::vector<std::wstring> maximum_custom_values{};
    std::vector<ERUI_Utf16View> maximum_custom_views{};
    maximum_custom_values.reserve(ERUI_SLIDER_MAX_VALUE_COUNT);
    maximum_custom_views.reserve(ERUI_SLIDER_MAX_VALUE_COUNT);
    for (std::uint32_t index = 0;
         index < ERUI_SLIDER_MAX_VALUE_COUNT;
         ++index) {
        maximum_custom_values.push_back(L"v" + std::to_wstring(index));
    }
    for (const std::wstring& value : maximum_custom_values) {
        maximum_custom_views.push_back(utf16(value));
    }
    ERUI_CustomSliderDesc maximum_custom{};
    maximum_custom.size = sizeof(maximum_custom);
    maximum_custom.label = utf16(L"Maximum custom values");
    maximum_custom.values = maximum_custom_views.data();
    maximum_custom.value_count = ERUI_SLIDER_MAX_VALUE_COUNT;
    maximum_custom.initial_index = ERUI_SLIDER_MAX_VALUE_COUNT - 1u;
    maximum_custom.enabled = 1;
    ERUI_RowHandle maximum_custom_row{};
    ERUI_TEST_CHECK(registry.add_custom_slider(
        provider_handle, root, &maximum_custom, &maximum_custom_row) ==
        ERUI_OK);

    /* Descriptors and display strings are borrowed only for the add call. */
    for (std::wstring& value : source_values) value.assign(L"overwritten");

    ERUI_TEST_CHECK(registry.commit_provider(provider_handle) == ERUI_OK);

    ERUI_DecimalValue numeric_value{};
    ERUI_TEST_CHECK(registry.get_numeric_slider_value(
        provider_handle, numeric_row, &numeric_value) == ERUI_OK);
    ERUI_TEST_CHECK(numeric_value.scaled_value == -6);
    ERUI_TEST_CHECK(numeric_value.decimal_places == 1);
    ERUI_TEST_CHECK(numeric_value.reserved == 0);
    std::uint32_t custom_index{};
    ERUI_TEST_CHECK(registry.get_custom_slider_index(
        provider_handle, custom_row, &custom_index) == ERUI_OK);
    ERUI_TEST_CHECK(custom_index == 4);
    ERUI_TEST_CHECK(registry.set_row_value(
        provider_handle, numeric_row, 0) == ERUI_INVALID_ARGUMENT);
    std::uint8_t generic_value{};
    ERUI_TEST_CHECK(registry.get_row_value(
        provider_handle, custom_row, &generic_value) == ERUI_INVALID_ARGUMENT);

    std::unique_ptr<erui::Menu> menu = registry.freeze_and_build();
    ERUI_TEST_CHECK(menu != nullptr);
    std::unique_ptr<erui::detail::CompiledMenu> compiled =
        erui::detail::MenuCompiler::compile(*menu);
    ERUI_TEST_CHECK(compiled != nullptr);
    ERUI_TEST_CHECK(compiled->modeled_numeric_slider_count == 2);
    ERUI_TEST_CHECK(compiled->modeled_custom_slider_count == 2);
    ERUI_TEST_CHECK(compiled->root_page().rows.size() == 4);

    const auto numeric_it = std::find_if(
        compiled->root_page().rows.begin(),
        compiled->root_page().rows.end(),
        [](const erui::detail::CompiledRow& row) {
            return row.kind == erui::RowKind::numeric_slider;
        });
    const auto custom_it = std::find_if(
        compiled->root_page().rows.begin(),
        compiled->root_page().rows.end(),
        [](const erui::detail::CompiledRow& row) {
            return row.kind == erui::RowKind::custom_slider;
        });
    ERUI_TEST_CHECK(numeric_it != compiled->root_page().rows.end());
    ERUI_TEST_CHECK(custom_it != compiled->root_page().rows.end());
    ERUI_TEST_CHECK(numeric_it->slider.minimum == 0);
    ERUI_TEST_CHECK(numeric_it->slider.maximum == 10);
    ERUI_TEST_CHECK(numeric_it->slider.step == 1);
    ERUI_TEST_CHECK(numeric_it->slider_value_texts.size() == 11);
    ERUI_TEST_CHECK(numeric_it->slider_value_texts.front() == L"x-1.0s");
    ERUI_TEST_CHECK(numeric_it->slider_value_texts[5] == L"x0.0s");
    ERUI_TEST_CHECK(numeric_it->slider_value_texts.back() == L"x1.0s");
    ERUI_TEST_CHECK(*numeric_it->byte_value == 2);
    ERUI_TEST_CHECK(custom_it->slider.minimum == 0);
    ERUI_TEST_CHECK(custom_it->slider.maximum == 6);
    ERUI_TEST_CHECK(custom_it->slider_value_texts.size() == 7);
    ERUI_TEST_CHECK(custom_it->slider_value_texts[0] == L"-7");
    ERUI_TEST_CHECK(custom_it->slider_value_texts[4] == L"hello");
    ERUI_TEST_CHECK(custom_it->slider_value_texts[6] == L"x1.3");
    ERUI_TEST_CHECK(*custom_it->byte_value == 4);

    const auto full_range_it = std::find_if(
        compiled->root_page().rows.begin(),
        compiled->root_page().rows.end(),
        [](const erui::detail::CompiledRow& row) {
            return row.kind == erui::RowKind::numeric_slider &&
                row.slider_value_texts.size() == ERUI_SLIDER_MAX_VALUE_COUNT;
        });
    const auto maximum_custom_it = std::find_if(
        compiled->root_page().rows.begin(),
        compiled->root_page().rows.end(),
        [](const erui::detail::CompiledRow& row) {
            return row.kind == erui::RowKind::custom_slider &&
                row.slider_value_texts.size() == ERUI_SLIDER_MAX_VALUE_COUNT;
        });
    ERUI_TEST_CHECK(full_range_it != compiled->root_page().rows.end());
    ERUI_TEST_CHECK(maximum_custom_it != compiled->root_page().rows.end());
    ERUI_TEST_CHECK(full_range_it->slider.maximum == 255);
    ERUI_TEST_CHECK(*full_range_it->byte_value == 255);
    ERUI_TEST_CHECK(full_range_it->slider_value_texts.front() ==
        L"-9223372036854775808");
    ERUI_TEST_CHECK(full_range_it->slider_value_texts.back() ==
        L"9223372036854775807");
    ERUI_TEST_CHECK(maximum_custom_it->slider.maximum == 255);
    ERUI_TEST_CHECK(*maximum_custom_it->byte_value == 255);
    ERUI_TEST_CHECK(maximum_custom_it->slider_value_texts.front() == L"v0");
    ERUI_TEST_CHECK(maximum_custom_it->slider_value_texts.back() == L"v255");

    /* Programmatic writes update the canonical/native values without
       masquerading as player changes when the runtime observes them. */
    ERUI_DecimalValue programmed_numeric{3, 1, 0};
    ERUI_TEST_CHECK(registry.set_numeric_slider_value(
        provider_handle, numeric_row, &programmed_numeric) == ERUI_OK);
    ERUI_TEST_CHECK(registry.set_custom_slider_index(
        provider_handle, custom_row, 6) == ERUI_OK);
    ERUI_TEST_CHECK(numeric_observation.calls == 0);
    ERUI_TEST_CHECK(custom_observation.calls == 0);
    registry.apply_pending_values();
    ERUI_TEST_CHECK(*numeric_it->byte_value == 6);
    ERUI_TEST_CHECK(*custom_it->byte_value == 6);
    numeric_it->value_action.invoke(*numeric_it->byte_value);
    custom_it->value_action.invoke(*custom_it->byte_value);
    ERUI_TEST_CHECK(numeric_observation.calls == 0);
    ERUI_TEST_CHECK(custom_observation.calls == 0);

    /* Native/player changes carry both the stable logical index and the
       exact public value/count through the typed callbacks. */
    *numeric_it->byte_value = 7;
    numeric_it->value_action.invoke(7);
    ERUI_TEST_CHECK(numeric_observation.calls == 1);
    ERUI_TEST_CHECK(numeric_observation.context.provider == provider_handle);
    ERUI_TEST_CHECK(numeric_observation.context.row == numeric_row);
    ERUI_TEST_CHECK(numeric_observation.context.value.scaled_value == 4);
    ERUI_TEST_CHECK(numeric_observation.context.value.decimal_places == 1);
    ERUI_TEST_CHECK(numeric_observation.context.index == 7);
    ERUI_TEST_CHECK(numeric_observation.context.value_count == 11);
    ERUI_TEST_CHECK(registry.get_numeric_slider_value(
        provider_handle, numeric_row, &numeric_value) == ERUI_OK);
    ERUI_TEST_CHECK(numeric_value.scaled_value == 4);

    *custom_it->byte_value = 1;
    custom_it->value_action.invoke(1);
    ERUI_TEST_CHECK(custom_observation.calls == 1);
    ERUI_TEST_CHECK(custom_observation.context.provider == provider_handle);
    ERUI_TEST_CHECK(custom_observation.context.row == custom_row);
    ERUI_TEST_CHECK(custom_observation.context.index == 1);
    ERUI_TEST_CHECK(custom_observation.context.value_count == 7);
    ERUI_TEST_CHECK(registry.get_custom_slider_index(
        provider_handle, custom_row, &custom_index) == ERUI_OK);
    ERUI_TEST_CHECK(custom_index == 1);

    return 0;
}
