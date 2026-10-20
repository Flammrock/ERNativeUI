#include "addresses.hpp"

#include "test_assertions.hpp"

#include <Windows.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace {

constexpr std::size_t kImageSize = 0x5E0DA00U;
constexpr std::uint32_t kTimestamp = 0x6A96B418U;
constexpr std::uintptr_t kGameOptionsRva = 0x959DF0U;
constexpr std::uintptr_t kSubHandlerRva = 0x95B770U;
constexpr std::uintptr_t kTextResolverRva = 0x764220U;
constexpr std::uintptr_t kSliderRefreshRva = 0x74FA20U;
constexpr std::uintptr_t kSelectorSelectRva = 0x739B90U;

class SyntheticGameImage {
public:
    SyntheticGameImage() noexcept {
        base_ = static_cast<std::uint8_t*>(VirtualAlloc(
            nullptr, kImageSize, MEM_RESERVE, PAGE_NOACCESS));
        if (!base_) return;
        if (!commit(0U)) return;
        if (!commit(kGameOptionsRva) || !commit(kSubHandlerRva) ||
            !commit(kTextResolverRva) || !commit(kSliderRefreshRva) ||
            !commit(kSelectorSelectRva)) {
            return;
        }

        auto* const dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base_);
        dos->e_magic = IMAGE_DOS_SIGNATURE;
        dos->e_lfanew = 0x100;
        auto* const nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(
            base_ + dos->e_lfanew);
        nt->Signature = IMAGE_NT_SIGNATURE;
        nt->FileHeader.Machine = IMAGE_FILE_MACHINE_AMD64;
        nt->FileHeader.NumberOfSections = 1;
        nt->FileHeader.TimeDateStamp = kTimestamp;
        nt->FileHeader.SizeOfOptionalHeader =
            sizeof(IMAGE_OPTIONAL_HEADER64);
        nt->OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR64_MAGIC;
        nt->OptionalHeader.SizeOfImage = static_cast<DWORD>(kImageSize);

        auto* const section = IMAGE_FIRST_SECTION(nt);
        std::memcpy(section->Name, ".text", 5U);
        section->VirtualAddress = 0x1000U;
        section->Misc.VirtualSize =
            static_cast<DWORD>(kImageSize - 0x1000U);
        section->Characteristics = IMAGE_SCN_MEM_EXECUTE |
            IMAGE_SCN_MEM_READ | IMAGE_SCN_CNT_CODE;
        ready_ = true;
    }

    ~SyntheticGameImage() {
        if (base_) VirtualFree(base_, 0U, MEM_RELEASE);
    }

    SyntheticGameImage(const SyntheticGameImage&) = delete;
    SyntheticGameImage& operator=(const SyntheticGameImage&) = delete;

    [[nodiscard]] bool ready() const noexcept { return ready_; }
    [[nodiscard]] std::uint8_t* base() const noexcept { return base_; }

private:
    [[nodiscard]] bool commit(std::uintptr_t rva) noexcept {
        constexpr std::uintptr_t page_mask = 0xFFFU;
        auto* const page = base_ + (rva & ~page_mask);
        return VirtualAlloc(
            page, 0x1000U, MEM_COMMIT, PAGE_READWRITE) == page;
    }

    std::uint8_t* base_{};
    bool ready_{};
};

void publish_pristine_entries(
    const erui::native::PreservedSharedMenuAddresses& preserved) {
    std::memcpy(
        reinterpret_cast<void*>(preserved.game_options_handler),
        preserved.game_options_pristine_prefix.data(),
        preserved.game_options_pristine_prefix.size());
    std::memcpy(
        reinterpret_cast<void*>(preserved.sub_handler),
        preserved.sub_pristine_prefix.data(),
        preserved.sub_pristine_prefix.size());
    std::memcpy(
        reinterpret_cast<void*>(preserved.text_resolver),
        preserved.text_pristine_prefix.data(),
        preserved.text_pristine_prefix.size());
    std::memcpy(
        reinterpret_cast<void*>(preserved.slider_refresh),
        preserved.slider_pristine_prefix.data(),
        preserved.slider_pristine_prefix.size());
    std::memcpy(
        preserved.selector_select,
        preserved.selector_select_pristine_prefix.data(),
        preserved.selector_select_pristine_prefix.size());
}

} // namespace

