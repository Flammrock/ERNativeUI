#include <ernativeui/ERNativeUI.hpp>

#include <array>
#include "test_assertions.hpp"
#include <cstdint>
#include <string_view>
#include <type_traits>

namespace {

ERUI_Result ERUI_CALL format_with_user_data(
    void*,
    const erui::PageTitleFormatContext*,
    std::uint16_t* output,
    std::uint32_t output_capacity,
    std::uint32_t* out_length) noexcept {
    return erui::write_page_title(
        L"With context", output, output_capacity, out_length);
}

ERUI_Result ERUI_CALL format_without_user_data(
    const erui::PageTitleFormatContext*,
    std::uint16_t* output,
    std::uint32_t output_capacity,
    std::uint32_t* out_length) noexcept {
    return erui::write_page_title(
        L"Static formatter", output, output_capacity, out_length);
}

static_assert(std::is_same<
    decltype(&format_with_user_data), erui::PageTitleFormatter>::value);
static_assert(std::is_same<
    decltype(&format_without_user_data),
    erui::StaticPageTitleFormatter>::value);

} // namespace

int main() {
    static_assert(__cplusplus >= 201703L);

    erui::PagePresentation presentation{};
    presentation.menu_title = L"Compile Test";
    presentation.page_title = L"Logical Page";
    presentation.formatter = &format_with_user_data;

    // A default Page has no draft, so these calls are harmless while still
    // compiling both public overloads exactly as a client mod uses them.
    erui::Page page{};
    page.set_presentation(presentation);
    page.set_presentation<&format_without_user_data>(
        L"Compile Test", L"Logical Page");

    std::array<std::uint16_t, 32> output{};
    std::uint32_t length{};
    ERUI_TEST_CHECK(erui::write_page_title(
        L"Cached title", output.data(),
        static_cast<std::uint32_t>(output.size()), &length) == ERUI_OK);
    ERUI_TEST_CHECK(length == std::wstring_view(L"Cached title").size());
    ERUI_TEST_CHECK(output[0] == static_cast<std::uint16_t>(L'C'));
    return 0;
}
