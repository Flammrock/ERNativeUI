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
    const erui::native::SharedMenuAddressProfile& left,
    const erui::native::SharedMenuAddressProfile& right) noexcept {
    return left.build == right.build &&
        left.game_options_handler_rva ==
            right.game_options_handler_rva &&
        left.sub_handler_rva == right.sub_handler_rva &&
        left.text_resolver_rva == right.text_resolver_rva &&
        left.slider_refresh_rva == right.slider_refresh_rva &&
        left.selector_select_rva == right.selector_select_rva;
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
        left.editor_factory_rva == right.editor_factory_rva &&
        left.option_setting_dialog_vtable_rva ==
            right.option_setting_dialog_vtable_rva &&
        left.pad_setting_dialog_vtable_rva ==
            right.pad_setting_dialog_vtable_rva &&
        left.edit_property_vtable_rva ==
            right.edit_property_vtable_rva &&
        left.text_input_controller_vtable_rva ==
            right.text_input_controller_vtable_rva &&
        left.action_button_controller_vtable_rva ==
            right.action_button_controller_vtable_rva &&
        left.software_keyboard_job_vtable_rva ==
            right.software_keyboard_job_vtable_rva;
}

bool same_profile(
    const erui::native::MenuRuntimeAddressProfile& left,
    const erui::native::MenuRuntimeAddressProfile& right) noexcept {
    return left.build == right.build &&
        left.camera_panel_materializer_rva == right.camera_panel_materializer_rva &&
        left.display_panel_materializer_rva == right.display_panel_materializer_rva &&
        left.sound_panel_materializer_rva == right.sound_panel_materializer_rva &&
        left.network_panel_materializer_rva == right.network_panel_materializer_rva &&
        left.keyboard_mouse_panel_materializer_rva == right.keyboard_mouse_panel_materializer_rva &&
        left.graphics_panel_materializer_rva == right.graphics_panel_materializer_rva &&
        left.open_sub_page_rva == right.open_sub_page_rva &&
        left.on_off_list_rva == right.on_off_list_rva &&
        left.menu_context_rva == right.menu_context_rva &&
        left.text_ref_help_rva == right.text_ref_help_rva &&
        left.text_ref_label_rva == right.text_ref_label_rva &&
        left.root_button_text_references_rva == right.root_button_text_references_rva &&
        left.root_button_display_text_rva == right.root_button_display_text_rva &&
        left.add_toggle_rva == right.add_toggle_rva &&
        left.add_inline_choice_rva == right.add_inline_choice_rva &&
        left.choice_context_constructor_rva == right.choice_context_constructor_rva &&
        left.choice_list_builder_rva == right.choice_list_builder_rva &&
        left.add_slider_rva == right.add_slider_rva &&
        left.add_button_rva == right.add_button_rva &&
        left.destroy_text_references_rva == right.destroy_text_references_rva &&
        left.native_back_rva == right.native_back_rva &&
        left.page_frame_rva == right.page_frame_rva &&
        left.scaleform_path_resolver_rva == right.scaleform_path_resolver_rva &&
        left.scaleform_text_setter_rva == right.scaleform_text_setter_rva &&
        left.popup_choice_constructor_rva == right.popup_choice_constructor_rva &&
        left.popup_choice_list_provider_rva == right.popup_choice_list_provider_rva &&
        left.popup_choice_list_template_rva == right.popup_choice_list_template_rva &&
        left.popup_choice_value_invoke_rva == right.popup_choice_value_invoke_rva &&
        left.popup_choice_selection_invoke_rva == right.popup_choice_selection_invoke_rva &&
        left.popup_choice_value_vtable_rva == right.popup_choice_value_vtable_rva &&
        left.popup_choice_selection_vtable_rva == right.popup_choice_selection_vtable_rva &&
        left.popup_choice_presentation_vtable_rva == right.popup_choice_presentation_vtable_rva;
}

