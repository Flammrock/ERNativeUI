#pragma once

#include "game_build.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace erui::native {

// Build-locked native interfaces are kept here so profile selection can be
// tested independently of the injected runtime. Each backend still validates
// selected function entries and applies its existing local checks to data,
// vtables, and live objects before use.
struct CoreUiAddressProfile {
    EldenRingBuild build{};
    std::uintptr_t scaleform_result_destructor_rva{};
};

// Entry points shared with menu-overhaul mods are captured at process attach,
// including the Selector::select viewport seam used by API 1.3 custom tabs.
// Exact-build RVAs make that snapshot bounded and deterministic: unlike
// ordinary late resolution, this path performs no executable-wide signature
// scans while another DLL's worker is racing to install hooks. Each entry may
// contain either its canonical game prefix or an already-installed,
// subsequently validated compatibility detour.
struct SharedMenuAddressProfile {
    EldenRingBuild build{};
    std::uintptr_t game_options_handler_rva{};
    std::uintptr_t sub_handler_rva{};
    std::uintptr_t text_resolver_rva{};
    std::uintptr_t slider_refresh_rva{};
    std::uintptr_t selector_select_rva{};
};

inline constexpr std::array<SharedMenuAddressProfile, 3>
    kSharedMenuAddressProfiles{{
        {
            EldenRingBuild::v2_7_0_0,
            0x958C50,
            0x95A5D0,
            0x7633D0,
            0x74FA20,
            0x739B90,
        },
        {
            EldenRingBuild::v2_7_1_0,
            0x959DF0,
            0x95B770,
            0x764220,
            0x74FA20,
            0x739B90,
        },
        {
            EldenRingBuild::v2_7_2_0,
            0x95A2A0,
            0x95BC20,
            0x7646D0,
            0x74FED0,
            0x73A040,
        },
    }};

inline constexpr std::array<CoreUiAddressProfile, 3>
    kCoreUiAddressProfiles{{
        {
            EldenRingBuild::v2_7_0_0,
            0xD81590,
        },
        {
            EldenRingBuild::v2_7_1_0,
            0xD81600,
        },
        {
            EldenRingBuild::v2_7_2_0,
            0xD82CF0,
        },
    }};

struct TextInputAddressProfile {
    EldenRingBuild build{};
    std::uintptr_t row_producer_rva{};
    std::uintptr_t menu_string_constructor_rva{};
    std::uintptr_t menu_string_borrowed_constructor_rva{};
    std::uintptr_t menu_string_destructor_rva{};
    std::uintptr_t editor_factory_builder_rva{};
    std::uintptr_t editor_factory_rva{};
    std::uintptr_t option_setting_dialog_vtable_rva{};
    std::uintptr_t pad_setting_dialog_vtable_rva{};
    std::uintptr_t edit_property_vtable_rva{};
    std::uintptr_t text_input_controller_vtable_rva{};
    std::uintptr_t software_keyboard_job_vtable_rva{};
};

inline constexpr std::array<TextInputAddressProfile, 3>
    kTextInputAddressProfiles{{
        {
            EldenRingBuild::v2_7_0_0,
            0x976EF0,
            0x5EE0F0,
            0x6766F0,
            0x1BCC60,
            0x915D70,
            0x81D610,
            0x2B14888,
            0x2B16DD8,
            0x2AD73D0,
            0x2B1B128,
            0x2AC5AD0,
        },
        {
            EldenRingBuild::v2_7_1_0,
            0x976EF0,
            0x5EE0F0,
            0x6766F0,
            0x1BCC60,
            0x915D70,
            0x81D610,
            0x2B14888,
            0x2B16DD8,
            0x2AD73D0,
            0x2B1B128,
            0x2AC5AD0,
        },
        {
            EldenRingBuild::v2_7_2_0,
            0x9773A0,
            0x5EE5A0,
            0x676BA0,
            0x1BCC60,
            0x916220,
            0x81DAC0,
            0x2B16978,
            0x2B18EC8,
            0x2AD94C0,
            0x2B1D218,
            0x2AC7BC0,
        },
    }};

