#include <ernativeui/ERNativeUI.hpp>

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace {

#if defined(ERUI_API_VERSION_1_1)
void text_changed(const erui::TextInputChange&) noexcept {}

static_assert(sizeof(ERUI_TextInputDesc) == 96);
static_assert(offsetof(ERUI_TextInputDesc, maximum_length) == 88);
static_assert(std::is_trivially_copyable<ERUI_TextInputDesc>::value);
static_assert(std::is_same<
    decltype(erui::TextInputOptions{}.maximum_length),
    std::uint32_t>::value);
static_assert(noexcept(text_changed(std::declval<const erui::TextInputChange&>())));
static_assert(static_cast<ERUI_Capabilities>(erui::Capability::text_input) ==
    ERUI_CAP_TEXT_INPUT);
#endif

} // namespace

int main() {
    static_assert(ERUI_API_V1_0_SIZE == 128u);
    static_assert(std::is_trivially_copyable<ERUI_Api>::value);
#if defined(ERUI_API_VERSION_1_1)
    erui::TextInputOptions options{};
    if (options.maximum_length != ERUI_TEXT_INPUT_DEFAULT_MAX_LENGTH) return 1;
    if (ERUI_API_V1_1_SIZE < 152u) return 2;
#endif
    return 0;
}
