#include "game_build_profiles.hpp"

#include "test_assertions.hpp"

#include <cstddef>

namespace {

bool same_profile(
    const erui::native::CoreUiAddressProfile& left,
    const erui::native::CoreUiAddressProfile& right) noexcept {
    return left.build == right.build &&
        left.scaleform_result_destructor_rva ==
            right.scaleform_result_destructor_rva;
}

bool same_profile(
    const erui::native::TextInputAddressProfile& left,
    const erui::native::TextInputAddressProfile& right) noexcept {
    return left.build == right.build &&
        left.row_producer_rva == right.row_producer_rva &&
        left.menu_string_constructor_rva ==
            right.menu_string_constructor_rva &&
        left.menu_string_borrowed_constructor_rva ==
            right.menu_string_borrowed_constructor_rva &&
        left.menu_string_destructor_rva ==
            right.menu_string_destructor_rva &&
        left.editor_factory_builder_rva ==
            right.editor_factory_builder_rva &&
        left.editor_factory_rva == right.editor_factory_rva;
}

bool same_profile(
    const erui::native::ColorPickerAddressProfile& left,
    const erui::native::ColorPickerAddressProfile& right) noexcept {
    return left.build == right.build &&
        left.build_menu_window_job_rva ==
            right.build_menu_window_job_rva &&
        left.submit_consume_rva == right.submit_consume_rva &&
        left.heap_provider_rva == right.heap_provider_rva &&
        left.game_allocate_rva == right.game_allocate_rva &&
        left.color_palette_constructor_rva ==
            right.color_palette_constructor_rva &&
        left.color_palette_destructor_rva ==
            right.color_palette_destructor_rva &&
        left.color_palette_populate_rva ==
            right.color_palette_populate_rva &&
        left.scene_proxy_bridge_rva == right.scene_proxy_bridge_rva &&
        left.scene_obj_proxy_destructor_rva ==
            right.scene_obj_proxy_destructor_rva &&
        left.color_control_constructor_rva ==
            right.color_control_constructor_rva &&
        left.color_picker_movie_name_rva ==
            right.color_picker_movie_name_rva &&
        left.scaleform_path_resolver_rva ==
            right.scaleform_path_resolver_rva &&
        left.game_options_action_widget_producer_rva ==
            right.game_options_action_widget_producer_rva &&
        left.scaleform_visibility_setter_rva ==
            right.scaleform_visibility_setter_rva &&
        left.scaleform_value_exists_rva ==
            right.scaleform_value_exists_rva &&
        left.scaleform_color_transform_setter_rva ==
            right.scaleform_color_transform_setter_rva;
}

bool same_profile(
    const erui::native::InputBindingsAddressProfile& left,
    const erui::native::InputBindingsAddressProfile& right) noexcept {
    return left.build == right.build &&
        left.build_key_setting_list_rva ==
            right.build_key_setting_list_rva &&
        left.construct_spacer_rva == right.construct_spacer_rva &&
        left.append_key_setting_rva == right.append_key_setting_rva &&
        left.construct_key_setting_rva ==
            right.construct_key_setting_rva &&
        left.poll_key_capture_rva == right.poll_key_capture_rva &&
        left.clear_key_setting_rva == right.clear_key_setting_rva &&
        left.refresh_key_conflict_rva == right.refresh_key_conflict_rva &&
        left.write_menu_config_rva == right.write_menu_config_rva &&
        left.write_binding_value_rva == right.write_binding_value_rva &&
        left.input_token_physical_id_rva ==
            right.input_token_physical_id_rva &&
        left.input_token_analog_rva == right.input_token_analog_rva &&
        left.get_player_input_rva == right.get_player_input_rva &&
        left.query_input_states_rva == right.query_input_states_rva &&
        left.input_manager_update_rva == right.input_manager_update_rva &&
        left.input_manager_slot_rva == right.input_manager_slot_rva &&
        left.software_keyboard_job_construct_rva ==
            right.software_keyboard_job_construct_rva &&
        left.software_keyboard_job_destroy_rva ==
            right.software_keyboard_job_destroy_rva &&
        left.text_input_dialog_construct_rva ==
            right.text_input_dialog_construct_rva &&
        left.text_input_dialog_destroy_rva ==
            right.text_input_dialog_destroy_rva &&
        left.key_config_dialog_vtable_rva ==
            right.key_config_dialog_vtable_rva &&
        left.key_setting_list_vtable_rva ==
            right.key_setting_list_vtable_rva &&
        left.key_config_vtable_rva == right.key_config_vtable_rva &&
        left.software_keyboard_job_vtable_rva ==
            right.software_keyboard_job_vtable_rva &&
        left.text_input_dialog_vtable_rva ==
            right.text_input_dialog_vtable_rva;
}

} // namespace