// Ordinary Game Options construction uses signatures when they are unique,
// but several constructors and callable bodies intentionally have duplicate
// byte sequences. Keep every fallback selected by the exact PE identity so a
// new game build can never inherit a plausible address from an older image.
struct MenuRuntimeAddressProfile {
    EldenRingBuild build{};
    std::uintptr_t camera_panel_materializer_rva{};
    std::uintptr_t display_panel_materializer_rva{};
    std::uintptr_t sound_panel_materializer_rva{};
    std::uintptr_t network_panel_materializer_rva{};
    std::uintptr_t keyboard_mouse_panel_materializer_rva{};
    std::uintptr_t graphics_panel_materializer_rva{};
    std::uintptr_t open_sub_page_rva{};
    std::uintptr_t on_off_list_rva{};
    std::uintptr_t menu_context_rva{};
    std::uintptr_t text_ref_help_rva{};
    std::uintptr_t text_ref_label_rva{};
    std::uintptr_t root_button_text_references_rva{};
    std::uintptr_t root_button_display_text_rva{};
    std::uintptr_t add_toggle_rva{};
    std::uintptr_t add_inline_choice_rva{};
    std::uintptr_t choice_context_constructor_rva{};
    std::uintptr_t choice_list_builder_rva{};
    std::uintptr_t add_slider_rva{};
    std::uintptr_t add_button_rva{};
    std::uintptr_t destroy_text_references_rva{};
    std::uintptr_t native_back_rva{};
    std::uintptr_t page_frame_rva{};
    std::uintptr_t scaleform_path_resolver_rva{};
    std::uintptr_t scaleform_text_setter_rva{};
    std::uintptr_t popup_choice_constructor_rva{};
    std::uintptr_t popup_choice_list_provider_rva{};
    std::uintptr_t popup_choice_list_template_rva{};
    std::uintptr_t popup_choice_value_invoke_rva{};
    std::uintptr_t popup_choice_selection_invoke_rva{};
    std::uintptr_t popup_choice_value_vtable_rva{};
    std::uintptr_t popup_choice_selection_vtable_rva{};
    std::uintptr_t popup_choice_presentation_vtable_rva{};
};

inline constexpr std::array<MenuRuntimeAddressProfile, 3>
    kMenuRuntimeAddressProfiles{{
        {
            EldenRingBuild::v2_7_0_0,
            0x959320, 0x95D540, 0x959090, 0x95AD00,
            0x95CB30, 0x95C050, 0x94FA20, 0x955550,
            0x869300, 0x760790, 0x760970, 0x958210,
            0x763BA0, 0x948FA0, 0x949120, 0x86A410,
            0x956CF0, 0x9493F0, 0x9298A0, 0x742C90,
            0x747CD0, 0x958FF0, 0x74B140, 0x74AE50,
            0x95D010, 0x9623A0, 0x86A610, 0x962520,
            0x962330, 0x2B15A78, 0x2B15AB0, 0x2B15AE8,
        },
        {
            EldenRingBuild::v2_7_1_0,
            0x959320, 0x95D540, 0x959090, 0x95AD00,
            0x95CB30, 0x95C050, 0x950850, 0x955550,
            0x86A2F0, 0x7615E0, 0x7617C0, 0x958210,
            0x763BA0, 0x94A140, 0x94A2C0, 0x86A410,
            0x956CF0, 0x94A590, 0x92AA40, 0x743AE0,
            0x747CD0, 0x958FF0, 0x74B140, 0x74AE50,
            0x95D010, 0x9623A0, 0x86A610, 0x962520,
            0x962330, 0x2B15A78, 0x2B15AB0, 0x2B15AE8,
        },
        {
            EldenRingBuild::v2_7_2_0,
            0x9597D0, 0x95D9F0, 0x959540, 0x95B1B0,
            0x95CFE0, 0x95C500, 0x950D00, 0x955A00,
            0x86A7A0, 0x761A90, 0x761C70, 0x9586C0,
            0x764050, 0x94A5F0, 0x94A770, 0x86A8C0,
            0x9571A0, 0x94AA40, 0x92AEF0, 0x743F90,
            0x748180, 0x9594A0, 0x74B5F0, 0x74B300,
            0x95D4C0, 0x962850, 0x86AAC0, 0x9629D0,
            0x9627E0, 0x2B17B68, 0x2B17BA0, 0x2B17BD8,
        },
    }};