int main() {
    using erui::native::PreservedSharedMenuEntry;

    SyntheticGameImage image;
    ERUI_TEST_CHECK(image.ready());

    erui::native::ModuleView game;
    ERUI_TEST_CHECK(game.initialize(
        reinterpret_cast<HMODULE>(image.base())));

    erui::native::PreservedSharedMenuAddresses preserved{};
    ERUI_TEST_CHECK(
        erui::native::capture_preserved_shared_menu_addresses_at_attach(
            reinterpret_cast<HMODULE>(image.base()), preserved));
    publish_pristine_entries(preserved);
    preserved.game_options_approved_prefix =
        preserved.game_options_pristine_prefix;
    preserved.sub_approved_prefix = preserved.sub_pristine_prefix;
    preserved.text_approved_prefix = preserved.text_pristine_prefix;
    preserved.slider_approved_prefix = preserved.slider_pristine_prefix;
    preserved.selector_select_approved_prefix =
        preserved.selector_select_pristine_prefix;
    preserved.required_entries_approved = true;
    preserved.slider_entry_approved = true;
    preserved.selector_select_entry_approved = true;

    ERUI_TEST_CHECK(erui::native::revalidate_preserved_shared_menu_entry(
        game, preserved, PreservedSharedMenuEntry::game_options));
    ERUI_TEST_CHECK(erui::native::revalidate_preserved_shared_menu_entry(
        game, preserved, PreservedSharedMenuEntry::sub_handler));
    ERUI_TEST_CHECK(erui::native::revalidate_preserved_shared_menu_entry(
        game, preserved, PreservedSharedMenuEntry::text_resolver));
    ERUI_TEST_CHECK(erui::native::revalidate_preserved_shared_menu_entry(
        game, preserved, PreservedSharedMenuEntry::slider_refresh));
    ERUI_TEST_CHECK(erui::native::revalidate_preserved_shared_menu_entry(
        game, preserved, PreservedSharedMenuEntry::selector_select));
    ERUI_TEST_CHECK(erui::native::revalidate_preserved_shared_menu_addresses(
        game, preserved));

    preserved.selector_select_entry_approved = false;
    ERUI_TEST_CHECK(!erui::native::revalidate_preserved_shared_menu_entry(
        game, preserved, PreservedSharedMenuEntry::selector_select));
    preserved.selector_select_entry_approved = true;

    preserved.selector_select_approved_detour_target = image.base() + 0x6000U;
    ERUI_TEST_CHECK(!erui::native::revalidate_preserved_shared_menu_entry(
        game, preserved, PreservedSharedMenuEntry::selector_select));
    preserved.selector_select_approved_detour_target = nullptr;

    preserved.selector_select_pristine_prefix[0] ^= 0xFFU;
    ERUI_TEST_CHECK(!erui::native::revalidate_preserved_shared_menu_entry(
        game, preserved, PreservedSharedMenuEntry::selector_select));
    preserved.selector_select_pristine_prefix[0] ^= 0xFFU;

    auto* const game_options = reinterpret_cast<unsigned char*>(
        preserved.game_options_handler);
    const unsigned char original_game_options_byte = game_options[0];
    game_options[0] ^= 0xFFU;
    ERUI_TEST_CHECK(!erui::native::revalidate_preserved_shared_menu_entry(
        game, preserved, PreservedSharedMenuEntry::game_options));
    // Entry-specific validation must remain usable after another entry has
    // legitimately changed (for example, after ERNativeUI installed it).
    ERUI_TEST_CHECK(erui::native::revalidate_preserved_shared_menu_entry(
        game, preserved, PreservedSharedMenuEntry::sub_handler));
    ERUI_TEST_CHECK(erui::native::revalidate_preserved_shared_menu_entry(
        game, preserved, PreservedSharedMenuEntry::text_resolver));
    ERUI_TEST_CHECK(erui::native::revalidate_preserved_shared_menu_entry(
        game, preserved, PreservedSharedMenuEntry::slider_refresh));
    game_options[0] = original_game_options_byte;

    auto* const selector = static_cast<unsigned char*>(
        preserved.selector_select);
    const unsigned char original_selector_byte = selector[0];
    selector[0] ^= 0xFFU;
    ERUI_TEST_CHECK(!erui::native::revalidate_preserved_shared_menu_entry(
        game, preserved, PreservedSharedMenuEntry::selector_select));
    // Selector::select is a viewport-only seam. Its compatibility state must
    // not make the ordinary row-hook aggregate unavailable.
    ERUI_TEST_CHECK(erui::native::revalidate_preserved_shared_menu_addresses(
        game, preserved));
    selector[0] = original_selector_byte;

    auto* const slider = reinterpret_cast<unsigned char*>(
        preserved.slider_refresh);
    const unsigned char original_slider_byte = slider[0];
    slider[0] ^= 0xFFU;
    ERUI_TEST_CHECK(!erui::native::revalidate_preserved_shared_menu_entry(
        game, preserved, PreservedSharedMenuEntry::slider_refresh));
    ERUI_TEST_CHECK(erui::native::revalidate_preserved_shared_menu_entry(
        game, preserved, PreservedSharedMenuEntry::game_options));
    slider[0] = original_slider_byte;

    std::array<unsigned char,
        erui::native::PreservedSharedMenuAddresses::entry_prefix_size>
        approved_slider_detour{};
    approved_slider_detour[0] = 0xFFU;
    approved_slider_detour[1] = 0x25U;
    const std::int32_t slider_slot_displacement = 26;
    std::memcpy(
        approved_slider_detour.data() + 2U,
        &slider_slot_displacement,
        sizeof(slider_slot_displacement));
    std::memcpy(
        slider,
        approved_slider_detour.data(),
        approved_slider_detour.size());
    const void* approved_slider_target = image.base() + 0x4000U;
    std::memcpy(
        slider + 32U,
        &approved_slider_target,
        sizeof(approved_slider_target));
    preserved.slider_approved_prefix = approved_slider_detour;
    preserved.slider_approved_detour_target = approved_slider_target;
    ERUI_TEST_CHECK(erui::native::revalidate_preserved_shared_menu_entry(
        game, preserved, PreservedSharedMenuEntry::slider_refresh));

    const void* changed_slider_target = image.base() + 0x5000U;
    std::memcpy(
        slider + 32U,
        &changed_slider_target,
        sizeof(changed_slider_target));
    ERUI_TEST_CHECK(!erui::native::revalidate_preserved_shared_menu_entry(
        game, preserved, PreservedSharedMenuEntry::slider_refresh));

    auto* const sub = reinterpret_cast<unsigned char*>(preserved.sub_handler);
    std::array<unsigned char,
        erui::native::PreservedSharedMenuAddresses::entry_prefix_size>
        approved_detour{};
    approved_detour[0] = 0xFFU;
    approved_detour[1] = 0x25U;
    const std::int32_t slot_displacement = 26;
    std::memcpy(
        approved_detour.data() + 2U,
        &slot_displacement,
        sizeof(slot_displacement));
    std::memcpy(sub, approved_detour.data(), approved_detour.size());
    const void* approved_target = image.base() + 0x2000U;
    std::memcpy(sub + 32U, &approved_target, sizeof(approved_target));
    preserved.sub_approved_prefix = approved_detour;
    preserved.approved_detour_targets[1] = approved_target;
    ERUI_TEST_CHECK(erui::native::revalidate_preserved_shared_menu_entry(
        game, preserved, PreservedSharedMenuEntry::sub_handler));

    // The FF 25 instruction bytes can remain unchanged while another worker
    // rewrites its pointer slot. The separately approved target must catch it.
    const void* changed_target = image.base() + 0x3000U;
    std::memcpy(sub + 32U, &changed_target, sizeof(changed_target));
    ERUI_TEST_CHECK(!erui::native::revalidate_preserved_shared_menu_entry(
        game, preserved, PreservedSharedMenuEntry::sub_handler));

    std::array<unsigned char,
        erui::native::PreservedSharedMenuAddresses::entry_prefix_size>
        approved_selector_detour{};
    approved_selector_detour[0] = 0xFFU;
    approved_selector_detour[1] = 0x25U;
    const std::int32_t selector_slot_displacement = 26;
    std::memcpy(
        approved_selector_detour.data() + 2U,
        &selector_slot_displacement,
        sizeof(selector_slot_displacement));
    std::memcpy(
        selector,
        approved_selector_detour.data(),
        approved_selector_detour.size());
    const void* approved_selector_target = image.base() + 0x6000U;
    std::memcpy(
        selector + 32U,
        &approved_selector_target,
        sizeof(approved_selector_target));
    preserved.selector_select_approved_prefix = approved_selector_detour;
    preserved.selector_select_approved_detour_target =
        approved_selector_target;
    ERUI_TEST_CHECK(erui::native::revalidate_preserved_shared_menu_entry(
        game, preserved, PreservedSharedMenuEntry::selector_select));

    const void* changed_selector_target = image.base() + 0x7000U;
    std::memcpy(
        selector + 32U,
        &changed_selector_target,
        sizeof(changed_selector_target));
    ERUI_TEST_CHECK(!erui::native::revalidate_preserved_shared_menu_entry(
        game, preserved, PreservedSharedMenuEntry::selector_select));

    ERUI_TEST_CHECK(!erui::native::revalidate_preserved_shared_menu_entry(
        game,
        preserved,
        static_cast<PreservedSharedMenuEntry>(0xFFU)));
    return 0;
}
