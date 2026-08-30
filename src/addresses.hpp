#pragma once

#include "module.hpp"

#include "runtime.hpp"

#include <cstddef>
#include <cstdint>

namespace erui::native {

using HubHandlerFn = void(__fastcall*)(
    void* page,
    std::uintptr_t argument2,
    std::uintptr_t argument3,
    std::uintptr_t argument4,
    std::uintptr_t argument5,
    std::uintptr_t argument6);
using SubHandlerFn = void(__fastcall*)(
    void* page,
    std::uintptr_t argument2,
    std::uintptr_t argument3,
    std::uintptr_t argument4);
using OpenSubPageFn = void(__fastcall*)(void** parent_page_slot);
using NativeBackFn = void(__fastcall*)(
    void* page,
    std::uint64_t action);
// Concrete Game Options page frame dispatcher. MS x64 places the page in
// RCX, the elapsed/frame scalar in XMM1, and an input-enabled byte in R8.
using PageFrameFn = void(__fastcall*)(
    void* page,
    float frame_value,
    std::uint8_t* input_enabled);
using BufferConstructorFn = void*(__fastcall*)(void* destination);
using TextReferenceFn = void*(__fastcall*)(void* destination, std::uint32_t message_id);
using AddToggleFn = void(__fastcall*)(
    void* page,
    void* text_references,
    std::uint8_t* value,
    void* on_off_list,
    void* menu_context_member,
    bool enabled);
using AddSliderFn = void(__fastcall*)(
    void* page,
    void* text_references,
    std::uint8_t* value,
    void* range,
    std::uint8_t current_value,
    bool enabled);
// Native inline discrete-choice row. The fourth argument is the owned native
// option container and the fifth is its dedicated input-context object.
using AddInlineChoiceFn = void(__fastcall*)(
    void* page,
    void* text_references,
    std::uint8_t* selected_index,
    void* choice_list,
    void* menu_context);
using ChoiceContextConstructorFn = void*(__fastcall*)(void* destination);
using ChoiceListBuilderFn = void*(__fastcall*)(void* destination);

// Native action-style selector. Its erased callable inputs are game-owned
// ABI objects prepared by native_menu.cpp and retained by the constructed row.
using PopupChoiceConstructorFn = void*(__fastcall*)(
    void* page,
    void* text_references,
    void* value_provider,
    void* selection_action,
    void* presentation_provider);
using PopupChoiceListProviderFn = void*(__fastcall*)(
    void* provider,
    void* destination,
    std::uintptr_t argument3,
    std::uintptr_t argument4);
using PopupChoiceListTemplateFn = void*(__fastcall*)(
    void* list,
    std::uint8_t selected_index);

// Native Game Options action-row constructor. The fourth and fifth arguments
// point to MSVC std::function<void()> objects. They remain opaque here so the
// address layer does not expose STL types.
using AddButtonFn = void(__fastcall*)(
    void* page,
    void* text_references,
    void* display_text_reference,
    void* primary_action,
    void* secondary_action);
using DestroyTextReferencesFn = void(__fastcall*)(void* text_references);

// Five-argument native UI text resolver used by Game Options widgets.
using TextResolverFn = void*(__fastcall*)(
    void* text_result,
    std::uintptr_t argument2,
    std::uint32_t message_id,
    std::uintptr_t argument4,
    std::uintptr_t argument5);

// Private Scaleform helpers used only to present custom physical pages. The
// path resolver constructs a 0x60-byte native result object in destination;
// the value consumed by the setter begins eight bytes into that result and
// the nested member passed to the cleanup routine begins at offset 0x28.
using ScaleformPathResolverFn = void*(__fastcall*)(
    void* movie_context,
    void* destination,
    const char* path);
using ScaleformTextSetterFn = void(__fastcall*)(
    void* scaleform_value,
    const wchar_t* text);
using ScaleformResultDestructorFn = void(__fastcall*)(void* nested_member);

struct GameAddresses {
    std::uint8_t* game_image_base{};
    std::size_t game_image_size{};
    HubHandlerFn hub_handler{};
    SubHandlerFn sub_handler{};
    OpenSubPageFn open_sub_page{};
    NativeBackFn native_back{};
    PageFrameFn page_frame{};
    BufferConstructorFn on_off_list{};
    BufferConstructorFn menu_context{};
    TextReferenceFn text_ref_help{};
    TextReferenceFn text_ref_label{};
    // Constructs the richer label/help pair required by Controller-page
    // action rows (the same object used by vanilla Advanced Settings).
    BufferConstructorFn root_button_text_references{};
    TextReferenceFn root_button_display_text{};
    AddToggleFn add_toggle{};
    AddInlineChoiceFn add_inline_choice{};
    ChoiceContextConstructorFn choice_context_constructor{};
    ChoiceListBuilderFn choice_list_builder{};
    PopupChoiceConstructorFn popup_choice_constructor{};
    PopupChoiceListProviderFn popup_choice_list_provider{};
    PopupChoiceListTemplateFn popup_choice_list_template{};
    void** popup_choice_value_vtable{};
    void** popup_choice_selection_vtable{};
    void** popup_choice_presentation_vtable{};
    AddSliderFn add_slider{};
    AddButtonFn add_button{};
    DestroyTextReferencesFn destroy_text_references{};
    TextResolverFn text_resolver{};
    ScaleformPathResolverFn scaleform_path_resolver{};
    ScaleformTextSetterFn scaleform_text_setter{};
    ScaleformResultDestructorFn scaleform_result_destructor{};

    [[nodiscard]] bool title_bridge_complete() const noexcept {
        return scaleform_path_resolver && scaleform_text_setter &&
            scaleform_result_destructor;
    }

    [[nodiscard]] bool complete(
        bool require_rows,
        bool require_buttons,
        bool require_submenus,
        bool require_popup_choices,
        bool require_custom_text) const noexcept;
};

bool resolve_game_addresses(
    const ModuleView& game,
    GameAddresses& output,
    bool require_rows,
    bool require_buttons,
    bool require_submenus,
    bool require_native_back,
    bool require_popup_choices,
    bool require_custom_text) noexcept;

} // namespace erui::native
