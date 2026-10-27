#include "addresses.hpp"

#include "game_build_profiles.hpp"
#include "runtime_log.hpp"

#include <cinttypes>
#include <cstring>
#include <limits>
#include <string_view>

namespace erui::native {
namespace {

constexpr std::string_view kGameOptionsHandlerPattern =
    "40 55 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 E0 F2 FF FF";
// Category 1's CameraSetting row materializer. The wrapper at 0x93CB70
// stores this target in the same two-argument callback slot used by the other
// ordinary OptionSetting panels.
constexpr std::string_view kCameraPanelMaterializerPattern =
    "40 55 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 B0 FD FF FF "
    "48 81 EC 50 03 00 00 48 C7 45 00 FE FF FF FF "
    "48 89 9C 24 A0 03 00 00";
constexpr std::string_view kDisplayPanelMaterializerPattern =
    "40 55 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 A0 F2 FF FF "
    "48 81 EC 60 0E 00 00 48 C7 45 18 FE FF FF FF "
    "48 89 9C 24 B0 0E 00 00";
constexpr std::string_view kSoundPanelMaterializerPattern =
    "40 55 56 57 48 8D AC 24 50 FE FF FF 48 81 EC B0 02 00 00 "
    "48 C7 44 24 70 FE FF FF FF 48 89 9C 24 E0 02 00 00 "
    "48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 85 A0 01 00 00 "
    "48 8B FA 48 8B D9 48 8D 4D F0 E8 ?? ?? ?? ?? 90 48 8D 4D 80";
constexpr std::string_view kNetworkPanelMaterializerPattern =
    "40 55 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 D0 F2 FF FF "
    "48 81 EC 30 0E 00 00 48 C7 44 24 58 FE FF FF FF "
    "48 89 9C 24 80 0E 00 00";
constexpr std::string_view kKeyboardMousePanelMaterializerPattern =
    "48 8B C4 55 41 54 41 55 41 56 41 57 48 8D A8 68 FE FF FF "
    "48 81 EC 70 02 00 00 48 C7 45 20 FE FF FF FF "
    "48 89 58 10 48 89 70 18 48 89 78 20";
constexpr std::string_view kGraphicsPanelMaterializerPattern =
    "40 55 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 50 F5 FF FF "
    "48 81 EC B0 0B 00 00 48 C7 45 80 FE FF FF FF "
    "48 89 9C 24 00 0C 00 00";
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
constexpr std::string_view kChoiceContextConstructorPrologue =
    "40 53 48 81 EC A0 00 00 00 48 C7 44 24 20 FE FF FF FF";
constexpr std::string_view kChoiceListBuilderPrologue =
    "48 8B C4 55 41 56 41 57 48 8D 68 A1 48 81 EC A0 00 00 00 "
    "48 C7 45 D7 FE FF FF FF";

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
// CS::SliderCtrl virtual slot 13. This refresh is independent from the row
// constructor and owns the native Text_0/Text_1 value presentation.
constexpr std::string_view kSliderRefreshPattern =
    "40 57 48 81 EC 90 00 00 00 48 C7 44 24 20 FE FF FF FF "
    "48 89 9C 24 A0 00 00 00 48 8B F9 48 8D 15 ?? ?? ?? ??";
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

constexpr std::array<std::uint8_t, 16> kSelectedHelpPresenterPrefix{
    0x48, 0x8B, 0xC4, 0x57, 0x48, 0x83, 0xEC, 0x70,
    0x48, 0xC7, 0x40, 0xA8, 0xFE, 0xFF, 0xFF, 0xFF,
};
constexpr std::array<std::uint8_t, 8> kSelectedHelpClonePrefix{
    0x48, 0x8D, 0x54, 0x24, 0x30, 0x48, 0x8B, 0xC8,
};
constexpr std::array<std::uint8_t, 14> kSelectedHelpCloneSuffix{
    0x90, 0x4C, 0x8B, 0x07, 0x48, 0x8B, 0xD0,
    0x48, 0x8B, 0xCF, 0x41, 0xFF, 0x50, 0x40,
};
constexpr std::array<std::uint8_t, 54> kSelectedHelpCleanup{
    0x90, 0x48, 0x8D, 0x44, 0x24, 0x38, 0x48, 0x89, 0x44, 0x24, 0x28,
    0x48, 0x83, 0x7C, 0x24, 0x58, 0x08, 0x72, 0x10, 0x48, 0x8B, 0x4C,
    0x24, 0x38, 0x48, 0x8B, 0x01, 0x48, 0x8B, 0x54, 0x24, 0x40, 0xFF,
    0x50, 0x68, 0x48, 0xC7, 0x44, 0x24, 0x58, 0x07, 0x00, 0x00, 0x00,
    0x48, 0x89, 0x74, 0x24, 0x50, 0x66, 0x89, 0x74, 0x24, 0x40,
};
constexpr std::array<std::uint8_t, 34> kHelpCloneWrapperPrefix{
    0x48, 0x89, 0x54, 0x24, 0x10, 0x53, 0x48, 0x83, 0xEC, 0x30, 0x48,
    0xC7, 0x44, 0x24, 0x28, 0xFE, 0xFF, 0xFF, 0xFF, 0x48, 0x8B, 0xDA,
    0xC7, 0x44, 0x24, 0x20, 0x00, 0x00, 0x00, 0x00, 0x48, 0x83, 0xC1,
    0x08,
};
constexpr std::array<std::uint8_t, 45> kHelpInnerClonePrefix{
    0x48, 0x89, 0x54, 0x24, 0x10, 0x53, 0x48, 0x83, 0xEC, 0x30, 0x48,
    0xC7, 0x44, 0x24, 0x28, 0xFE, 0xFF, 0xFF, 0xFF, 0x48, 0x8B, 0xDA,
    0xC7, 0x44, 0x24, 0x20, 0x00, 0x00, 0x00, 0x00, 0x48, 0x8B, 0x41,
    0x38, 0x48, 0x89, 0x02, 0x48, 0x8D, 0x51, 0x40, 0x48, 0x8D, 0x4B,
    0x08,
};
constexpr std::array<std::uint8_t, 18> kHelpCloneSuffix{
    0x90, 0xC7, 0x44, 0x24, 0x20, 0x01, 0x00, 0x00, 0x00,
    0x48, 0x8B, 0xC3, 0x48, 0x83, 0xC4, 0x30, 0x5B, 0xC3,
};
constexpr std::array<std::uint8_t, 32> kSelectedHelpThunkPrefix{
    0x48, 0x8B, 0xC2, 0x48, 0x8B, 0x12, 0x48, 0x85, 0xD2, 0x75, 0x0E,
    0x48, 0x83, 0x78, 0x28, 0x08, 0x48, 0x8D, 0x50, 0x10, 0x72, 0x03,
    0x48, 0x8B, 0x12, 0x48, 0x81, 0xC1, 0xF8, 0x02, 0x00, 0x00,
};
// The native row destructor owns the strings rooted at +0x48 and +0x08 but
// never the direct help pointer at +0x40. Exact equality is what permits the
// runtime to publish a stable host-owned pointer in that direct slot.
constexpr std::array<std::uint8_t, 0xBD> kTwoOwnedStringCleanupBody{
    0x48, 0x89, 0x4C, 0x24, 0x08, 0x57, 0x48, 0x83, 0xEC, 0x30, 0x48,
    0xC7, 0x44, 0x24, 0x20, 0xFE, 0xFF, 0xFF, 0xFF, 0x48, 0x89, 0x5C,
    0x24, 0x50, 0x48, 0x89, 0x74, 0x24, 0x58, 0x48, 0x8B, 0xF1, 0x48,
    0x8D, 0x41, 0x48, 0x48, 0x89, 0x44, 0x24, 0x48, 0x48, 0x8D, 0x59,
    0x50, 0x48, 0x89, 0x5C, 0x24, 0x48, 0x48, 0x83, 0x7B, 0x20, 0x08,
    0x72, 0x0E, 0x48, 0x8B, 0x0B, 0x48, 0x8B, 0x01, 0x48, 0x8B, 0x53,
    0x08, 0xFF, 0x50, 0x68, 0x90, 0x48, 0xC7, 0x43, 0x20, 0x07, 0x00,
    0x00, 0x00, 0x48, 0x83, 0xC3, 0x08, 0x33, 0xFF, 0x48, 0x89, 0x7B,
    0x10, 0x48, 0x83, 0x7B, 0x18, 0x08, 0x72, 0x03, 0x48, 0x8B, 0x1B,
    0x66, 0x89, 0x3B, 0x48, 0x8D, 0x46, 0x08, 0x48, 0x89, 0x44, 0x24,
    0x40, 0x48, 0x8D, 0x5E, 0x10, 0x48, 0x89, 0x5C, 0x24, 0x40, 0x48,
    0x83, 0x7B, 0x20, 0x08, 0x72, 0x0E, 0x48, 0x8B, 0x0B, 0x48, 0x8B,
    0x01, 0x48, 0x8B, 0x53, 0x08, 0xFF, 0x50, 0x68, 0x90, 0x48, 0xC7,
    0x43, 0x20, 0x07, 0x00, 0x00, 0x00, 0x48, 0x89, 0x7B, 0x18, 0x48,
    0x83, 0x7B, 0x20, 0x08, 0x72, 0x09, 0x48, 0x8B, 0x43, 0x08, 0x66,
    0x89, 0x38, 0xEB, 0x04, 0x66, 0x89, 0x7B, 0x08, 0x48, 0x8B, 0x5C,
    0x24, 0x50, 0x48, 0x8B, 0x74, 0x24, 0x58, 0x48, 0x83, 0xC4, 0x30,
    0x5F, 0xC3,
};

constexpr std::array<unsigned char, 16> kGameOptionsEntryPrefix{
    0x40, 0x55, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55,
    0x41, 0x56, 0x41, 0x57, 0x48, 0x8D, 0xAC, 0x24,
};
constexpr std::array<unsigned char, 16> kSubHandlerEntryPrefix{
    0x40, 0x55, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55,
    0x41, 0x56, 0x41, 0x57, 0x48, 0x8D, 0xAC, 0x24,
};
constexpr std::array<unsigned char, 16> kTextResolverEntryPrefix{
    0x48, 0x89, 0x4C, 0x24, 0x08, 0x57, 0x48, 0x83,
    0xEC, 0x40, 0x48, 0xC7, 0x44, 0x24, 0x38, 0xFE,
};
constexpr std::array<unsigned char, 16> kSliderRefreshEntryPrefix{
    0x40, 0x57, 0x48, 0x81, 0xEC, 0x90, 0x00, 0x00,
    0x00, 0x48, 0xC7, 0x44, 0x24, 0x20, 0xFE, 0xFF,
};
constexpr std::array<unsigned char, 16> kSelectorSelectEntryPrefix{
    0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x6C,
    0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x18, 0x48,
};

// These interfaces were recovered from the current Game Options construction
// path. The resolver receives MenuTitle/Text_0 at the page constructor and
// writes a persistent result to page+0x230. Native one-shot call sites use the
// same resolver with a 0x60-byte stack result, call the UTF-16 setter with
// proxy self-link at result+0x08, and then destroy the embedded
// CSScaleformValue wrapper at result+0x28.
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

// TextInput retains owner-relative state and callbacks, so a locally matching
// byte sequence is accepted only after a recognized PE identity selects an
// explicitly verified build profile.
// Every recognized build keeps an explicit record, even where individual
// RVAs coincide, so later divergence remains visible and unit-testable.

constexpr std::string_view kTextInputRowProducerPattern =
    "40 55 53 56 57 41 54 41 55 41 56 41 57 "
    "48 8D AC 24 08 FD FF FF 48 81 EC F8 03 00 00 "
    "48 C7 44 24 78 FE FF FF FF";
constexpr std::string_view kNativeMenuStringConstructorPattern =
    "48 89 4C 24 08 53 48 83 EC 30 "
    "48 C7 44 24 28 FE FF FF FF 48 8B D9 "
    "C7 44 24 20 00 00 00 00 E8 ?? ?? ?? ??";
constexpr std::string_view kNativeMenuStringBorrowedConstructorPattern =
    "48 89 4C 24 08 57 48 83 EC 30 "
    "48 C7 44 24 28 FE FF FF FF "
    "48 89 5C 24 50 48 89 74 24 58 48 8B F9 33 F6 "
    "89 74 24 20 48 89 11 48 8D 59 08";
constexpr std::string_view kNativeMenuStringDestructorPattern =
    "48 89 4C 24 08 53 48 83 EC 30 "
    "48 C7 44 24 20 FE FF FF FF 48 8D 59 08 "
    "48 89 5C 24 40 48 83 7B 20 08";
constexpr std::string_view kTextInputEditorFactoryBuilderPattern =
    "48 89 4C 24 08 48 83 EC 18 "
    "48 C7 04 24 FE FF FF FF 48 C7 41 38 00 00 00 00 "
    "48 85 D2 74 ??";
constexpr std::string_view kTextInputEditorFactoryPattern =
    "4C 8B DC 4D 89 4B 20 49 89 4B 08 55 56 57 "
    "48 81 EC 90 00 00 00 49 C7 43 90 FE FF FF FF";

// Bytes of kNativeBackPattern that an inline hook may overwrite: the whole
// prologue up to the `mov rbx, rcx` (5 + 1 + 7 + 9). A 5-byte rel32 jump and
// a 14-byte absolute jump both end inside it, so the rest stays untouched.
constexpr std::size_t kNativeBackHookablePrologue = 22;

[[nodiscard]] std::string_view pattern_after_bytes(
    std::string_view pattern,
    std::size_t bytes) noexcept {
    std::size_t cursor = 0;
    for (std::size_t skipped = 0; skipped < bytes; ++skipped) {
        while (cursor < pattern.size() && pattern[cursor] == ' ') ++cursor;
        while (cursor < pattern.size() && pattern[cursor] != ' ') ++cursor;
    }
    while (cursor < pattern.size() && pattern[cursor] == ' ') ++cursor;
    return pattern.substr(cursor);
}

// Another mod may already hold an inline hook on the function: its jump
// replaces the first instructions while the body stays as the game shipped
// it. At the RVA of a verified build profile, a recognised hook jump followed
// by the untouched rest of the pattern is still the same function, and both
// calling it and hooking it again chain through the other mod's hook.
[[nodiscard]] bool matches_behind_hook_jump(
    const ModuleView& game,
    const std::uint8_t* address,
    std::string_view pattern,
    std::size_t hookable_prologue) noexcept {
    if (!address || pattern.empty() || hookable_prologue == 0) return false;
    const bool hook_jump = game.matches(address, "E9") ||
        game.matches(address, "FF 25") ||
        game.matches(address, "48 B8 ?? ?? ?? ?? ?? ?? ?? ?? FF E0");
    if (!hook_jump) return false;
    const std::string_view tail = pattern_after_bytes(pattern, hookable_prologue);
    return !tail.empty() && game.matches(address + hookable_prologue, tail);
}

void log_hooked_fallback(
    const char* name,
    const std::uint8_t* fallback,
    std::uintptr_t fallback_rva) noexcept {
    erui::detail::logf(
        erui::LogLevel::info,
        "Address %-22s = %p (fallback RVA 0x%" PRIXPTR "; entry already hooked by another module, body validated)",
        name,
        static_cast<const void*>(fallback),
        fallback_rva);
}

std::uint8_t* resolve_direct(
    const ModuleView& game,
    const char* name,
    std::string_view scan_pattern,
    std::uintptr_t fallback_rva,
    std::string_view fallback_validation = {},
    std::size_t hookable_prologue = 0) noexcept {
    const ScanResult scan = game.scan_executable(scan_pattern);
    if (scan.address && scan.matches == 1) {
        erui::detail::logf(
            erui::LogLevel::verbose,
            "Address %-22s = %p (AOB, RVA 0x%" PRIXPTR ")",
            name,
            static_cast<void*>(scan.address),
            reinterpret_cast<std::uintptr_t>(scan.address) -
                reinterpret_cast<std::uintptr_t>(game.base()));
        return scan.address;
    }

    if (scan.matches > 1) {
        erui::detail::logf(
            erui::LogLevel::verbose,
            "Address %-22s AOB was ambiguous (%zu matches)",
            name,
            scan.matches);
    } else {
        erui::detail::logf(
            erui::LogLevel::verbose,
            "Address %-22s AOB was not found",
            name);
    }

    auto* fallback = game.at_rva(fallback_rva);
    const bool valid = fallback && game.is_executable(fallback) &&
        (fallback_validation.empty() || game.matches(fallback, fallback_validation));
    if (valid) {
        erui::detail::logf(
            erui::LogLevel::verbose,
            "Address %-22s = %p (validated fallback RVA 0x%" PRIXPTR ")",
            name,
            static_cast<void*>(fallback),
            fallback_rva);
        return fallback;
    }
    if (fallback && game.is_executable(fallback) &&
        matches_behind_hook_jump(
            game, fallback, fallback_validation, hookable_prologue)) {
        log_hooked_fallback(name, fallback, fallback_rva);
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
                erui::LogLevel::verbose,
                "Address %-22s = %p (AOB call target, RVA 0x%" PRIXPTR ")",
                name,
                static_cast<void*>(target),
                reinterpret_cast<std::uintptr_t>(target) -
                    reinterpret_cast<std::uintptr_t>(game.base()));
            return target;
        }
        erui::detail::logf(
            erui::LogLevel::verbose,
            "Address %-22s callsite matched but rel32 resolution failed",
            name);
    } else if (scan.matches > 1) {
        erui::detail::logf(
            erui::LogLevel::verbose,
            "Address %-22s callsite AOB was ambiguous (%zu matches)",
            name,
            scan.matches);
    } else {
        erui::detail::logf(
            erui::LogLevel::verbose,
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
            erui::LogLevel::verbose,
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
    std::string_view fallback_validation,
    std::size_t hookable_prologue = 0) noexcept {
    const ScanResult scan = game.scan_executable(scan_pattern);
    if (scan.address && scan.matches == 1) {
        erui::detail::logf(
            erui::LogLevel::verbose,
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
            erui::LogLevel::verbose,
            "Address %-22s = %p (validated fallback RVA 0x%" PRIXPTR ")",
            name,
            static_cast<void*>(fallback),
            fallback_rva);
        return fallback;
    }
    if (fallback && game.is_executable(fallback) &&
        matches_behind_hook_jump(
            game, fallback, fallback_validation, hookable_prologue)) {
        log_hooked_fallback(name, fallback, fallback_rva);
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
                erui::LogLevel::verbose,
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

std::uint8_t* resolve_text_resolver(
    const ModuleView& game,
    std::uintptr_t fallback_rva) noexcept {
    return resolve_direct(
        game,
        "native text resolver",
        kTextResolverPrologue,
        fallback_rva,
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
            erui::LogLevel::verbose,
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
            erui::LogLevel::verbose,
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

const void* resolve_profile_vtable(
    const ModuleView& game,
    const char* name,
    std::uintptr_t profile_rva,
    std::size_t required_last_slot = 0) noexcept {
    auto* const address = game.at_rva(profile_rva);
    const std::size_t required_size =
        (required_last_slot + 1u) * sizeof(void*);
    if (!address || !game.contains(address, required_size)) {
        erui::detail::logf(
            erui::LogLevel::error,
            "Address %-22s profile RVA 0x%" PRIXPTR " is outside the selected game image",
            name,
            profile_rva);
        return nullptr;
    }

    void* first_entry{};
    void* required_entry{};
    std::memcpy(&first_entry, address, sizeof(first_entry));
    std::memcpy(
        &required_entry,
        address + required_last_slot * sizeof(void*),
        sizeof(required_entry));
    if (!game.is_executable(first_entry) ||
        !game.is_executable(required_entry)) {
        erui::detail::logf(
            erui::LogLevel::error,
            "Address %-22s profile RVA 0x%" PRIXPTR " failed vtable validation (slot=%zu)",
            name,
            profile_rva,
            required_last_slot);
        return nullptr;
    }

    erui::detail::logf(
        erui::LogLevel::verbose,
        "Address %-22s = %p (validated profile vtable RVA 0x%" PRIXPTR ")",
        name,
        static_cast<void*>(address),
        profile_rva);
    return address;
}

SelectedHelpPresenterFn resolve_selected_help_presenter(
    const ModuleView& game,
    const SelectedHelpPresenterAddressProfile* profile,
    const void* option_setting_vtable,
    ScaleformTextSetterFn text_setter) noexcept {
    constexpr std::size_t kPresenterCloneCallOffset = 0x68;
    constexpr std::size_t kPresenterCleanupOffset = 0x7B;
    constexpr std::size_t kHelpThunkSize = 37;
    constexpr std::size_t kHelpVtableSlot = 8;
    constexpr std::size_t kCloneWrapperSize = 57;
    constexpr std::size_t kInnerCloneSize = 68;

    const char* failed = "profile";
    void* observed_thunk{};
    std::int32_t displacement{};
    std::uint8_t* thunk_tail{};
    if (!profile || game.image_size() != profile->image_size ||
        !option_setting_vtable || !text_setter) {
        erui::detail::logf(
            erui::LogLevel::warning,
            "Selected-help presenter unavailable: exact profile/interfaces missing");
        return nullptr;
    }

    auto* const presenter = game.at_rva(profile->presenter_rva);
    auto* const clone = game.at_rva(profile->clone_wrapper_rva);
    auto* const inner = game.at_rva(profile->inner_clone_rva);
    auto* const two_owned_cleanup =
        game.at_rva(profile->two_owned_string_cleanup_rva);
    auto* const owned_clone = game.at_rva(profile->owned_string_clone_rva);
    auto* const expected_thunk = game.at_rva(profile->help_thunk_rva);
    if (!presenter || !clone || !inner || !two_owned_cleanup ||
        !owned_clone || !expected_thunk ||
        !game.contains(
            presenter,
            kPresenterCleanupOffset + kSelectedHelpCleanup.size()) ||
        !game.contains(clone, kCloneWrapperSize) ||
        !game.contains(inner, kInnerCloneSize) ||
        !game.contains(two_owned_cleanup, kTwoOwnedStringCleanupBody.size()) ||
        !game.contains(expected_thunk, kHelpThunkSize) ||
        !game.contains(option_setting_vtable,
            (kHelpVtableSlot + 1u) * sizeof(void*)) ||
        !game.is_executable(presenter) || !game.is_executable(clone) ||
        !game.is_executable(inner) || !game.is_executable(two_owned_cleanup) ||
        !game.is_executable(owned_clone) || !game.is_executable(expected_thunk)) {
        failed = "ranges";
        goto rejected;
    }

    failed = "presenter";
    if (std::memcmp(presenter, kSelectedHelpPresenterPrefix.data(),
            kSelectedHelpPresenterPrefix.size()) != 0 ||
        std::memcmp(
            presenter + kPresenterCloneCallOffset -
                kSelectedHelpClonePrefix.size(),
            kSelectedHelpClonePrefix.data(),
            kSelectedHelpClonePrefix.size()) != 0 ||
        resolve_rel32_call(
            game, presenter + kPresenterCloneCallOffset) != clone ||
        std::memcmp(
            presenter + kPresenterCloneCallOffset + 5u,
            kSelectedHelpCloneSuffix.data(),
            kSelectedHelpCloneSuffix.size()) != 0 ||
        std::memcmp(
            presenter + kPresenterCleanupOffset,
            kSelectedHelpCleanup.data(),
            kSelectedHelpCleanup.size()) != 0) {
        goto rejected;
    }

    failed = "clone-wrapper";
    if (std::memcmp(clone, kHelpCloneWrapperPrefix.data(),
            kHelpCloneWrapperPrefix.size()) != 0 ||
        resolve_rel32_call(
            game, clone + kHelpCloneWrapperPrefix.size()) != inner ||
        std::memcmp(
            clone + kHelpCloneWrapperPrefix.size() + 5u,
            kHelpCloneSuffix.data(),
            kHelpCloneSuffix.size()) != 0) {
        goto rejected;
    }

    failed = "inner-clone";
    if (std::memcmp(inner, kHelpInnerClonePrefix.data(),
            kHelpInnerClonePrefix.size()) != 0 ||
        resolve_rel32_call(
            game, inner + kHelpInnerClonePrefix.size()) != owned_clone ||
        std::memcmp(
            inner + kHelpInnerClonePrefix.size() + 5u,
            kHelpCloneSuffix.data(),
            kHelpCloneSuffix.size()) != 0) {
        goto rejected;
    }

    failed = "row-help-ownership";
    if (std::memcmp(
            two_owned_cleanup,
            kTwoOwnedStringCleanupBody.data(),
            kTwoOwnedStringCleanupBody.size()) != 0) {
        goto rejected;
    }

    failed = "help-thunk";
    std::memcpy(
        &observed_thunk,
        static_cast<const std::byte*>(option_setting_vtable) +
            kHelpVtableSlot * sizeof(void*),
        sizeof(observed_thunk));
    if (observed_thunk != expected_thunk ||
        std::memcmp(expected_thunk, kSelectedHelpThunkPrefix.data(),
            kSelectedHelpThunkPrefix.size()) != 0 ||
        expected_thunk[32] != 0xE9u) {
        goto rejected;
    }
    std::memcpy(&displacement, expected_thunk + 33, sizeof(displacement));
    thunk_tail = reinterpret_cast<std::uint8_t*>(
        reinterpret_cast<std::uintptr_t>(expected_thunk) + kHelpThunkSize +
        static_cast<std::intptr_t>(displacement));
    if (thunk_tail !=
        reinterpret_cast<std::uint8_t*>(text_setter)) {
        failed = "help-thunk-target";
        goto rejected;
    }

    erui::detail::logf(
        erui::LogLevel::verbose,
        "Address %-22s = %p (validated exact profile RVA 0x%" PRIXPTR ")",
        "selected help presenter",
        static_cast<void*>(presenter),
        profile->presenter_rva);
    return reinterpret_cast<SelectedHelpPresenterFn>(presenter);

rejected:
    erui::detail::logf(
        erui::LogLevel::warning,
        "Selected-help presenter exact profile rejected stage=%s RVA=0x%" PRIXPTR,
        failed,
        profile->presenter_rva);
    return nullptr;
}

[[nodiscard]] bool preserved_entry_matches_profile(
    const ModuleView& game,
    const void* address,
    std::uintptr_t expected_rva) noexcept {
    return address != nullptr &&
        address == game.at_rva(expected_rva) &&
        game.contains(address) &&
        game.is_executable(address);
}

[[nodiscard]] bool live_prefix_matches(
    const void* address,
    const std::array<unsigned char, 16>& approved) noexcept {
    return address &&
        std::memcmp(address, approved.data(), approved.size()) == 0;
}

[[nodiscard]] const void* absolute_indirect_jump_target(
    const unsigned char* entry) noexcept {
    if (!entry || entry[0] != 0xFF || entry[1] != 0x25) return nullptr;
    std::int32_t displacement{};
    std::memcpy(&displacement, entry + 2, sizeof(displacement));
    const std::uintptr_t instruction_end =
        reinterpret_cast<std::uintptr_t>(entry) + 6U;
    std::uintptr_t slot_value{};
    if (displacement >= 0) {
        const auto distance = static_cast<std::uintptr_t>(displacement);
        if (instruction_end >
            (std::numeric_limits<std::uintptr_t>::max)() - distance) {
            return nullptr;
        }
        slot_value = instruction_end + distance;
    } else {
        const auto distance = static_cast<std::uintptr_t>(
            -static_cast<std::int64_t>(displacement));
        if (instruction_end < distance) return nullptr;
        slot_value = instruction_end - distance;
    }

    const auto* const slot = reinterpret_cast<const void*>(slot_value);
    MEMORY_BASIC_INFORMATION memory{};
    if (VirtualQuery(slot, &memory, sizeof(memory)) != sizeof(memory) ||
        memory.State != MEM_COMMIT ||
        (memory.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) {
        return nullptr;
    }
    const auto region_end = reinterpret_cast<std::uintptr_t>(
        memory.BaseAddress) + memory.RegionSize;
    if (slot_value > region_end ||
        sizeof(void*) > region_end - slot_value) {
        return nullptr;
    }

    const void* target{};
#if defined(_MSC_VER)
    __try {
#endif
    std::memcpy(&target, slot, sizeof(target));
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
#endif
    return target;
}

[[nodiscard]] bool approved_detour_target_matches(
    const void* entry,
    const std::array<unsigned char, 16>& pristine,
    const std::array<unsigned char, 16>& approved,
    const void* approved_target) noexcept {
    if (approved == pristine) return approved_target == nullptr;
    return approved_target &&
        absolute_indirect_jump_target(
            static_cast<const unsigned char*>(entry)) == approved_target;
}

template <typename Function>
[[nodiscard]] bool capture_attach_entry(
    std::uint8_t* image_base,
    std::size_t image_size,
    std::uintptr_t rva,
    const std::array<unsigned char, 16>& expected_prefix,
    Function& function,
    std::array<unsigned char, 16>& pristine_prefix) noexcept {
    if (!image_base || rva >= image_size ||
        pristine_prefix.size() > image_size - rva) {
        return false;
    }
    auto* const entry = image_base + rva;
    pristine_prefix = expected_prefix;
    function = reinterpret_cast<Function>(entry);
    return true;
}

[[nodiscard]] bool canonical_prefixes_are_complete(
    const PreservedSharedMenuAddresses& preserved) noexcept {
    return preserved.game_options_pristine_prefix ==
            kGameOptionsEntryPrefix &&
        preserved.sub_pristine_prefix == kSubHandlerEntryPrefix &&
        preserved.text_pristine_prefix == kTextResolverEntryPrefix &&
        (!preserved.slider_refresh ||
            preserved.slider_pristine_prefix == kSliderRefreshEntryPrefix);
}

[[nodiscard]] bool validate_preserved_shared_menu_entry(
    const ModuleView& game,
    const PreservedSharedMenuAddresses& preserved,
    const SharedMenuAddressProfile* profile,
    PreservedSharedMenuEntry entry) noexcept {
    if (!profile || !preserved.required_complete() ||
        !preserved.required_entries_approved ||
        preserved.game_image_base != game.base() ||
        preserved.game_image_size != game.image_size()) {
        return false;
    }

    const void* address{};
    std::uintptr_t expected_rva{};
    const std::array<unsigned char,
        PreservedSharedMenuAddresses::entry_prefix_size>* pristine{};
    const std::array<unsigned char,
        PreservedSharedMenuAddresses::entry_prefix_size>* approved{};
    const void* approved_target{};

    switch (entry) {
    case PreservedSharedMenuEntry::game_options:
        address = reinterpret_cast<const void*>(
            preserved.game_options_handler);
        expected_rva = profile->game_options_handler_rva;
        pristine = &preserved.game_options_pristine_prefix;
        approved = &preserved.game_options_approved_prefix;
        approved_target = preserved.approved_detour_targets[0];
        if (*pristine != kGameOptionsEntryPrefix) return false;
        break;
    case PreservedSharedMenuEntry::sub_handler:
        address = reinterpret_cast<const void*>(preserved.sub_handler);
        expected_rva = profile->sub_handler_rva;
        pristine = &preserved.sub_pristine_prefix;
        approved = &preserved.sub_approved_prefix;
        approved_target = preserved.approved_detour_targets[1];
        if (*pristine != kSubHandlerEntryPrefix) return false;
        break;
    case PreservedSharedMenuEntry::text_resolver:
        address = reinterpret_cast<const void*>(preserved.text_resolver);
        expected_rva = profile->text_resolver_rva;
        pristine = &preserved.text_pristine_prefix;
        approved = &preserved.text_approved_prefix;
        approved_target = preserved.approved_detour_targets[2];
        if (*pristine != kTextResolverEntryPrefix) return false;
        break;
    case PreservedSharedMenuEntry::slider_refresh:
        if (!preserved.slider_refresh ||
            !preserved.slider_entry_approved) {
            return false;
        }
        address = reinterpret_cast<const void*>(preserved.slider_refresh);
        expected_rva = profile->slider_refresh_rva;
        pristine = &preserved.slider_pristine_prefix;
        approved = &preserved.slider_approved_prefix;
        approved_target = preserved.slider_approved_detour_target;
        if (*pristine != kSliderRefreshEntryPrefix) return false;
        break;
    case PreservedSharedMenuEntry::selector_select:
        if (!preserved.selector_select ||
            !preserved.selector_select_entry_approved) {
            return false;
        }
        address = preserved.selector_select;
        expected_rva = profile->selector_select_rva;
        pristine = &preserved.selector_select_pristine_prefix;
        approved = &preserved.selector_select_approved_prefix;
        approved_target =
            preserved.selector_select_approved_detour_target;
        if (*pristine != kSelectorSelectEntryPrefix) return false;
        break;
    default:
        return false;
    }

    return preserved_entry_matches_profile(game, address, expected_rva) &&
        live_prefix_matches(address, *approved) &&
        approved_detour_target_matches(
            address, *pristine, *approved, approved_target);
}

[[nodiscard]] bool validate_preserved_shared_menu_addresses(
    const ModuleView& game,
    const PreservedSharedMenuAddresses& preserved,
    const SharedMenuAddressProfile* profile) noexcept {
    return profile && preserved.required_complete() &&
        preserved.required_entries_approved &&
        canonical_prefixes_are_complete(preserved) &&
        preserved.game_image_base == game.base() &&
        preserved.game_image_size == game.image_size() &&
        validate_preserved_shared_menu_entry(
            game,
            preserved,
            profile,
            PreservedSharedMenuEntry::game_options) &&
        validate_preserved_shared_menu_entry(
            game,
            preserved,
            profile,
            PreservedSharedMenuEntry::sub_handler) &&
        validate_preserved_shared_menu_entry(
            game,
            preserved,
            profile,
            PreservedSharedMenuEntry::text_resolver) &&
        (!preserved.slider_refresh ||
            validate_preserved_shared_menu_entry(
                game,
                preserved,
                profile,
                PreservedSharedMenuEntry::slider_refresh));
}

} // namespace

bool GameAddresses::complete(
    bool require_rows,
    bool require_buttons,
    bool require_submenus,
    bool require_popup_choices,
    bool require_text_inputs,
    bool require_row_presentations,
    bool require_custom_text) const noexcept {
    const bool rows_ready = !require_rows || (
        game_options_handler && on_off_list && menu_context && text_ref_help &&
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
    const bool text_input_ready =
        !require_text_inputs || text_input_complete();
    const bool row_presentation_ready = !require_row_presentations ||
        row_presentation_complete();
    return rows_ready && buttons_ready && submenus_ready &&
        popup_choices_ready && text_input_ready &&
        row_presentation_ready && text_ready;
}

bool capture_preserved_shared_menu_addresses_at_attach(
    HMODULE game_module,
    PreservedSharedMenuAddresses& output) noexcept {
    output = {};
    auto* const image_base = reinterpret_cast<std::uint8_t*>(game_module);
    if (!image_base) return false;

    const auto* const dos =
        reinterpret_cast<const IMAGE_DOS_HEADER*>(image_base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 ||
        static_cast<std::uint64_t>(dos->e_lfanew) > UINT64_C(0x100000)) {
        return false;
    }
    const auto* const nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(
        image_base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
        nt->OptionalHeader.SizeOfImage < 0x1000U) {
        return false;
    }

    const std::size_t image_size = nt->OptionalHeader.SizeOfImage;
    const EldenRingBuild build = identify_elden_ring_build(
        nt->FileHeader.TimeDateStamp, image_size);
    const SharedMenuAddressProfile* const profile =
        find_shared_menu_address_profile(build);
    if (!profile) return false;

    PreservedSharedMenuAddresses captured{};
    captured.game_image_base = image_base;
    captured.game_image_size = image_size;
    if (!capture_attach_entry(
            image_base,
            image_size,
            profile->game_options_handler_rva,
            kGameOptionsEntryPrefix,
            captured.game_options_handler,
            captured.game_options_pristine_prefix) ||
        !capture_attach_entry(
            image_base,
            image_size,
            profile->sub_handler_rva,
            kSubHandlerEntryPrefix,
            captured.sub_handler,
            captured.sub_pristine_prefix) ||
        !capture_attach_entry(
            image_base,
            image_size,
            profile->text_resolver_rva,
            kTextResolverEntryPrefix,
            captured.text_resolver,
            captured.text_pristine_prefix) ||
        !capture_attach_entry(
            image_base,
            image_size,
            profile->selector_select_rva,
            kSelectorSelectEntryPrefix,
            captured.selector_select,
            captured.selector_select_pristine_prefix)) {
        return false;
    }

    // Slider presentation is optional at installation time, but its exact
    // address and canonical prefix are available for every recognized shared
    // profile. Later ownership validation decides whether the live entry is
    // pristine, an approved detour, or unavailable.
    (void)capture_attach_entry(
        image_base,
        image_size,
        profile->slider_refresh_rva,
        kSliderRefreshEntryPrefix,
        captured.slider_refresh,
        captured.slider_pristine_prefix);
    output = captured;
    return true;
}

bool capture_preserved_shared_menu_addresses(
    const ModuleView& game,
    PreservedSharedMenuAddresses& output) noexcept {
    const EldenRingBuild build = identify_elden_ring_build(
        game.timestamp(), game.image_size());
    const SharedMenuAddressProfile* const profile =
        find_shared_menu_address_profile(build);
    if (!profile) {
        erui::detail::logf(
            erui::LogLevel::verbose,
            "Shared menu snapshot unavailable: unsupported game image timestamp=0x%08X size=0x%zX",
            static_cast<unsigned>(game.timestamp()),
            game.image_size());
        return false;
    }

    if (!capture_preserved_shared_menu_addresses_at_attach(
            reinterpret_cast<HMODULE>(game.base()), output) ||
        output.game_image_base != game.base() ||
        output.game_image_size != game.image_size() ||
        !game.is_executable(
            reinterpret_cast<const void*>(output.game_options_handler)) ||
        !game.is_executable(
            reinterpret_cast<const void*>(output.sub_handler)) ||
        !game.is_executable(
            reinterpret_cast<const void*>(output.text_resolver)) ||
        !game.is_executable(output.selector_select) ||
        !game.matches(
            reinterpret_cast<const void*>(output.game_options_handler),
            kGameOptionsHandlerPattern) ||
        !game.matches(
            reinterpret_cast<const void*>(output.sub_handler),
            kSubHandlerPattern) ||
        !game.matches(
            reinterpret_cast<const void*>(output.text_resolver),
            kTextResolverPrologue) ||
        std::memcmp(
            output.selector_select,
            kSelectorSelectEntryPrefix.data(),
            kSelectorSelectEntryPrefix.size()) != 0) {
        output = {};
        erui::detail::logf(
            erui::LogLevel::verbose,
            "Shared menu snapshot rejected: one or more required pristine prologues did not match Elden Ring %.*s",
            static_cast<int>(elden_ring_build_name(build).size()),
            elden_ring_build_name(build).data());
        return false;
    }

    if (output.slider_refresh &&
        (!game.is_executable(
             reinterpret_cast<const void*>(output.slider_refresh)) ||
         !game.matches(
             reinterpret_cast<const void*>(output.slider_refresh),
             kSliderRefreshPattern))) {
        output.slider_refresh = nullptr;
        output.slider_pristine_prefix = {};
    }

    output.game_options_approved_prefix =
        output.game_options_pristine_prefix;
    output.sub_approved_prefix = output.sub_pristine_prefix;
    output.text_approved_prefix = output.text_pristine_prefix;
    output.selector_select_approved_prefix =
        output.selector_select_pristine_prefix;
    output.required_entries_approved = true;
    output.selector_select_entry_approved = true;
    if (output.slider_refresh) {
        output.slider_approved_prefix = output.slider_pristine_prefix;
        output.slider_entry_approved = true;
    }

    erui::detail::logf(
        erui::LogLevel::verbose,
        "Shared menu snapshot captured: Elden Ring %.*s slider=%u",
        static_cast<int>(elden_ring_build_name(build).size()),
        elden_ring_build_name(build).data(),
        output.slider_refresh ? 1U : 0U);
    return true;
}

bool revalidate_preserved_shared_menu_addresses(
    const ModuleView& game,
    const PreservedSharedMenuAddresses& preserved) noexcept {
    const EldenRingBuild build = identify_elden_ring_build(
        game.timestamp(), game.image_size());
    return validate_preserved_shared_menu_addresses(
        game,
        preserved,
        find_shared_menu_address_profile(build));
}

bool revalidate_preserved_shared_menu_entry(
    const ModuleView& game,
    const PreservedSharedMenuAddresses& preserved,
    PreservedSharedMenuEntry entry) noexcept {
    const EldenRingBuild build = identify_elden_ring_build(
        game.timestamp(), game.image_size());
    return validate_preserved_shared_menu_entry(
        game,
        preserved,
        find_shared_menu_address_profile(build),
        entry);
}

bool preflight_row_presentation_addresses(
    const ModuleView& game) noexcept {
    const EldenRingBuild game_build = identify_elden_ring_build(
        game.timestamp(), game.image_size());
    if (game_build != EldenRingBuild::v2_7_1_0 &&
        game_build != EldenRingBuild::v2_7_2_0) {
        return false;
    }

    const CoreUiAddressProfile* const core_ui_profile =
        find_core_ui_address_profile(game_build);
    const MenuRuntimeAddressProfile* const menu_runtime_profile =
        find_menu_runtime_address_profile(game_build);
    const TextInputAddressProfile* const text_input_profile =
        find_text_input_address_profile(game_build);
    const SelectedHelpPresenterAddressProfile* const help_presenter_profile =
        find_selected_help_presenter_address_profile(game_build);
    if (!core_ui_profile || !menu_runtime_profile || !text_input_profile ||
        !help_presenter_profile) {
        return false;
    }

    GameAddresses addresses{};
    addresses.game_image_base = game.base();
    addresses.game_image_size = game.image_size();
    addresses.scaleform_path_resolver =
        reinterpret_cast<ScaleformPathResolverFn>(resolve_optional_direct(
            game,
            "Scaleform path",
            kScaleformPathResolverPattern,
            menu_runtime_profile->scaleform_path_resolver_rva,
            kScaleformPathResolverPattern));
    addresses.scaleform_text_setter =
        reinterpret_cast<ScaleformTextSetterFn>(resolve_optional_direct(
            game,
            "Scaleform text setter",
            kScaleformTextSetterPattern,
            menu_runtime_profile->scaleform_text_setter_rva,
            kScaleformTextSetterPattern));
    addresses.scaleform_result_destructor =
        reinterpret_cast<ScaleformResultDestructorFn>(resolve_optional_direct(
            game,
            "Scaleform result dtor",
            kScaleformResultDestructorPattern,
            core_ui_profile->scaleform_result_destructor_rva,
            kScaleformResultDestructorPattern));
    addresses.option_setting_dialog_vtable = resolve_profile_vtable(
        game,
        "OptionSetting vtable",
        text_input_profile->option_setting_dialog_vtable_rva);
    addresses.edit_property_vtable = resolve_profile_vtable(
        game,
        "EditProperty vtable",
        text_input_profile->edit_property_vtable_rva);
    addresses.text_input_controller_vtable = resolve_profile_vtable(
        game,
        "row controller vtable",
        text_input_profile->text_input_controller_vtable_rva,
        6);
    addresses.action_button_controller_vtable = resolve_profile_vtable(
        game,
        "action-button controller vtable",
        text_input_profile->action_button_controller_vtable_rva,
        6);
    addresses.page_frame = reinterpret_cast<PageFrameFn>(
        resolve_optional_direct(
            game,
            "page frame",
            kPageFramePattern,
            menu_runtime_profile->page_frame_rva,
            kPageFramePattern));
    addresses.selected_help_presenter = resolve_selected_help_presenter(
        game,
        help_presenter_profile,
        addresses.option_setting_dialog_vtable,
        addresses.scaleform_text_setter);

    if (!addresses.row_presentation_complete()) {
        erui::detail::logf(
            erui::LogLevel::warning,
            "Runtime row-presentation preflight rejected one or more required native interfaces");
        return false;
    }
    return true;
}

bool resolve_game_addresses(
    const ModuleView& game,
    GameAddresses& output,
    bool require_rows,
    bool require_buttons,
    bool require_submenus,
    bool require_native_back,
    bool require_popup_choices,
    bool require_text_inputs,
    bool require_row_presentations,
    bool require_custom_text,
    const PreservedSharedMenuAddresses* preserved_shared) noexcept {
    output = {};
    output.game_image_base = game.base();
    output.game_image_size = game.image_size();
    const EldenRingBuild game_build = identify_elden_ring_build(
        game.timestamp(), game.image_size());
    const CoreUiAddressProfile* const core_ui_profile =
        find_core_ui_address_profile(game_build);
    const SharedMenuAddressProfile* const shared_menu_profile =
        find_shared_menu_address_profile(game_build);
    const MenuRuntimeAddressProfile* const menu_runtime_profile =
        find_menu_runtime_address_profile(game_build);
    const SelectedHelpPresenterAddressProfile* const help_presenter_profile =
        find_selected_help_presenter_address_profile(game_build);
    const std::uintptr_t scaleform_result_destructor_fallback_rva =
        core_ui_profile
            ? core_ui_profile->scaleform_result_destructor_rva
            : std::uintptr_t{0};

    if (!require_rows) {
        erui::detail::logf(
            erui::LogLevel::error,
            "Row injection is disabled; there is nothing to resolve.");
        return false;
    }

    // Address scans are used to prove the selected exact profile, not to
    // admit an otherwise unknown executable. A unique byte pattern in a new
    // build is not sufficient evidence that every associated ABI, vtable and
    // object layout is still compatible.
    if (!core_ui_profile || !shared_menu_profile ||
        !menu_runtime_profile) {
        erui::detail::logf(
            erui::LogLevel::error,
            "Unsupported Elden Ring executable identity timestamp=0x%08X imageSize=0x%zX; native UI hooks were not installed",
            game.timestamp(),
            game.image_size());
        return false;
    }

    if (preserved_shared &&
        !validate_preserved_shared_menu_addresses(
            game, *preserved_shared, shared_menu_profile)) {
        erui::detail::logf(
            erui::LogLevel::error,
            "Preserved shared menu interfaces do not match the selected exact game profile");
        return false;
    }

    if (preserved_shared) {
        erui::detail::logf(
            erui::LogLevel::info,
            "Using approved exact-profile shared menu interfaces for cooperative hook chaining");
    }

    if (require_rows) {
        output.game_options_handler = preserved_shared
            ? preserved_shared->game_options_handler
            : reinterpret_cast<GameOptionsHandlerFn>(resolve_direct(
                game,
                "Game Options handler",
                kGameOptionsHandlerPattern,
                shared_menu_profile
                    ? shared_menu_profile->game_options_handler_rva
                    : std::uintptr_t{0},
                kGameOptionsHandlerPattern));
        // Each built-in destination is optional and independently hooked.
        // A signature failure disables only that panel; Game Options and the
        // other resolved destinations remain available.
        output.camera_panel_materializer =
            reinterpret_cast<BuiltinPanelMaterializerFn>(resolve_optional_direct(
                game,
                "Camera panel rows",
                kCameraPanelMaterializerPattern,
                menu_runtime_profile
                    ? menu_runtime_profile->camera_panel_materializer_rva
                    : std::uintptr_t{0},
                kCameraPanelMaterializerPattern));
        output.display_panel_materializer =
            reinterpret_cast<BuiltinPanelMaterializerFn>(resolve_optional_direct(
                game,
                "Display panel rows",
                kDisplayPanelMaterializerPattern,
                menu_runtime_profile
                    ? menu_runtime_profile->display_panel_materializer_rva
                    : std::uintptr_t{0},
                kDisplayPanelMaterializerPattern));
        output.sound_panel_materializer =
            reinterpret_cast<BuiltinPanelMaterializerFn>(resolve_optional_direct(
                game,
                "Sound panel rows",
                kSoundPanelMaterializerPattern,
                menu_runtime_profile
                    ? menu_runtime_profile->sound_panel_materializer_rva
                    : std::uintptr_t{0},
                kSoundPanelMaterializerPattern));
        output.network_panel_materializer =
            reinterpret_cast<BuiltinPanelMaterializerFn>(resolve_optional_direct(
                game,
                "Network panel rows",
                kNetworkPanelMaterializerPattern,
                menu_runtime_profile
                    ? menu_runtime_profile->network_panel_materializer_rva
                    : std::uintptr_t{0},
                kNetworkPanelMaterializerPattern));
        output.keyboard_mouse_panel_materializer =
            reinterpret_cast<BuiltinPanelMaterializerFn>(
                resolve_optional_direct(
                    game,
                    "Keyboard panel rows",
                    kKeyboardMousePanelMaterializerPattern,
                    menu_runtime_profile
                        ? menu_runtime_profile->keyboard_mouse_panel_materializer_rva
                        : std::uintptr_t{0},
                    kKeyboardMousePanelMaterializerPattern));
        output.graphics_panel_materializer =
            reinterpret_cast<BuiltinPanelMaterializerFn>(resolve_optional_direct(
                game,
                "Graphics panel rows",
                kGraphicsPanelMaterializerPattern,
                menu_runtime_profile
                    ? menu_runtime_profile->graphics_panel_materializer_rva
                    : std::uintptr_t{0},
                kGraphicsPanelMaterializerPattern));
        if (require_submenus) {
            output.sub_handler = preserved_shared
                ? preserved_shared->sub_handler
                : reinterpret_cast<SubHandlerFn>(resolve_direct(
                    game,
                    "subpage handler",
                    kSubHandlerPattern,
                    shared_menu_profile
                        ? shared_menu_profile->sub_handler_rva
                        : std::uintptr_t{0},
                    kSubHandlerPattern));
            output.open_sub_page = reinterpret_cast<OpenSubPageFn>(resolve_direct(
                game,
                "open subpage",
                kOpenSubPagePattern,
                menu_runtime_profile
                    ? menu_runtime_profile->open_sub_page_rva
                    : std::uintptr_t{0},
                kOpenSubPagePattern));

            // Presentation is deliberately optional. A game update may disable
            // custom titles without preventing page construction or row input.
            output.scaleform_path_resolver =
                reinterpret_cast<ScaleformPathResolverFn>(resolve_optional_direct(
                    game,
                    "Scaleform path",
                    kScaleformPathResolverPattern,
                    menu_runtime_profile
                        ? menu_runtime_profile->scaleform_path_resolver_rva
                        : std::uintptr_t{0},
                    kScaleformPathResolverPattern));
            output.scaleform_text_setter =
                reinterpret_cast<ScaleformTextSetterFn>(resolve_optional_direct(
                    game,
                    "Scaleform text setter",
                    kScaleformTextSetterPattern,
                    menu_runtime_profile
                        ? menu_runtime_profile->scaleform_text_setter_rva
                        : std::uintptr_t{0},
                    kScaleformTextSetterPattern));
            output.scaleform_result_destructor =
                reinterpret_cast<ScaleformResultDestructorFn>(resolve_optional_direct(
                    game,
                    "Scaleform result dtor",
                    kScaleformResultDestructorPattern,
                    scaleform_result_destructor_fallback_rva,
                    kScaleformResultDestructorPattern));
        } else {
            erui::detail::logf(
                erui::LogLevel::verbose,
                "Address resolution: no native submenu rows declared");

            // API 1.2 formatted sliders reuse the same narrow Scaleform text
            // bridge even when every row lives directly on a built-in page.
            // Resolve these interfaces opportunistically; runtime.cpp makes
            // them mandatory only when a compiled formatted slider exists.
            output.scaleform_path_resolver =
                reinterpret_cast<ScaleformPathResolverFn>(resolve_optional_direct(
                    game,
                    "Scaleform path",
                    kScaleformPathResolverPattern,
                    menu_runtime_profile
                        ? menu_runtime_profile->scaleform_path_resolver_rva
                        : std::uintptr_t{0},
                    kScaleformPathResolverPattern));
            output.scaleform_text_setter =
                reinterpret_cast<ScaleformTextSetterFn>(resolve_optional_direct(
                    game,
                    "Scaleform text setter",
                    kScaleformTextSetterPattern,
                    menu_runtime_profile
                        ? menu_runtime_profile->scaleform_text_setter_rva
                        : std::uintptr_t{0},
                    kScaleformTextSetterPattern));
            output.scaleform_result_destructor =
                reinterpret_cast<ScaleformResultDestructorFn>(
                    resolve_optional_direct(
                        game,
                        "Scaleform result dtor",
                        kScaleformResultDestructorPattern,
                        scaleform_result_destructor_fallback_rva,
                        kScaleformResultDestructorPattern));
        }
        output.on_off_list = reinterpret_cast<BufferConstructorFn>(resolve_direct(
            game,
            "on/off list",
            kOnOffListPattern,
            menu_runtime_profile
                ? menu_runtime_profile->on_off_list_rva
                : std::uintptr_t{0},
            kOnOffListPattern));
        output.menu_context = reinterpret_cast<BufferConstructorFn>(resolve_direct(
            game,
            "menu context",
            kMenuContextPattern,
            menu_runtime_profile
                ? menu_runtime_profile->menu_context_rva
                : std::uintptr_t{0},
            kMenuContextPattern));
        output.text_ref_help = reinterpret_cast<TextReferenceFn>(resolve_direct(
            game,
            "text ref help",
            kTextRefHelpPattern,
            menu_runtime_profile
                ? menu_runtime_profile->text_ref_help_rva
                : std::uintptr_t{0},
            "4C 8B DC 57 48 81 EC 90 00 00 00"));
        output.text_ref_label = reinterpret_cast<TextReferenceFn>(resolve_call_target(
            game,
            "text ref label",
            kTextRefLabelCallsitePattern,
            kTextRefLabelCallOffset,
            menu_runtime_profile
                ? menu_runtime_profile->text_ref_label_rva
                : std::uintptr_t{0}));
        if (require_buttons) {
            output.root_button_text_references =
                reinterpret_cast<BufferConstructorFn>(resolve_direct(
                    game,
                    "root button text refs",
                    kRootButtonTextReferencesPattern,
                    menu_runtime_profile
                        ? menu_runtime_profile->root_button_text_references_rva
                        : std::uintptr_t{0},
                    kRootButtonTextReferencesPattern));
            output.root_button_display_text =
                reinterpret_cast<TextReferenceFn>(resolve_direct(
                    game,
                    "root button display",
                    kRootButtonDisplayTextPattern,
                    menu_runtime_profile
                        ? menu_runtime_profile->root_button_display_text_rva
                        : std::uintptr_t{0},
                    kRootButtonDisplayTextPattern));
        }
        output.add_toggle = reinterpret_cast<AddToggleFn>(resolve_call_target(
            game,
            "add toggle",
            kAddToggleCallsitePattern,
            kAddToggleCallOffset,
            menu_runtime_profile
                ? menu_runtime_profile->add_toggle_rva
                : std::uintptr_t{0},
            kAddTogglePrologue));
        output.add_inline_choice =
            reinterpret_cast<AddInlineChoiceFn>(resolve_call_target(
            game, "add inline choice", kAddChoiceCallsitePattern,
            kAddChoiceCallOffset,
            menu_runtime_profile
                ? menu_runtime_profile->add_inline_choice_rva
                : std::uintptr_t{0},
            kAddChoicePrologue));
        output.choice_context_constructor =
            reinterpret_cast<ChoiceContextConstructorFn>(resolve_optional_direct(
                game, "choice context", kChoiceContextConstructorPrologue,
                menu_runtime_profile
                    ? menu_runtime_profile->choice_context_constructor_rva
                    : std::uintptr_t{0},
                kChoiceContextConstructorPrologue));
        output.choice_list_builder = reinterpret_cast<ChoiceListBuilderFn>(
            resolve_optional_direct(
                game, "choice list builder", kChoiceListBuilderPrologue,
                menu_runtime_profile
                    ? menu_runtime_profile->choice_list_builder_rva
                    : std::uintptr_t{0},
                kChoiceListBuilderPrologue));
        output.add_slider = reinterpret_cast<AddSliderFn>(resolve_call_target(
            game,
            "add slider",
            kAddSliderCallsitePattern,
            kAddSliderCallOffset,
            menu_runtime_profile
                ? menu_runtime_profile->add_slider_rva
                : std::uintptr_t{0},
            kAddSliderPrologue));
        // Optional until a compiled API 1.2 NumericSlider or CustomSlider
        // requests formatted presentation. An unresolved formatter must not
        // disable ordinary API 1.0/1.1 integer sliders.
        output.slider_refresh = preserved_shared
            ? preserved_shared->slider_refresh
            : reinterpret_cast<SliderRefreshFn>(resolve_optional_direct(
                game,
                "slider value refresh",
                kSliderRefreshPattern,
                shared_menu_profile
                    ? shared_menu_profile->slider_refresh_rva
                    : std::uintptr_t{0},
                kSliderRefreshPattern));

        // Inline and popup selectors both clone their text references during
        // construction, after which the temporary source object is destroyed.
        output.destroy_text_references =
            reinterpret_cast<DestroyTextReferencesFn>(resolve_direct(
                game,
                "destroy text refs",
                kDestroyTextReferencesPrologue,
                menu_runtime_profile
                    ? menu_runtime_profile->destroy_text_references_rva
                    : std::uintptr_t{0},
                kDestroyTextReferencesPrologue));

        if (require_buttons) {
            output.add_button = reinterpret_cast<AddButtonFn>(resolve_direct(
                game,
                "add button",
                kAddButtonPrologue,
                menu_runtime_profile
                    ? menu_runtime_profile->add_button_rva
                    : std::uintptr_t{0},
                kAddButtonPrologue));
        } else {
            erui::detail::logf(
                erui::LogLevel::verbose,
                "Address resolution: no native button rows declared");
        }

        if (require_popup_choices) {
            output.popup_choice_constructor =
                reinterpret_cast<PopupChoiceConstructorFn>(resolve_direct(
                    game,
                    "popup choice ctor",
                    kPopupChoiceConstructorPattern,
                    menu_runtime_profile
                        ? menu_runtime_profile->popup_choice_constructor_rva
                        : std::uintptr_t{0},
                    kPopupChoiceConstructorPattern));
            output.popup_choice_list_provider =
                reinterpret_cast<PopupChoiceListProviderFn>(resolve_direct(
                    game,
                    "popup list provider",
                    kPopupChoiceListProviderPattern,
                    menu_runtime_profile
                        ? menu_runtime_profile->popup_choice_list_provider_rva
                        : std::uintptr_t{0},
                    kPopupChoiceListProviderPattern));
            output.popup_choice_list_template =
                reinterpret_cast<PopupChoiceListTemplateFn>(resolve_direct(
                    game,
                    "popup list template",
                    kPopupChoiceListTemplatePattern,
                    menu_runtime_profile
                        ? menu_runtime_profile->popup_choice_list_template_rva
                        : std::uintptr_t{0},
                    kPopupChoiceListTemplatePattern));
            auto* value_invoke = resolve_direct(
                game,
                "popup value invoke",
                kPopupChoiceValueInvokePattern,
                menu_runtime_profile
                    ? menu_runtime_profile->popup_choice_value_invoke_rva
                    : std::uintptr_t{0},
                kPopupChoiceValueInvokePattern);
            auto* selection_invoke = resolve_direct(
                game,
                "popup commit invoke",
                kPopupChoiceSelectionInvokePattern,
                menu_runtime_profile
                    ? menu_runtime_profile->popup_choice_selection_invoke_rva
                    : std::uintptr_t{0},
                kPopupChoiceSelectionInvokePattern);
            if (value_invoke) {
                output.popup_choice_value_vtable = resolve_callable_vtable(
                    game,
                    "popup value vtable",
                    value_invoke,
                    menu_runtime_profile
                        ? menu_runtime_profile->popup_choice_value_vtable_rva
                        : std::uintptr_t{0});
            }
            if (selection_invoke) {
                output.popup_choice_selection_vtable =
                    resolve_callable_vtable(
                        game,
                        "popup commit vtable",
                        selection_invoke,
                        menu_runtime_profile
                            ? menu_runtime_profile->popup_choice_selection_vtable_rva
                            : std::uintptr_t{0});
            }
            if (output.popup_choice_list_provider) {
                output.popup_choice_presentation_vtable =
                    resolve_callable_vtable(
                        game,
                        "popup list vtable",
                        reinterpret_cast<const void*>(
                            output.popup_choice_list_provider),
                        menu_runtime_profile
                            ? menu_runtime_profile->popup_choice_presentation_vtable_rva
                            : std::uintptr_t{0});
            }
        } else {
            erui::detail::logf(
                erui::LogLevel::verbose,
                "Address resolution: no popup-choice rows declared");
        }
    } else {
        erui::detail::logf(
            erui::LogLevel::verbose,
            "Address resolution: native row path disabled");
    }

    if (require_custom_text) {
        output.text_resolver = preserved_shared
            ? preserved_shared->text_resolver
            : reinterpret_cast<TextResolverFn>(resolve_text_resolver(
                game,
                shared_menu_profile
                    ? shared_menu_profile->text_resolver_rva
                    : std::uintptr_t{0}));
    } else {
        erui::detail::logf(
            erui::LogLevel::verbose,
            "Address resolution: custom text hook disabled");
    }

    const TextInputAddressProfile* const text_input_profile =
        find_text_input_address_profile(game_build);
    if (!text_input_profile) {
        if (require_text_inputs || require_row_presentations) {
            erui::detail::logf(
                erui::LogLevel::error,
                "TextInput/row-presentation interfaces unavailable: unsupported game image timestamp=0x%08X size=0x%zX",
                static_cast<unsigned>(game.timestamp()),
                game.image_size());
        } else {
            erui::detail::logf(
                erui::LogLevel::verbose,
                "Address resolution: no compatible TextInput path for this game image");
        }
    } else {
        const std::string_view build_name =
            elden_ring_build_name(game_build);
        erui::detail::logf(
            erui::LogLevel::verbose,
            "TextInput/row-presentation address profile selected: Elden Ring %.*s",
            static_cast<int>(build_name.size()),
            build_name.data());
        if (require_text_inputs) {
            output.text_input_row_producer =
                reinterpret_cast<TextInputRowProducerFn>(resolve_optional_direct(
                    game, "text input producer", kTextInputRowProducerPattern,
                    text_input_profile->row_producer_rva,
                    kTextInputRowProducerPattern));
            output.native_menu_string_constructor =
                reinterpret_cast<NativeMenuStringConstructorFn>(
                    resolve_optional_direct(
                        game, "menu string ctor",
                        kNativeMenuStringConstructorPattern,
                        text_input_profile->menu_string_constructor_rva,
                        kNativeMenuStringConstructorPattern));
            output.native_menu_string_borrowed_constructor =
                reinterpret_cast<NativeMenuStringBorrowedConstructorFn>(
                    resolve_optional_direct(
                        game, "menu string literal ctor",
                        kNativeMenuStringBorrowedConstructorPattern,
                        text_input_profile->menu_string_borrowed_constructor_rva,
                        kNativeMenuStringBorrowedConstructorPattern));
            output.native_menu_string_destructor =
                reinterpret_cast<NativeMenuStringDestructorFn>(
                    resolve_optional_direct(
                        game, "menu string dtor",
                        kNativeMenuStringDestructorPattern,
                        text_input_profile->menu_string_destructor_rva,
                        kNativeMenuStringDestructorPattern));
            output.text_input_editor_factory_builder =
                reinterpret_cast<TextInputEditorFactoryBuilderFn>(
                    resolve_optional_direct(
                        game, "text editor builder",
                        kTextInputEditorFactoryBuilderPattern,
                        text_input_profile->editor_factory_builder_rva,
                        kTextInputEditorFactoryBuilderPattern));
            output.text_input_editor_factory =
                reinterpret_cast<TextInputEditorFactoryFn>(
                    resolve_optional_direct(
                        game, "text editor factory",
                        kTextInputEditorFactoryPattern,
                        text_input_profile->editor_factory_rva,
                        kTextInputEditorFactoryPattern));
        }

        output.option_setting_dialog_vtable = resolve_profile_vtable(
            game,
            "OptionSetting vtable",
            text_input_profile->option_setting_dialog_vtable_rva);
        output.edit_property_vtable = resolve_profile_vtable(
            game,
            "EditProperty vtable",
            text_input_profile->edit_property_vtable_rva);
        output.text_input_controller_vtable = resolve_profile_vtable(
            game,
            "row controller vtable",
            text_input_profile->text_input_controller_vtable_rva,
            6);
        output.action_button_controller_vtable = resolve_profile_vtable(
            game,
            "action-button controller vtable",
            text_input_profile->action_button_controller_vtable_rva,
            6);
        if (require_row_presentations) {
            output.selected_help_presenter =
                resolve_selected_help_presenter(
                    game,
                    help_presenter_profile,
                    output.option_setting_dialog_vtable,
                    output.scaleform_text_setter);
        }
        if (require_text_inputs) {
            output.pad_setting_dialog_vtable = resolve_profile_vtable(
                game,
                "PadSetting vtable",
                text_input_profile->pad_setting_dialog_vtable_rva);
            output.software_keyboard_job_vtable = resolve_profile_vtable(
                game,
                "software keyboard vtable",
                text_input_profile->software_keyboard_job_vtable_rva);
        }
        if (require_text_inputs && !output.text_input_complete()) {
            erui::detail::logf(
                erui::LogLevel::error,
                "TextInput unavailable: one or more native interfaces were unresolved");
        }
        if (require_row_presentations &&
            (!output.option_setting_dialog_vtable ||
             !output.edit_property_vtable ||
             !output.text_input_controller_vtable ||
             !output.action_button_controller_vtable ||
             !output.selected_help_presenter)) {
            erui::detail::logf(
                erui::LogLevel::error,
                "Runtime row presentation unavailable: one or more exact-profile vtables failed validation");
        }
    }

    if (require_native_back) {
        output.native_back = reinterpret_cast<NativeBackFn>(resolve_direct(
            game, "native Back", kNativeBackPattern,
            menu_runtime_profile
                ? menu_runtime_profile->native_back_rva
                : std::uintptr_t{0},
            kNativeBackPattern, kNativeBackHookablePrologue));
        if (!output.native_back) {
            return false;
        }
    } else if (require_custom_text) {
        // Alerts are optional, but a modal must be able to suppress the real
        // Back command. Missing alert-only interfaces do not disable menus.
        output.native_back = reinterpret_cast<NativeBackFn>(
            resolve_optional_direct(
                game, "native Back", kNativeBackPattern,
                menu_runtime_profile
                    ? menu_runtime_profile->native_back_rva
                    : std::uintptr_t{0},
                kNativeBackPattern, kNativeBackHookablePrologue));
    }

    if (require_custom_text || require_text_inputs ||
        require_row_presentations) {
        output.page_frame = reinterpret_cast<PageFrameFn>(
            resolve_optional_direct(
                game, "page frame", kPageFramePattern,
                menu_runtime_profile
                    ? menu_runtime_profile->page_frame_rva
                    : std::uintptr_t{0},
                kPageFramePattern));
    }

    if (!output.complete(
            require_rows,
            require_buttons,
            require_submenus,
            require_popup_choices,
            require_text_inputs,
            require_row_presentations,
            require_custom_text)) {
        erui::detail::logf(
            erui::LogLevel::error,
            "One or more native interfaces required by the enabled paths were unresolved; no hooks installed.");
        return false;
    }

    return true;
}

} // namespace erui::native