bool same_profile(
    const erui::native::SelectedHelpPresenterAddressProfile& left,
    const erui::native::SelectedHelpPresenterAddressProfile& right) noexcept {
    return left.build == right.build &&
        left.image_size == right.image_size &&
        left.presenter_rva == right.presenter_rva &&
        left.clone_wrapper_rva == right.clone_wrapper_rva &&
        left.inner_clone_rva == right.inner_clone_rva &&
        left.two_owned_string_cleanup_rva ==
            right.two_owned_string_cleanup_rva &&
        left.owned_string_clone_rva == right.owned_string_clone_rva &&
        left.help_thunk_rva == right.help_thunk_rva;
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
    static_assert(
        erui::native::identify_elden_ring_build(
            0x6AABF7E7, 0x5DFD600) ==
        EldenRingBuild::v2_7_2_0);

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
        erui::native::identify_elden_ring_build(
            0x6AABF7E7, 0x5E0DA00) ==
        EldenRingBuild::unsupported);
    ERUI_TEST_CHECK(
        erui::native::identify_elden_ring_build(
            0x6A96B418, 0x5DFD600) ==
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
            EldenRingBuild::v2_7_2_0) == "2.7.2.0");
    ERUI_TEST_CHECK(
        erui::native::elden_ring_build_name(
            EldenRingBuild::unsupported) == "unsupported");

    const auto* const core_270 =
        erui::native::find_core_ui_address_profile(
            EldenRingBuild::v2_7_0_0);
    const auto* const core_271 =
        erui::native::find_core_ui_address_profile(
            EldenRingBuild::v2_7_1_0);
    const auto* const core_272 =
        erui::native::find_core_ui_address_profile(
            EldenRingBuild::v2_7_2_0);
    ERUI_TEST_CHECK(core_270 && core_271 && core_272);
    ERUI_TEST_CHECK(same_profile(
        *core_270,
        erui::native::CoreUiAddressProfile{
            EldenRingBuild::v2_7_0_0, 0xD81590}));
    ERUI_TEST_CHECK(same_profile(
        *core_271,
        erui::native::CoreUiAddressProfile{
            EldenRingBuild::v2_7_1_0, 0xD81600}));
    ERUI_TEST_CHECK(same_profile(
        *core_272,
        erui::native::CoreUiAddressProfile{
            EldenRingBuild::v2_7_2_0, 0xD82CF0}));

    const auto* const shared_270 =
        erui::native::find_shared_menu_address_profile(
            EldenRingBuild::v2_7_0_0);
    const auto* const shared_271 =
        erui::native::find_shared_menu_address_profile(
            EldenRingBuild::v2_7_1_0);
    const auto* const shared_272 =
        erui::native::find_shared_menu_address_profile(
            EldenRingBuild::v2_7_2_0);
    ERUI_TEST_CHECK(shared_270 && shared_271 && shared_272);
    ERUI_TEST_CHECK(same_profile(
        *shared_270,
        erui::native::SharedMenuAddressProfile{
            EldenRingBuild::v2_7_0_0,
            0x958C50, 0x95A5D0, 0x7633D0, 0x74FA20, 0x739B90}));
    ERUI_TEST_CHECK(same_profile(
        *shared_271,
        erui::native::SharedMenuAddressProfile{
            EldenRingBuild::v2_7_1_0,
            0x959DF0, 0x95B770, 0x764220, 0x74FA20, 0x739B90}));
    ERUI_TEST_CHECK(same_profile(
        *shared_272,
        erui::native::SharedMenuAddressProfile{
            EldenRingBuild::v2_7_2_0,
            0x95A2A0, 0x95BC20, 0x7646D0, 0x74FED0, 0x73A040}));

    const auto* const text_270 =
        erui::native::find_text_input_address_profile(
            EldenRingBuild::v2_7_0_0);
    const auto* const text_271 =
        erui::native::find_text_input_address_profile(
            EldenRingBuild::v2_7_1_0);
    const auto* const text_272 =
        erui::native::find_text_input_address_profile(
            EldenRingBuild::v2_7_2_0);
    ERUI_TEST_CHECK(text_270 && text_271 && text_272);
    ERUI_TEST_CHECK(same_profile(
        *text_270,
        erui::native::TextInputAddressProfile{
            EldenRingBuild::v2_7_0_0,
            0x976EF0, 0x5EE0F0, 0x6766F0, 0x1BCC60,
            0x915D70, 0x81D610, 0x2B14888, 0x2B16DD8,
            0x2AD73D0, 0x2B1B128, 0x2AD7430, 0x2AC5AD0}));
    ERUI_TEST_CHECK(same_profile(
        *text_271,
        erui::native::TextInputAddressProfile{
            EldenRingBuild::v2_7_1_0,
            0x976EF0, 0x5EE0F0, 0x6766F0, 0x1BCC60,
            0x915D70, 0x81D610, 0x2B14888, 0x2B16DD8,
            0x2AD73D0, 0x2B1B128, 0x2AD7430, 0x2AC5AD0}));
    ERUI_TEST_CHECK(same_profile(
        *text_272,
        erui::native::TextInputAddressProfile{
            EldenRingBuild::v2_7_2_0,
            0x9773A0, 0x5EE5A0, 0x676BA0, 0x1BCC60,
            0x916220, 0x81DAC0, 0x2B16978, 0x2B18EC8,
            0x2AD94C0, 0x2B1D218, 0x2AD9520, 0x2AC7BC0}));

    const auto* const menu_270 =
        erui::native::find_menu_runtime_address_profile(
            EldenRingBuild::v2_7_0_0);
    const auto* const menu_271 =
        erui::native::find_menu_runtime_address_profile(
            EldenRingBuild::v2_7_1_0);
    const auto* const menu_272 =
        erui::native::find_menu_runtime_address_profile(
            EldenRingBuild::v2_7_2_0);
    ERUI_TEST_CHECK(menu_270 && menu_271 && menu_272);
    ERUI_TEST_CHECK(same_profile(
        *menu_270,
        erui::native::MenuRuntimeAddressProfile{
            EldenRingBuild::v2_7_0_0,
            0x959320, 0x95D540, 0x959090, 0x95AD00,
            0x95CB30, 0x95C050, 0x94FA20, 0x955550,
            0x869300, 0x760790, 0x760970, 0x958210,
            0x763BA0, 0x948FA0, 0x949120, 0x86A410,
            0x956CF0, 0x9493F0, 0x9298A0, 0x742C90,
            0x747CD0, 0x958FF0, 0x74B140, 0x74AE50,
            0x95D010, 0x9623A0, 0x86A610, 0x962520,
            0x962330, 0x2B15A78, 0x2B15AB0, 0x2B15AE8}));
    ERUI_TEST_CHECK(same_profile(
        *menu_271,
        erui::native::MenuRuntimeAddressProfile{
            EldenRingBuild::v2_7_1_0,
            0x959320, 0x95D540, 0x959090, 0x95AD00,
            0x95CB30, 0x95C050, 0x950850, 0x955550,
            0x86A2F0, 0x7615E0, 0x7617C0, 0x958210,
            0x763BA0, 0x94A140, 0x94A2C0, 0x86A410,
            0x956CF0, 0x94A590, 0x92AA40, 0x743AE0,
            0x747CD0, 0x958FF0, 0x74B140, 0x74AE50,
            0x95D010, 0x9623A0, 0x86A610, 0x962520,
            0x962330, 0x2B15A78, 0x2B15AB0, 0x2B15AE8}));
    ERUI_TEST_CHECK(same_profile(
        *menu_272,
        erui::native::MenuRuntimeAddressProfile{
            EldenRingBuild::v2_7_2_0,
            0x9597D0, 0x95D9F0, 0x959540, 0x95B1B0,
            0x95CFE0, 0x95C500, 0x950D00, 0x955A00,
            0x86A7A0, 0x761A90, 0x761C70, 0x9586C0,
            0x764050, 0x94A5F0, 0x94A770, 0x86A8C0,
            0x9571A0, 0x94AA40, 0x92AEF0, 0x743F90,
            0x748180, 0x9594A0, 0x74B5F0, 0x74B300,
            0x95D4C0, 0x962850, 0x86AAC0, 0x9629D0,
            0x9627E0, 0x2B17B68, 0x2B17BA0, 0x2B17BD8}));

    const auto* const help_270 =
        erui::native::find_selected_help_presenter_address_profile(
            EldenRingBuild::v2_7_0_0);
    const auto* const help_271 =
        erui::native::find_selected_help_presenter_address_profile(
            EldenRingBuild::v2_7_1_0);
    const auto* const help_272 =
        erui::native::find_selected_help_presenter_address_profile(
            EldenRingBuild::v2_7_2_0);
    ERUI_TEST_CHECK(help_270 == nullptr);
    ERUI_TEST_CHECK(help_271 && help_272);
    ERUI_TEST_CHECK(same_profile(
        *help_271,
        erui::native::SelectedHelpPresenterAddressProfile{
            EldenRingBuild::v2_7_1_0,
            0x5E0DA00,
            0x977BE0,
            0x93D300,
            0x78EB60,
            0x7373A0,
            0x117910,
            0x7469C0}));
    ERUI_TEST_CHECK(same_profile(
        *help_272,
        erui::native::SelectedHelpPresenterAddressProfile{
            EldenRingBuild::v2_7_2_0,
            0x5DFD600,
            0x978090,
            0x93D7B0,
            0x78F010,
            0x737850,
            0x117910,
            0x746E70}));

    const auto* const color_270 =
        erui::native::find_color_picker_address_profile(
            EldenRingBuild::v2_7_0_0);
    const auto* const color_271 =
        erui::native::find_color_picker_address_profile(
            EldenRingBuild::v2_7_1_0);
    const auto* const color_272 =
        erui::native::find_color_picker_address_profile(
            EldenRingBuild::v2_7_2_0);
    ERUI_TEST_CHECK(color_270 && color_271 && color_272);
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
    ERUI_TEST_CHECK(same_profile(
        *color_272,
        erui::native::ColorPickerAddressProfile{
            EldenRingBuild::v2_7_2_0,
            0x7ADE30, 0x7AA580, 0x7A85D0, 0x1EBD430,
            0x77CAD0, 0x77CEC0, 0x77D440, 0x746580,
            0xD82CF0, 0x8B7440, 0x2ABAED8, 0x74B5F0,
            0x86BDF0, 0x734640, 0x734450, 0xD86D70}));

    const auto* const input_270 =
        erui::native::find_input_bindings_address_profile(
            EldenRingBuild::v2_7_0_0);
    const auto* const input_271 =
        erui::native::find_input_bindings_address_profile(
            EldenRingBuild::v2_7_1_0);
    const auto* const input_272 =
        erui::native::find_input_bindings_address_profile(
            EldenRingBuild::v2_7_2_0);
    ERUI_TEST_CHECK(input_270 && input_271 && input_272);
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
    ERUI_TEST_CHECK(same_profile(
        *input_272,
        erui::native::InputBindingsAddressProfile{
            EldenRingBuild::v2_7_2_0,
            0x869A30, 0x869B60, 0x86A480, 0x8684B0,
            0x869110, 0x868C80, 0x868CC0, 0x868110,
            0x242960, 0x240220, 0x2403C0, 0x2413F0,
            0x2669270, 0x266BC20, 0x4864D50, 0x81D160,
            0x81D2F0, 0x9BA2B0, 0x9BA470, 0x2B0FDB0,
            0x2AD8F60, 0x2AD89C0, 0x2AC7BC0, 0x2B2D9F8}));

    ERUI_TEST_CHECK(
        erui::native::find_core_ui_address_profile(
            EldenRingBuild::unsupported) == nullptr);
    ERUI_TEST_CHECK(
        erui::native::find_shared_menu_address_profile(
            EldenRingBuild::unsupported) == nullptr);
    ERUI_TEST_CHECK(
        erui::native::find_text_input_address_profile(
            EldenRingBuild::unsupported) == nullptr);
    ERUI_TEST_CHECK(
        erui::native::find_menu_runtime_address_profile(
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
