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
    ERUI_TEST_CHECK(registry.enable_updates_backend());
    ERUI_TEST_CHECK(registry.enable_row_configuration_backend());
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
    provider.api_version = ERUI_API_VERSION_1_4;
    provider.owner_module = GetModuleHandleW(nullptr);
    provider.provider_id = ascii("slider-registry-current");
    provider.display_name = utf16(L"Slider registry current");
    ERUI_ProviderHandle provider_handle{};
    ERUI_PageHandle root{};
    ERUI_TEST_CHECK(registry.register_provider(
        ERUI_API_VERSION_1_4,
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

    // API 1.4 copies complete same-cardinality label tables and publishes
    // both slider families through the ordinary atomic row-update envelope.
    std::array<std::wstring, 11> numeric_replacements{};
    std::array<ERUI_Utf16View, numeric_replacements.size()>
        numeric_replacement_views{};
    for (std::size_t index = 0;
         index < numeric_replacements.size();
         ++index) {
        numeric_replacements[index] =
            L"numeric-" + std::to_wstring(index);
        numeric_replacement_views[index] = utf16(
            numeric_replacements[index]);
    }
    std::array<std::wstring, 7> custom_replacements{};
    std::array<ERUI_Utf16View, custom_replacements.size()>
        custom_replacement_views{};
    for (std::size_t index = 0;
         index < custom_replacements.size();
         ++index) {
        custom_replacements[index] =
            L"custom-" + std::to_wstring(index);
        custom_replacement_views[index] = utf16(
            custom_replacements[index]);
    }

    ERUI_NumericSliderControlUpdateDesc numeric_control_payload{};
    numeric_control_payload.size = sizeof(numeric_control_payload);
    numeric_control_payload.fields =
        ERUI_NUMERIC_SLIDER_CONTROL_UPDATE_FIELD_VALUE_LABELS;
    numeric_control_payload.value_labels =
        numeric_replacement_views.data();
    numeric_control_payload.value_label_count =
        static_cast<std::uint32_t>(numeric_replacement_views.size());
    numeric_control_payload.value_label_stride = sizeof(ERUI_Utf16View);
    ERUI_RowControlUpdateDesc numeric_control{};
    numeric_control.size = sizeof(numeric_control);
    numeric_control.kind =
        ERUI_ROW_CONTROL_UPDATE_KIND_NUMERIC_SLIDER;
    numeric_control.payload = &numeric_control_payload;
    ERUI_RowUpdateDesc numeric_update{};
    numeric_update.size = sizeof(numeric_update);
    numeric_update.row = numeric_row;
    numeric_update.fields =
        ERUI_ROW_UPDATE_FIELD_CONTROL_CONFIGURATION;
    numeric_update.control_configuration = &numeric_control;

    ERUI_CustomSliderControlUpdateDesc custom_control_payload{};
    custom_control_payload.size = sizeof(custom_control_payload);
    custom_control_payload.fields =
        ERUI_CUSTOM_SLIDER_CONTROL_UPDATE_FIELD_VALUES;
    custom_control_payload.values = custom_replacement_views.data();
    custom_control_payload.value_count =
        static_cast<std::uint32_t>(custom_replacement_views.size());
    custom_control_payload.value_stride = sizeof(ERUI_Utf16View);
    ERUI_RowControlUpdateDesc custom_control{};
    custom_control.size = sizeof(custom_control);
    custom_control.kind = ERUI_ROW_CONTROL_UPDATE_KIND_CUSTOM_SLIDER;
    custom_control.payload = &custom_control_payload;
    ERUI_RowUpdateDesc custom_update{};
    custom_update.size = sizeof(custom_update);
    custom_update.row = custom_row;
    custom_update.fields =
        ERUI_ROW_UPDATE_FIELD_CONTROL_CONFIGURATION;
    custom_update.control_configuration = &custom_control;

    std::array<ERUI_UpdateDesc, 2> control_updates{};
    control_updates[0].size = sizeof(ERUI_UpdateDesc);
    control_updates[0].kind = ERUI_UPDATE_KIND_ROW;
    control_updates[0].payload = &numeric_update;
    control_updates[1].size = sizeof(ERUI_UpdateDesc);
    control_updates[1].kind = ERUI_UPDATE_KIND_ROW;
    control_updates[1].payload = &custom_update;
    ERUI_UpdateBatchDesc control_batch{};
    control_batch.size = sizeof(control_batch);
    control_batch.updates = control_updates.data();
    control_batch.update_count =
        static_cast<std::uint32_t>(control_updates.size());
    control_batch.update_stride = sizeof(ERUI_UpdateDesc);

    const auto expect_invalid_control_batch = [&] {
        ERUI_TEST_CHECK(registry.commit_updates(
            provider_handle, &control_batch) == ERUI_INVALID_ARGUMENT);
    };

    // Establish the tables whose survival is checked after every malformed
    // nested descriptor and the later two-row transaction failure.
    ERUI_TEST_CHECK(registry.commit_updates(
        provider_handle, &control_batch) == ERUI_OK);

    numeric_control.kind = ERUI_ROW_CONTROL_UPDATE_KIND_CUSTOM_SLIDER;
    expect_invalid_control_batch();
    numeric_control.kind = ERUI_ROW_CONTROL_UPDATE_KIND_NUMERIC_SLIDER;

    --numeric_control_payload.size;
    expect_invalid_control_batch();
    numeric_control_payload.size = sizeof(numeric_control_payload);

    numeric_control_payload.flags = 1;
    expect_invalid_control_batch();
    numeric_control_payload.flags = 0;

    numeric_control_payload.reserved[0] = 1;
    expect_invalid_control_batch();
    numeric_control_payload.reserved[0] = 0;

    numeric_control_payload.fields = 0;
    expect_invalid_control_batch();
    numeric_control_payload.fields =
        ERUI_NUMERIC_SLIDER_CONTROL_UPDATE_FIELD_VALUE_LABELS;

    numeric_control_payload.value_label_stride = 1;
    expect_invalid_control_batch();
    numeric_control_payload.value_label_stride = sizeof(ERUI_Utf16View);

    const ERUI_Utf16View first_numeric_view =
        numeric_replacement_views.front();
    numeric_replacement_views.front() = utf16(L"");
    expect_invalid_control_batch();
    const std::wstring oversized_label(
        ERUI_CUSTOM_SLIDER_MAX_TEXT_LENGTH + 1u, L'x');
    numeric_replacement_views.front() = utf16(oversized_label);
    expect_invalid_control_batch();
    numeric_replacement_views.front() = first_numeric_view;

    numeric_update.fields = ERUI_ROW_UPDATE_FIELD_INTERACTIVE;
    numeric_update.interactive = 1;
    expect_invalid_control_batch();
    numeric_update.interactive = 0;
    numeric_update.fields = ERUI_ROW_UPDATE_FIELD_CONTROL_CONFIGURATION;

    std::array<std::wstring, 11> later_numeric_replacements{};
    std::array<ERUI_Utf16View, later_numeric_replacements.size()>
        later_numeric_views{};
    for (std::size_t index = 0;
         index < later_numeric_replacements.size();
         ++index) {
        later_numeric_replacements[index] =
            L"later-numeric-" + std::to_wstring(index);
        later_numeric_views[index] = utf16(later_numeric_replacements[index]);
    }
    std::array<std::wstring, 7> later_custom_replacements{};
    std::array<ERUI_Utf16View, later_custom_replacements.size()>
        later_custom_views{};
    for (std::size_t index = 0;
         index < later_custom_replacements.size();
         ++index) {
        later_custom_replacements[index] =
            L"later-custom-" + std::to_wstring(index);
        later_custom_views[index] = utf16(later_custom_replacements[index]);
    }
    numeric_control_payload.value_labels = later_numeric_views.data();
    custom_control_payload.values = later_custom_views.data();

    // The first row is valid and different, but the second row's wrong
    // cardinality rejects the complete batch atomically.
    const std::uint32_t valid_custom_count =
        custom_control_payload.value_count;
    custom_control_payload.value_count = valid_custom_count - 1u;
    expect_invalid_control_batch();

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

    // Freeze the post-failure model before accepting the later valid runtime
    // update. The compiled snapshot below must still contain the first tables.
    std::unique_ptr<erui::Menu> menu = registry.freeze_and_build();
    ERUI_TEST_CHECK(menu != nullptr);
    custom_control_payload.value_count = valid_custom_count;
    ERUI_TEST_CHECK(registry.commit_updates(
        provider_handle, &control_batch) == ERUI_OK);

    // Every nested client buffer is borrowed only for commit_updates.
    for (std::wstring& text : later_numeric_replacements) {
        text.assign(L"overwritten");
    }
    for (std::wstring& text : later_custom_replacements) {
        text.assign(L"overwritten");
    }

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
    ERUI_TEST_CHECK(numeric_it->slider_value_texts.front() == L"numeric-0");
    ERUI_TEST_CHECK(numeric_it->slider_value_texts[5] == L"numeric-5");
    ERUI_TEST_CHECK(numeric_it->slider_value_texts.back() == L"numeric-10");
    ERUI_TEST_CHECK(*numeric_it->byte_value == 2);
    ERUI_TEST_CHECK(custom_it->slider.minimum == 0);
    ERUI_TEST_CHECK(custom_it->slider.maximum == 6);
    ERUI_TEST_CHECK(custom_it->slider_value_texts.size() == 7);
    ERUI_TEST_CHECK(custom_it->slider_value_texts[0] == L"custom-0");
    ERUI_TEST_CHECK(custom_it->slider_value_texts[4] == L"custom-4");
    ERUI_TEST_CHECK(custom_it->slider_value_texts[6] == L"custom-6");
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
