#include "addresses.hpp"

#include "runtime_log.hpp"

#include <cinttypes>
#include <cstring>
#include <string_view>

namespace erui::native {
namespace {

constexpr std::uintptr_t kHubHandlerRva = 0x958C50;
constexpr std::uintptr_t kSubHandlerRva = 0x95A5D0;
constexpr std::uintptr_t kOpenSubPageRva = 0x94FA20;
constexpr std::uintptr_t kOnOffListRva = 0x955550;
constexpr std::uintptr_t kMenuContextRva = 0x869300;
constexpr std::uintptr_t kTextRefHelpRva = 0x760790;
constexpr std::uintptr_t kTextRefLabelRva = 0x760970;
constexpr std::uintptr_t kRootButtonTextReferencesRva = 0x958210;
constexpr std::uintptr_t kRootButtonDisplayTextRva = 0x763BA0;
constexpr std::uintptr_t kAddToggleRva = 0x948FA0;
constexpr std::uintptr_t kAddChoiceRva = 0x949120;
constexpr std::uintptr_t kAddSliderRva = 0x9493F0;
constexpr std::uintptr_t kAddButtonRva = 0x9298A0;
constexpr std::uintptr_t kDestroyTextReferencesRva = 0x742C90;
constexpr std::uintptr_t kTextResolverRva = 0x7633D0;
constexpr std::uintptr_t kNativeBackRva = 0x747CD0;
constexpr std::uintptr_t kPageFrameRva = 0x958FF0;
constexpr std::uintptr_t kScaleformPathResolverRva = 0x74B140;
constexpr std::uintptr_t kScaleformTextSetterRva = 0x74AE50;
constexpr std::uintptr_t kScaleformResultDestructorRva = 0xD81590;

constexpr std::string_view kHubHandlerPattern =
    "40 55 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 E0 F2 FF FF";
constexpr std::string_view kSubHandlerPattern =
    "40 55 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 20 F7 FF FF "
    "48 81 EC E0 09 00 00 48 C7 44 24 40 FE FF FF FF";
constexpr std::string_view kOpenSubPagePattern =
    "40 55 53 56 57 41 56 41 57 48 8D 6C 24 D8 48 81 EC 28 01 00 00 "
    "48 C7 45 E8 FE FF FF FF";
constexpr std::string_view kOnOffListPattern =
    "48 8B C4 55 41 56 41 57 48 8D 68 A1 48 81 EC D0 00 00 00 "
    "48 C7 45 A7 FE FF FF FF 48 89 58 10 48 89 70 18 48 89 78 20 "
    "48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 45 3F "
    "48 8B F9 48 89 4D AF 33 F6 89 75 9F "
    "48 8D 05 ?? ?? ?? ?? 48 89 01 "
    "48 8D 05 ?? ?? ?? ?? 48 89 01 "
    "48 8D 05 ?? ?? ?? ?? 48 89 01 "
    "48 89 B1 A0 00 00 00 "
    "48 8D 05 ?? ?? ?? ?? 48 89 01 "
    "C7 45 9F 01 00 00 00 BA 6C 9D 00 00";
constexpr std::string_view kMenuContextPattern =
    "40 53 48 81 EC F0 01 00 00 48 C7 44 24 20 FE FF FF FF";
constexpr std::string_view kTextRefHelpPattern =
    "4C 8B DC 57 48 81 EC 90 00 00 00 49 C7 43 A8 FE FF FF FF "
    "49 89 5B 18 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 84 24 88 00 00 00";
constexpr std::string_view kTextRefLabelCallsitePattern =
    "BA 54 70 04 00 48 8D 4C 24 30 E8 ?? ?? ?? ??";
constexpr std::size_t kTextRefLabelCallOffset = 10;
constexpr std::string_view kRootButtonTextReferencesPattern =
    "4C 8B DC 57 48 81 EC 80 00 00 00 49 C7 43 A8 FE FF FF FF "
    "49 89 5B 10 49 89 73 18";
constexpr std::string_view kRootButtonDisplayTextPattern =
    "48 89 4C 24 08 53 48 83 EC 40 48 C7 44 24 38 FE FF FF FF "
    "48 8B D9 C7 44 24 30 00 00 00 00 85 D2 7F 07";
constexpr std::string_view kAddToggleCallsitePattern =
    "8D 9C 00 00 00 48 89 4C 24 20 4C 8B CB 48 8B D0 48 8B CE E8 ?? ?? ?? ??";
constexpr std::size_t kAddToggleCallOffset = 19;
constexpr std::string_view kAddSliderCallsitePattern =
    "0F B6 45 F4 88 44 24 20 4C 8D 4C 24 4C 48 8B CB E8 ?? ?? ?? ??";
constexpr std::size_t kAddSliderCallOffset = 16;
// The Graphics subpage constructs many discrete-choice rows through one
// common target. This callsite is the second repeated row and is distinctive
// because its selected byte is base+1 and its menu context is stack+0x31.
constexpr std::string_view kAddChoiceCallsitePattern =
    "4D 8D 46 01 48 8D 4C 24 31 48 89 4C 24 20 "
    "4C 8B CB 48 8B D0 48 8B CE E8 ?? ?? ?? ??";
constexpr std::size_t kAddChoiceCallOffset = 23;
constexpr std::uintptr_t kChoiceContextConstructorRva = 0x86A410;
constexpr std::string_view kChoiceContextConstructorPrologue =
    "40 53 48 81 EC A0 00 00 00 48 C7 44 24 20 FE FF FF FF";
constexpr std::uintptr_t kChoiceListBuilderRva = 0x956CF0;
constexpr std::string_view kChoiceListBuilderPrologue =
    "48 8B C4 55 41 56 41 57 48 8D 68 A1 48 81 EC A0 00 00 00 "
    "48 C7 45 D7 FE FF FF FF";

constexpr std::uintptr_t kPopupChoiceConstructorRva = 0x95D010;
constexpr std::uintptr_t kPopupChoiceListProviderRva = 0x9623A0;
constexpr std::uintptr_t kPopupChoiceListTemplateRva = 0x86A610;
constexpr std::uintptr_t kPopupChoiceValueInvokeRva = 0x962520;
constexpr std::uintptr_t kPopupChoiceSelectionInvokeRva = 0x962330;
constexpr std::uintptr_t kPopupChoiceValueVtableRva = 0x2B15A78;
constexpr std::uintptr_t kPopupChoiceSelectionVtableRva = 0x2B15AB0;
constexpr std::uintptr_t kPopupChoicePresentationVtableRva = 0x2B15AE8;

constexpr std::string_view kPopupChoiceConstructorPattern =
    "40 55 53 56 57 41 54 41 55 41 56 41 57 "
    "48 8D AC 24 D8 FD FF FF 48 81 EC 28 03 00 00 "
    "48 C7 44 24 40 FE FF FF FF";
constexpr std::string_view kPopupChoiceListProviderPattern =
    "48 89 54 24 10 53 48 83 EC 30 "
    "48 C7 44 24 28 FE FF FF FF "
    "48 8B DA C7 44 24 20 00 00 00 00 "
    "4C 8D 41 08 0F B6 54 24 48 48 8B CB";
constexpr std::string_view kPopupChoiceListTemplatePattern =
    "48 8B C4 55 57 41 54 41 56 41 57 "
    "48 8D 68 A1 48 81 EC C0 00 00 00 "
    "48 C7 45 9F FE FF FF FF "
    "48 89 58 10 48 89 70 18";
constexpr std::string_view kPopupChoiceValueInvokePattern =
    "48 8B 41 08 0F B6 40 01 C3";
constexpr std::string_view kPopupChoiceSelectionInvokePattern =
    "4C 8B 41 08 0F B6 02 41 88 40 01 "
    "48 8B 49 10 48 8B 01 48 FF A0 A0 00 00 00";

constexpr std::string_view kAddTogglePrologue =
    "40 55 53 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 18 FF FF FF "
    "48 81 EC E8 01 00 00";
constexpr std::string_view kAddSliderPrologue =
    "4C 8B DC 53 55 56 57 41 56 41 57 48 81 EC 98 01 00 00";
constexpr std::string_view kAddChoicePrologue =
    "40 55 53 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 18 FF FF FF "
    "48 81 EC E8 01 00 00";
constexpr std::string_view kAddButtonPrologue =
    "40 53 55 56 57 48 81 EC 18 01 00 00";
constexpr std::string_view kDestroyTextReferencesPrologue =
    "48 89 4C 24 08 57 48 83 EC 30 48 C7 44 24 20 FE FF FF FF "
    "48 89 5C 24 50 48 89 74 24 58 48 8B F1 48 8D 41 38";
constexpr std::string_view kTextResolverPrologue =
    "48 89 4C 24 08 57 48 83 EC 40 48 C7 44 24 38 FE FF FF FF "
    "48 89 5C 24 58 48 89 6C 24 60 48 89 74 24 68 49 8B F1";
constexpr std::string_view kNativeBackPattern =
    "48 89 54 24 10 53 48 81 EC 80 00 00 00 48 C7 44 24 20 FE FF FF FF "
    "48 8B D9 80 B9 B0 03 00 00 00 0F 85 ?? ?? ?? ?? "
    "48 8D 8C 24 98 00 00 00 E8 ?? ?? ?? ??";
constexpr std::string_view kPageFramePattern =
    "48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 30 "
    "0F 29 74 24 20 49 8B F0 0F 28 F1 48 8B F9 "
    "E8 ?? ?? ?? ?? 48 8B C8 48 8D 97 38 1B 00 00 "
    "E8 ?? ?? ?? ?? 80 BF E2 1C 00 00 00 75 ?? "
    "80 BF E1 1C 00 00 00 75 ??";

// These interfaces were recovered from the current Game Options construction
// path. The resolver receives MenuTitle/Text_0 at the page constructor and
// writes a persistent result to page+0x230. Native one-shot call sites use the
// same resolver with a 0x60-byte stack result, call the UTF-16 setter with
// result+0x08, and then invoke the nested-member destructor on result+0x28.
// Current-build research anchors are the resolver call at RVA 0x74293B and
// temporary cleanup at RVA 0x74BEC5; neither anchor is executed directly.
constexpr std::string_view kScaleformPathResolverPattern =
    "4C 89 44 24 18 4C 89 4C 24 20 55 53 56 57 41 56 41 57 "
    "48 8D 6C 24 D1 48 81 EC C8 00 00 00 48 C7 45 8F FE FF FF FF";
constexpr std::string_view kScaleformTextSetterPattern =
    "40 53 48 83 EC 20 48 8B 09 48 8B DA 48 8B 01 FF 50 08 "
    "8B 48 20 81 E1 8F 00 00 00 83 F9 02 72 1B";
constexpr std::string_view kScaleformResultDestructorPattern =
    "48 89 4C 24 08 53 48 83 EC 30 48 C7 44 24 20 FE FF FF FF "
    "48 8D 05 ?? ?? ?? ?? 48 89 01 48 8D 59 08 8B 43 18 "
    "C1 E8 06 A8 01 74 1A";

std::uint8_t* resolve_direct(
    const ModuleView& game,
    const char* name,
    std::string_view scan_pattern,
    std::uintptr_t fallback_rva,
    std::string_view fallback_validation = {}) noexcept {
    const ScanResult scan = game.scan_executable(scan_pattern);
    if (scan.address && scan.matches == 1) {
        erui::detail::logf(
            erui::LogLevel::info,
            "Address %-22s = %p (AOB, RVA 0x%" PRIXPTR ")",
            name,
            static_cast<void*>(scan.address),
            reinterpret_cast<std::uintptr_t>(scan.address) -
                reinterpret_cast<std::uintptr_t>(game.base()));
        return scan.address;
    }

    if (scan.matches > 1) {
        erui::detail::logf(
            erui::LogLevel::info,
            "Address %-22s AOB was ambiguous (%zu matches)",
            name,
            scan.matches);
    } else {
        erui::detail::logf(
            erui::LogLevel::info,
            "Address %-22s AOB was not found",
            name);
    }

    auto* fallback = game.at_rva(fallback_rva);
    const bool valid = fallback && game.is_executable(fallback) &&
        (fallback_validation.empty() || game.matches(fallback, fallback_validation));
    if (valid) {
        erui::detail::logf(
            erui::LogLevel::info,
            "Address %-22s = %p (validated fallback RVA 0x%" PRIXPTR ")",
            name,
            static_cast<void*>(fallback),
            fallback_rva);
        return fallback;
    }

    erui::detail::logf(
        erui::LogLevel::error,
        "Address %-22s fallback RVA 0x%" PRIXPTR " failed validation",
        name,
        fallback_rva);
    return nullptr;
}

std::uint8_t* resolve_call_target(
    const ModuleView& game,
    const char* name,
    std::string_view callsite_pattern,
    std::size_t call_offset,
    std::uintptr_t fallback_rva,
    std::string_view fallback_validation = {}) noexcept {
    const ScanResult scan = game.scan_executable(callsite_pattern);
    if (scan.address && scan.matches == 1) {
        if (auto* target = resolve_rel32_call(game, scan.address + call_offset)) {
            erui::detail::logf(
                erui::LogLevel::info,
                "Address %-22s = %p (AOB call target, RVA 0x%" PRIXPTR ")",
                name,
                static_cast<void*>(target),
                reinterpret_cast<std::uintptr_t>(target) -
                    reinterpret_cast<std::uintptr_t>(game.base()));
            return target;
        }
        erui::detail::logf(
            erui::LogLevel::info,
            "Address %-22s callsite matched but rel32 resolution failed",
            name);
    } else if (scan.matches > 1) {
        erui::detail::logf(
            erui::LogLevel::info,
            "Address %-22s callsite AOB was ambiguous (%zu matches)",
            name,
            scan.matches);
    } else {
        erui::detail::logf(
            erui::LogLevel::info,
            "Address %-22s callsite AOB was not found",
            name);
    }

    if (fallback_rva == 0) {
        erui::detail::logf(
            erui::LogLevel::error,
            "Address %-22s has no validated fallback",
            name);
        return nullptr;
    }

    auto* fallback = game.at_rva(fallback_rva);
    const bool valid = !fallback_validation.empty() && fallback &&
        game.is_executable(fallback) && game.matches(fallback, fallback_validation);
    if (valid) {
        erui::detail::logf(
            erui::LogLevel::info,
            "Address %-22s = %p (validated fallback RVA 0x%" PRIXPTR ")",
            name,
            static_cast<void*>(fallback),
            fallback_rva);
        return fallback;
    }

    erui::detail::logf(
        erui::LogLevel::error,
        "Address %-22s fallback RVA 0x%" PRIXPTR " failed validation",
        name,
        fallback_rva);
    return nullptr;
}

std::uint8_t* resolve_optional_direct(
    const ModuleView& game,
    const char* name,
    std::string_view scan_pattern,
    std::uintptr_t fallback_rva,
    std::string_view fallback_validation) noexcept {
    const ScanResult scan = game.scan_executable(scan_pattern);
    if (scan.address && scan.matches == 1) {
        erui::detail::logf(
            erui::LogLevel::info,
            "Address %-22s = %p (AOB, RVA 0x%" PRIXPTR ")",
            name,
            static_cast<void*>(scan.address),
            reinterpret_cast<std::uintptr_t>(scan.address) -
                reinterpret_cast<std::uintptr_t>(game.base()));
        return scan.address;
    }

    auto* fallback = game.at_rva(fallback_rva);
    const bool valid = !fallback_validation.empty() && fallback &&
        game.is_executable(fallback) && game.matches(fallback, fallback_validation);
    if (valid) {
        erui::detail::logf(
            erui::LogLevel::info,
            "Address %-22s = %p (validated fallback RVA 0x%" PRIXPTR ")",
            name,
            static_cast<void*>(fallback),
            fallback_rva);
        return fallback;
    }

    erui::detail::logf(
        erui::LogLevel::warning,
        "Optional address %-13s unavailable (AOB matches=%zu, fallback RVA 0x%" PRIXPTR " invalid)",
        name,
        scan.matches,
        fallback_rva);
    return nullptr;
}

std::uint8_t* resolve_optional_call_target(
    const ModuleView& game,
    const char* name,
    std::string_view callsite_pattern,
    std::size_t call_offset) noexcept {
    const ScanResult scan = game.scan_executable(callsite_pattern);
    if (scan.address && scan.matches == 1) {
        if (auto* target = resolve_rel32_call(
                game, scan.address + call_offset)) {
            erui::detail::logf(
                erui::LogLevel::info,
                "Optional address %-13s = %p (AOB call target, RVA 0x%" PRIXPTR ")",
                name,
                static_cast<void*>(target),
                reinterpret_cast<std::uintptr_t>(target) -
                    reinterpret_cast<std::uintptr_t>(game.base()));
            return target;
        }
    }
    erui::detail::logf(
        erui::LogLevel::warning,
        "Optional address %-13s unavailable (callsite AOB matches=%zu)",
        name,
        scan.matches);
    return nullptr;
}

std::uint8_t* resolve_text_resolver(const ModuleView& game) noexcept {
    return resolve_direct(
        game,
        "native text resolver",
        kTextResolverPrologue,
        kTextResolverRva,
        kTextResolverPrologue);
}

bool valid_callable_vtable(
    const ModuleView& game,
    void** vtable,
    const void* expected_invoke) noexcept {
    if (!vtable || !expected_invoke ||
        !game.contains(vtable, 3 * sizeof(void*))) {
        return false;
    }
    void* entries[3]{};
    std::memcpy(entries, vtable, sizeof(entries));
    return entries[2] == expected_invoke &&
        game.is_executable(entries[0]) &&
        game.is_executable(entries[1]) &&
        game.is_executable(entries[2]);
}

void** resolve_callable_vtable(
    const ModuleView& game,
    const char* name,
    const void* invoke,
    std::uintptr_t fallback_rva) noexcept {
    void** match = nullptr;
    std::size_t matches = 0;
    const std::uintptr_t expected = reinterpret_cast<std::uintptr_t>(invoke);
    for (const ModuleSection& section : game.sections()) {
        if (section.executable || section.size < 3 * sizeof(void*)) continue;
        for (std::size_t offset = 2 * sizeof(void*);
             offset + sizeof(void*) <= section.size;
             offset += sizeof(void*)) {
            std::uintptr_t candidate_invoke{};
            std::memcpy(
                &candidate_invoke, section.begin + offset,
                sizeof(candidate_invoke));
            if (candidate_invoke != expected) continue;
            auto** candidate = reinterpret_cast<void**>(
                section.begin + offset - 2 * sizeof(void*));
            if (!valid_callable_vtable(game, candidate, invoke)) continue;
            if (!match) match = candidate;
            ++matches;
        }
    }

    if (matches == 1) {
        erui::detail::logf(
            erui::LogLevel::info,
            "Address %-22s = %p (callable vtable scan, RVA 0x%" PRIXPTR ")",
            name,
            static_cast<void*>(match),
            reinterpret_cast<std::uintptr_t>(match) -
                reinterpret_cast<std::uintptr_t>(game.base()));
        return match;
    }

    auto** fallback = reinterpret_cast<void**>(game.at_rva(fallback_rva));
    if (valid_callable_vtable(game, fallback, invoke)) {
        erui::detail::logf(
            erui::LogLevel::info,
            "Address %-22s = %p (validated fallback RVA 0x%" PRIXPTR ")",
            name,
            static_cast<void*>(fallback),
            fallback_rva);
        return fallback;
    }

    erui::detail::logf(
        erui::LogLevel::error,
        "Address %-22s callable scan had %zu matches and fallback RVA 0x%" PRIXPTR " failed validation",
        name,
        matches,
        fallback_rva);
    return nullptr;
}

} // namespace

bool GameAddresses::complete(
    bool require_rows,
    bool require_buttons,
    bool require_submenus,
    bool require_popup_choices,
    bool require_custom_text) const noexcept {
    const bool rows_ready = !require_rows || (
        hub_handler && on_off_list && menu_context && text_ref_help &&
        text_ref_label && add_toggle && add_slider && add_inline_choice &&
        choice_context_constructor && choice_list_builder &&
        destroy_text_references);
    const bool buttons_ready = !require_buttons ||
        (add_button && root_button_text_references &&
            root_button_display_text && destroy_text_references);
    const bool submenus_ready = !require_submenus ||
        (sub_handler && open_sub_page && add_button && destroy_text_references);
    const bool popup_choices_ready = !require_popup_choices ||
        (popup_choice_constructor && popup_choice_list_provider &&
            popup_choice_list_template && popup_choice_value_vtable &&
            popup_choice_selection_vtable &&
            popup_choice_presentation_vtable);
    const bool text_ready = !require_custom_text || text_resolver;
    return rows_ready && buttons_ready && submenus_ready &&
        popup_choices_ready && text_ready;
}

bool resolve_game_addresses(
    const ModuleView& game,
    GameAddresses& output,
    bool require_rows,
    bool require_buttons,
    bool require_submenus,
    bool require_native_back,
    bool require_popup_choices,
    bool require_custom_text) noexcept {
    output = {};
    output.game_image_base = game.base();
    output.game_image_size = game.image_size();

    if (!require_rows) {
        erui::detail::logf(
            erui::LogLevel::error,
            "Row injection is disabled; there is nothing to resolve.");
        return false;
    }

    if (require_rows) {
        output.hub_handler = reinterpret_cast<HubHandlerFn>(resolve_direct(
            game,
            "hub handler",
            kHubHandlerPattern,
            kHubHandlerRva,
            kHubHandlerPattern));
        if (require_submenus) {
            output.sub_handler = reinterpret_cast<SubHandlerFn>(resolve_direct(
                game,
                "subpage handler",
                kSubHandlerPattern,
                kSubHandlerRva,
                kSubHandlerPattern));
            output.open_sub_page = reinterpret_cast<OpenSubPageFn>(resolve_direct(
                game,
                "open subpage",
                kOpenSubPagePattern,
                kOpenSubPageRva,
                kOpenSubPagePattern));

            // Presentation is deliberately optional. A game update may disable
            // custom titles without preventing page construction or row input.
            output.scaleform_path_resolver =
                reinterpret_cast<ScaleformPathResolverFn>(resolve_optional_direct(
                    game,
                    "Scaleform path",
                    kScaleformPathResolverPattern,
                    kScaleformPathResolverRva,
                    kScaleformPathResolverPattern));
            output.scaleform_text_setter =
                reinterpret_cast<ScaleformTextSetterFn>(resolve_optional_direct(
                    game,
                    "Scaleform text setter",
                    kScaleformTextSetterPattern,
                    kScaleformTextSetterRva,
                    kScaleformTextSetterPattern));
            output.scaleform_result_destructor =
                reinterpret_cast<ScaleformResultDestructorFn>(resolve_optional_direct(
                    game,
                    "Scaleform result dtor",
                    kScaleformResultDestructorPattern,
                    kScaleformResultDestructorRva,
                    kScaleformResultDestructorPattern));
        } else {
            erui::detail::logf(
                erui::LogLevel::info,
                "Address resolution: no native submenu rows declared");
        }
        output.on_off_list = reinterpret_cast<BufferConstructorFn>(resolve_direct(
            game,
            "on/off list",
            kOnOffListPattern,
            kOnOffListRva,
            kOnOffListPattern));
        output.menu_context = reinterpret_cast<BufferConstructorFn>(resolve_direct(
            game,
            "menu context",
            kMenuContextPattern,
            kMenuContextRva,
            kMenuContextPattern));
        output.text_ref_help = reinterpret_cast<TextReferenceFn>(resolve_direct(
            game,
            "text ref help",
            kTextRefHelpPattern,
            kTextRefHelpRva,
            "4C 8B DC 57 48 81 EC 90 00 00 00"));
        output.text_ref_label = reinterpret_cast<TextReferenceFn>(resolve_call_target(
            game,
            "text ref label",
            kTextRefLabelCallsitePattern,
            kTextRefLabelCallOffset,
            kTextRefLabelRva));
        if (require_buttons) {
            output.root_button_text_references =
                reinterpret_cast<BufferConstructorFn>(resolve_direct(
                    game,
                    "root button text refs",
                    kRootButtonTextReferencesPattern,
                    kRootButtonTextReferencesRva,
                    kRootButtonTextReferencesPattern));
            output.root_button_display_text =
                reinterpret_cast<TextReferenceFn>(resolve_direct(
                    game,
                    "root button display",
                    kRootButtonDisplayTextPattern,
                    kRootButtonDisplayTextRva,
                    kRootButtonDisplayTextPattern));
        }
        output.add_toggle = reinterpret_cast<AddToggleFn>(resolve_call_target(
            game,
            "add toggle",
            kAddToggleCallsitePattern,
            kAddToggleCallOffset,
            kAddToggleRva,
            kAddTogglePrologue));
        output.add_inline_choice =
            reinterpret_cast<AddInlineChoiceFn>(resolve_call_target(
            game, "add inline choice", kAddChoiceCallsitePattern,
            kAddChoiceCallOffset, kAddChoiceRva, kAddChoicePrologue));
        output.choice_context_constructor =
            reinterpret_cast<ChoiceContextConstructorFn>(resolve_optional_direct(
                game, "choice context", kChoiceContextConstructorPrologue,
                kChoiceContextConstructorRva,
                kChoiceContextConstructorPrologue));
        output.choice_list_builder = reinterpret_cast<ChoiceListBuilderFn>(
            resolve_optional_direct(
                game, "choice list builder", kChoiceListBuilderPrologue,
                kChoiceListBuilderRva, kChoiceListBuilderPrologue));
        output.add_slider = reinterpret_cast<AddSliderFn>(resolve_call_target(
            game,
            "add slider",
            kAddSliderCallsitePattern,
            kAddSliderCallOffset,
            kAddSliderRva,
            kAddSliderPrologue));

        // Inline and popup selectors both clone their text references during
        // construction, after which the temporary source object is destroyed.
        output.destroy_text_references =
            reinterpret_cast<DestroyTextReferencesFn>(resolve_direct(
                game,
                "destroy text refs",
                kDestroyTextReferencesPrologue,
                kDestroyTextReferencesRva,
                kDestroyTextReferencesPrologue));

        if (require_buttons) {
            output.add_button = reinterpret_cast<AddButtonFn>(resolve_direct(
                game,
                "add button",
                kAddButtonPrologue,
                kAddButtonRva,
                kAddButtonPrologue));
        } else {
            erui::detail::logf(
                erui::LogLevel::info,
                "Address resolution: no native button rows declared");
        }

        if (require_popup_choices) {
            output.popup_choice_constructor =
                reinterpret_cast<PopupChoiceConstructorFn>(resolve_direct(
                    game,
                    "popup choice ctor",
                    kPopupChoiceConstructorPattern,
                    kPopupChoiceConstructorRva,
                    kPopupChoiceConstructorPattern));
            output.popup_choice_list_provider =
                reinterpret_cast<PopupChoiceListProviderFn>(resolve_direct(
                    game,
                    "popup list provider",
                    kPopupChoiceListProviderPattern,
                    kPopupChoiceListProviderRva,
                    kPopupChoiceListProviderPattern));
            output.popup_choice_list_template =
                reinterpret_cast<PopupChoiceListTemplateFn>(resolve_direct(
                    game,
                    "popup list template",
                    kPopupChoiceListTemplatePattern,
                    kPopupChoiceListTemplateRva,
                    kPopupChoiceListTemplatePattern));
            auto* value_invoke = resolve_direct(
                game,
                "popup value invoke",
                kPopupChoiceValueInvokePattern,
                kPopupChoiceValueInvokeRva,
                kPopupChoiceValueInvokePattern);
            auto* selection_invoke = resolve_direct(
                game,
                "popup commit invoke",
                kPopupChoiceSelectionInvokePattern,
                kPopupChoiceSelectionInvokeRva,
                kPopupChoiceSelectionInvokePattern);
            if (value_invoke) {
                output.popup_choice_value_vtable = resolve_callable_vtable(
                    game,
                    "popup value vtable",
                    value_invoke,
                    kPopupChoiceValueVtableRva);
            }
            if (selection_invoke) {
                output.popup_choice_selection_vtable =
                    resolve_callable_vtable(
                        game,
                        "popup commit vtable",
                        selection_invoke,
                        kPopupChoiceSelectionVtableRva);
            }
            if (output.popup_choice_list_provider) {
                output.popup_choice_presentation_vtable =
                    resolve_callable_vtable(
                        game,
                        "popup list vtable",
                        reinterpret_cast<const void*>(
                            output.popup_choice_list_provider),
                        kPopupChoicePresentationVtableRva);
            }
        } else {
            erui::detail::logf(
                erui::LogLevel::info,
                "Address resolution: no popup-choice rows declared");
        }
    } else {
        erui::detail::logf(
            erui::LogLevel::info,
            "Address resolution: native row path disabled");
    }

    if (require_custom_text) {
        output.text_resolver = reinterpret_cast<TextResolverFn>(
            resolve_text_resolver(game));
    } else {
        erui::detail::logf(
            erui::LogLevel::info,
            "Address resolution: custom text hook disabled");
    }

    if (require_native_back) {
        output.native_back = reinterpret_cast<NativeBackFn>(resolve_direct(
            game, "native Back", kNativeBackPattern,
            kNativeBackRva, kNativeBackPattern));
        if (!output.native_back) {
            return false;
        }
    } else if (require_custom_text) {
        // Alerts are optional, but a modal must be able to suppress the real
        // Back command. Missing alert-only interfaces do not disable menus.
        output.native_back = reinterpret_cast<NativeBackFn>(
            resolve_optional_direct(
                game, "native Back", kNativeBackPattern,
                kNativeBackRva, kNativeBackPattern));
    }

    if (require_custom_text) {
        output.page_frame = reinterpret_cast<PageFrameFn>(
            resolve_optional_direct(
                game, "page frame", kPageFramePattern,
                kPageFrameRva, kPageFramePattern));
    }

    if (!output.complete(
            require_rows,
            require_buttons,
            require_submenus,
            require_popup_choices,
            require_custom_text)) {
        erui::detail::logf(
            erui::LogLevel::error,
            "One or more native interfaces required by the enabled paths were unresolved; no hooks installed.");
        return false;
    }

    return true;
}

} // namespace erui::native