struct ColorPickerAddressProfile {
    EldenRingBuild build{};
    std::uintptr_t build_menu_window_job_rva{};
    std::uintptr_t submit_consume_rva{};
    std::uintptr_t heap_provider_rva{};
    std::uintptr_t game_allocate_rva{};
    std::uintptr_t color_palette_constructor_rva{};
    std::uintptr_t color_palette_destructor_rva{};
    std::uintptr_t color_palette_populate_rva{};
    std::uintptr_t scene_proxy_bridge_rva{};
    std::uintptr_t scene_obj_proxy_destructor_rva{};
    std::uintptr_t color_control_constructor_rva{};
    std::uintptr_t color_picker_movie_name_rva{};
    std::uintptr_t scaleform_path_resolver_rva{};
    std::uintptr_t game_options_action_widget_producer_rva{};
    std::uintptr_t scaleform_visibility_setter_rva{};
    std::uintptr_t scaleform_value_exists_rva{};
    std::uintptr_t scaleform_color_transform_setter_rva{};
};

inline constexpr std::array<ColorPickerAddressProfile, 3>
    kColorPickerAddressProfiles{{
        {
            EldenRingBuild::v2_7_0_0,
            0x7AD980,
            0x7AA0D0,
            0x7A8120,
            0x1EBBCD0,
            0x77C620,
            0x77CA10,
            0x77CF90,
            0x7460D0,
            0xD81590,
            0x8B6F90,
            0x2AB8DE8,
            0x74B140,
            0x86B940,
            0x734190,
            0x733FA0,
            0xD85610,
        },
        {
            EldenRingBuild::v2_7_1_0,
            0x7AD980,
            0x7AA0D0,
            0x7A8120,
            0x1EBBD40,
            0x77C620,
            0x77CA10,
            0x77CF90,
            0x7460D0,
            0xD81600,
            0x8B6F90,
            0x2AB8DE8,
            0x74B140,
            0x86B940,
            0x734190,
            0x733FA0,
            0xD85680,
        },
        {
            EldenRingBuild::v2_7_2_0,
            0x7ADE30,
            0x7AA580,
            0x7A85D0,
            0x1EBD430,
            0x77CAD0,
            0x77CEC0,
            0x77D440,
            0x746580,
            0xD82CF0,
            0x8B7440,
            0x2ABAED8,
            0x74B5F0,
            0x86BDF0,
            0x734640,
            0x734450,
            0xD86D70,
        },
    }};

struct InputBindingsAddressProfile {
    EldenRingBuild build{};
    std::uintptr_t build_key_setting_list_rva{};
    std::uintptr_t construct_spacer_rva{};
    std::uintptr_t append_key_setting_rva{};
    std::uintptr_t construct_key_setting_rva{};
    std::uintptr_t poll_key_capture_rva{};
    std::uintptr_t clear_key_setting_rva{};
    std::uintptr_t refresh_key_conflict_rva{};
    std::uintptr_t write_menu_config_rva{};
    std::uintptr_t write_binding_value_rva{};
    std::uintptr_t input_token_physical_id_rva{};
    std::uintptr_t input_token_analog_rva{};
    std::uintptr_t get_player_input_rva{};
    std::uintptr_t query_input_states_rva{};
    std::uintptr_t input_manager_update_rva{};
    std::uintptr_t input_manager_slot_rva{};
    std::uintptr_t software_keyboard_job_construct_rva{};
    std::uintptr_t software_keyboard_job_destroy_rva{};
    std::uintptr_t text_input_dialog_construct_rva{};
    std::uintptr_t text_input_dialog_destroy_rva{};
    std::uintptr_t key_config_dialog_vtable_rva{};
    std::uintptr_t key_setting_list_vtable_rva{};
    std::uintptr_t key_config_vtable_rva{};
    std::uintptr_t software_keyboard_job_vtable_rva{};
    std::uintptr_t text_input_dialog_vtable_rva{};
};