int main() {
    using erui::native::EldenRingBuild;

    static_assert(
        erui::native::identify_elden_ring_build(
            0x69E9C9B9, 0x5E09600) ==
        EldenRingBuild::v2_7_0_0);
    static_assert(
        erui::native::identify_elden_ring_build(
            0x6A96B418, 0x5E0DA00) ==
        EldenRingBuild::v2_7_1_0);

    // A timestamp or image-size match by itself is not accepted. This keeps
    // an accidental hybrid profile from reaching any native call boundary.
    ERUI_TEST_CHECK(
        erui::native::identify_elden_ring_build(
            0x69E9C9B9, 0x5E0DA00) ==
        EldenRingBuild::unsupported);
    ERUI_TEST_CHECK(
        erui::native::identify_elden_ring_build(
            0x6A96B418, 0x5E09600) ==
        EldenRingBuild::unsupported);
    ERUI_TEST_CHECK(
        erui::native::identify_elden_ring_build(0, 0) ==
        EldenRingBuild::unsupported);

    ERUI_TEST_CHECK(
        erui::native::elden_ring_build_name(
            EldenRingBuild::v2_7_0_0) == "2.7.0.0");
    ERUI_TEST_CHECK(
        erui::native::elden_ring_build_name(
            EldenRingBuild::v2_7_1_0) == "2.7.1.0");
    ERUI_TEST_CHECK(
        erui::native::elden_ring_build_name(
            EldenRingBuild::unsupported) == "unsupported");

    const auto* const core_270 =
        erui::native::find_core_ui_address_profile(
            EldenRingBuild::v2_7_0_0);
    const auto* const core_271 =
        erui::native::find_core_ui_address_profile(
            EldenRingBuild::v2_7_1_0);
    ERUI_TEST_CHECK(core_270 && core_271);
    ERUI_TEST_CHECK(same_profile(
        *core_270,
        erui::native::CoreUiAddressProfile{
            EldenRingBuild::v2_7_0_0, 0xD81590}));
    ERUI_TEST_CHECK(same_profile(
        *core_271,
        erui::native::CoreUiAddressProfile{
            EldenRingBuild::v2_7_1_0, 0xD81600}));

    const auto* const text_270 =
        erui::native::find_text_input_address_profile(
            EldenRingBuild::v2_7_0_0);
    const auto* const text_271 =
        erui::native::find_text_input_address_profile(
            EldenRingBuild::v2_7_1_0);
    ERUI_TEST_CHECK(text_270 && text_271);
    ERUI_TEST_CHECK(same_profile(
        *text_270,
        erui::native::TextInputAddressProfile{
            EldenRingBuild::v2_7_0_0,
            0x976EF0, 0x5EE0F0, 0x6766F0, 0x1BCC60,
            0x915D70, 0x81D610}));
    ERUI_TEST_CHECK(same_profile(
        *text_271,
        erui::native::TextInputAddressProfile{
            EldenRingBuild::v2_7_1_0,
            0x976EF0, 0x5EE0F0, 0x6766F0, 0x1BCC60,
            0x915D70, 0x81D610}));

    const auto* const color_270 =
        erui::native::find_color_picker_address_profile(
            EldenRingBuild::v2_7_0_0);
    const auto* const color_271 =
        erui::native::find_color_picker_address_profile(
            EldenRingBuild::v2_7_1_0);
    ERUI_TEST_CHECK(color_270 && color_271);
    ERUI_TEST_CHECK(same_profile(
        *color_270,
        erui::native::ColorPickerAddressProfile{
            EldenRingBuild::v2_7_0_0,
            0x7AD980, 0x7AA0D0, 0x7A8120, 0x1EBBCD0,
            0x77C620, 0x77CA10, 0x77CF90, 0x7460D0,
            0xD81590, 0x8B6F90, 0x2AB8DE8, 0x74B140,
            0x86B940, 0x734190, 0x733FA0, 0xD85610}));
    ERUI_TEST_CHECK(same_profile(
        *color_271,
        erui::native::ColorPickerAddressProfile{
            EldenRingBuild::v2_7_1_0,
            0x7AD980, 0x7AA0D0, 0x7A8120, 0x1EBBD40,
            0x77C620, 0x77CA10, 0x77CF90, 0x7460D0,
            0xD81600, 0x8B6F90, 0x2AB8DE8, 0x74B140,
            0x86B940, 0x734190, 0x733FA0, 0xD85680}));

    const auto* const input_270 =
        erui::native::find_input_bindings_address_profile(
            EldenRingBuild::v2_7_0_0);
    const auto* const input_271 =
        erui::native::find_input_bindings_address_profile(
            EldenRingBuild::v2_7_1_0);
    ERUI_TEST_CHECK(input_270 && input_271);
    ERUI_TEST_CHECK(same_profile(
        *input_270,
        erui::native::InputBindingsAddressProfile{
            EldenRingBuild::v2_7_0_0,
            0x869580, 0x8696B0, 0x869FD0, 0x868000,
            0x868C60, 0x8687D0, 0x868810, 0x867C60,
            0x242960, 0x240220, 0x2403C0, 0x2413F0,
            0x2667AD0, 0x266A480, 0x4861D30, 0x81CCB0,
            0x81CE40, 0x9B9E00, 0x9B9FC0, 0x2B0DCC0,
            0x2AD6E70, 0x2AD68D0, 0x2AC5AD0, 0x2B2B908}));
    ERUI_TEST_CHECK(same_profile(
        *input_271,
        erui::native::InputBindingsAddressProfile{
            EldenRingBuild::v2_7_1_0,
            0x869580, 0x8696B0, 0x869FD0, 0x868000,
            0x868C60, 0x8687D0, 0x868810, 0x867C60,
            0x242960, 0x240220, 0x2403C0, 0x2413F0,
            0x2667B40, 0x266A4F0, 0x4861D30, 0x81CCB0,
            0x81CE40, 0x9B9E00, 0x9B9FC0, 0x2B0DCC0,
            0x2AD6E70, 0x2AD68D0, 0x2AC5AD0, 0x2B2B908}));

    ERUI_TEST_CHECK(
        erui::native::find_core_ui_address_profile(
            EldenRingBuild::unsupported) == nullptr);
    ERUI_TEST_CHECK(
        erui::native::find_text_input_address_profile(
            EldenRingBuild::unsupported) == nullptr);
    ERUI_TEST_CHECK(
        erui::native::find_color_picker_address_profile(
            EldenRingBuild::unsupported) == nullptr);
    ERUI_TEST_CHECK(
        erui::native::find_input_bindings_address_profile(
            EldenRingBuild::unsupported) == nullptr);

    for (std::size_t left = 0;
         left < erui::native::kSupportedEldenRingBuilds.size(); ++left) {
        const auto& identity =
            erui::native::kSupportedEldenRingBuilds[left];
        ERUI_TEST_CHECK(
            erui::native::find_elden_ring_build(
                identity.pe_timestamp, identity.image_size) == &identity);
        for (std::size_t right = left + 1;
             right < erui::native::kSupportedEldenRingBuilds.size();
             ++right) {
            const auto& other =
                erui::native::kSupportedEldenRingBuilds[right];
            ERUI_TEST_CHECK(identity.build != other.build);
            ERUI_TEST_CHECK(
                identity.pe_timestamp != other.pe_timestamp ||
                identity.image_size != other.image_size);
        }
    }

    return 0;
}
