#pragma once

#include <cstdint>
#include <span>

namespace erui {

struct TitleScreenTextSpec;

}

namespace erui::native {

class ModuleView;

// Runtime GFX completion is optional. The host remains usable when the game
// build or native file interface is unknown; bundled/third-party GFX files
// then pass through unchanged and the existing native fallbacks still apply.
enum class NativeGfxLoaderInstallStatus : std::uint8_t {
    installed,
    already_installed,
    unsupported_game,
    interface_mismatch,
    allocation_failed,
    hook_failed,
};

enum class NativeTitleTextPublishStatus : std::uint8_t {
    published,
    already_published,
    unavailable,
    invalid_text,
    too_many_lines,
    allocation_failed,
};

// Frozen platform observation used only to choose the collision-safe title
// footer layout. `unknown` must retain the authored DLC row; it is never
// treated as evidence that the DLC label will be empty.
enum class NativeTitleDlcState : std::uint8_t {
    unknown,
    absent,
    installed,
};

// Internal compatibility observations for title fields created downstream of
// ERNativeUI's OpenFile transformation. Coordinates are local to the title
// movie's native Info sprite. The public title-text API deliberately does not
// expose this implementation detail.
enum class NativeTitleTextSide : std::uint8_t {
    left,
    right,
};

struct NativeTitleTextOccupancy {
    NativeTitleTextSide side{};
    std::int32_t y{};
};

// Native hook policy, supplied at installation so the exact first title open
// observes an immutable deadline. Zero selects immediate fail-open; larger
// values are capped to keep Scaleform's synchronous loader bounded.
inline constexpr std::uint32_t
    kNativeTitleCatalogueWaitDefaultMilliseconds = 2000U;
inline constexpr std::uint32_t
    kNativeTitleCatalogueWaitMaximumMilliseconds = 68000U;
static_assert(
    kNativeTitleCatalogueWaitDefaultMilliseconds <=
    kNativeTitleCatalogueWaitMaximumMilliseconds);

[[nodiscard]] NativeGfxLoaderInstallStatus install_native_gfx_loader(
    const ModuleView& game,
    std::uint16_t minimum_tab_slots = 8,
    std::uint32_t title_catalogue_wait_ms =
        kNativeTitleCatalogueWaitDefaultMilliseconds) noexcept;

[[nodiscard]] const char* native_gfx_loader_install_status_name(
    NativeGfxLoaderInstallStatus status) noexcept;

// Publishes the immutable, registry-ordered title-screen catalogue exactly
// once after provider registration freezes. A title movie that opens
// concurrently receives one bounded startup handoff; after that deadline the
// loader fails open and will not mutate an already-exposed movie. Text and
// provider identifiers are copied synchronously.
[[nodiscard]] NativeTitleTextPublishStatus publish_native_title_screen_texts(
    std::span<const TitleScreenTextSpec> lines,
    NativeTitleDlcState dlc_state,
    std::span<const NativeTitleTextOccupancy> external_occupancy = {}) noexcept;

// Settles a still-pending title catalogue as empty and wakes a first title
// open immediately. The host calls this on every startup stop/failure path so
// Scaleform never waits out the full lifecycle budget after publication has
// become impossible. It is harmless after a successful publication.
void abandon_native_title_screen_texts() noexcept;

[[nodiscard]] const char* native_title_text_publish_status_name(
    NativeTitleTextPublishStatus status) noexcept;

} // namespace erui::native