inline constexpr std::array<InputBindingsAddressProfile, 3>
    kInputBindingsAddressProfiles{{
        {
            EldenRingBuild::v2_7_0_0,
            0x869580,
            0x8696B0,
            0x869FD0,
            0x868000,
            0x868C60,
            0x8687D0,
            0x868810,
            0x867C60,
            0x242960,
            0x240220,
            0x2403C0,
            0x2413F0,
            0x2667AD0,
            0x266A480,
            0x4861D30,
            0x81CCB0,
            0x81CE40,
            0x9B9E00,
            0x9B9FC0,
            0x2B0DCC0,
            0x2AD6E70,
            0x2AD68D0,
            0x2AC5AD0,
            0x2B2B908,
        },
        {
            EldenRingBuild::v2_7_1_0,
            0x869580,
            0x8696B0,
            0x869FD0,
            0x868000,
            0x868C60,
            0x8687D0,
            0x868810,
            0x867C60,
            0x242960,
            0x240220,
            0x2403C0,
            0x2413F0,
            0x2667B40,
            0x266A4F0,
            0x4861D30,
            0x81CCB0,
            0x81CE40,
            0x9B9E00,
            0x9B9FC0,
            0x2B0DCC0,
            0x2AD6E70,
            0x2AD68D0,
            0x2AC5AD0,
            0x2B2B908,
        },
        {
            EldenRingBuild::v2_7_2_0,
            0x869A30,
            0x869B60,
            0x86A480,
            0x8684B0,
            0x869110,
            0x868C80,
            0x868CC0,
            0x868110,
            0x242960,
            0x240220,
            0x2403C0,
            0x2413F0,
            0x2669270,
            0x266BC20,
            0x4864D50,
            0x81D160,
            0x81D2F0,
            0x9BA2B0,
            0x9BA470,
            0x2B0FDB0,
            0x2AD8F60,
            0x2AD89C0,
            0x2AC7BC0,
            0x2B2D9F8,
        },
    }};

template <typename Profile, std::size_t Count>
[[nodiscard]] constexpr const Profile* find_address_profile(
    const std::array<Profile, Count>& profiles,
    EldenRingBuild build) noexcept {
    for (const Profile& profile : profiles) {
        if (profile.build == build) return &profile;
    }
    return nullptr;
}

[[nodiscard]] constexpr const TextInputAddressProfile*
find_text_input_address_profile(EldenRingBuild build) noexcept {
    return find_address_profile(kTextInputAddressProfiles, build);
}

[[nodiscard]] constexpr const MenuRuntimeAddressProfile*
find_menu_runtime_address_profile(EldenRingBuild build) noexcept {
    return find_address_profile(kMenuRuntimeAddressProfiles, build);
}

[[nodiscard]] constexpr const CoreUiAddressProfile*
find_core_ui_address_profile(EldenRingBuild build) noexcept {
    return find_address_profile(kCoreUiAddressProfiles, build);
}

[[nodiscard]] constexpr const SharedMenuAddressProfile*
find_shared_menu_address_profile(EldenRingBuild build) noexcept {
    return find_address_profile(kSharedMenuAddressProfiles, build);
}

[[nodiscard]] constexpr const ColorPickerAddressProfile*
find_color_picker_address_profile(EldenRingBuild build) noexcept {
    return find_address_profile(kColorPickerAddressProfiles, build);
}

[[nodiscard]] constexpr const InputBindingsAddressProfile*
find_input_bindings_address_profile(EldenRingBuild build) noexcept {
    return find_address_profile(kInputBindingsAddressProfiles, build);
}

} // namespace erui::native
