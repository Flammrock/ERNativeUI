#include "localization_config.hpp"

#include <algorithm>
#include <utility>

namespace erui::host {
namespace {

std::wstring_view trim_ascii_space(std::wstring_view value) noexcept {
    while (!value.empty() &&
        (value.front() == L' ' || value.front() == L'\t')) {
        value.remove_prefix(1);
    }
    while (!value.empty() &&
        (value.back() == L' ' || value.back() == L'\t')) {
        value.remove_suffix(1);
    }
    return value;
}

} // namespace

bool is_safe_locale_identifier(std::string_view identifier) noexcept {
    if (identifier.empty() || identifier.size() > 64) return false;
    return std::all_of(
        identifier.begin(), identifier.end(), [](unsigned char value) {
            return (value >= 'a' && value <= 'z') ||
                (value >= '0' && value <= '9') || value == '_' ||
                value == '-';
        });
}

LocalizationLanguageSetting parse_localization_language(
    std::wstring_view configured_value) noexcept {
    const std::wstring_view value = trim_ascii_space(configured_value);
    if (value == L"auto") {
        return {LocalizationLanguageMode::automatic, {}};
    }
    if (value.empty() || value.size() > 64) return {};

    try {
        std::string identifier{};
        identifier.reserve(value.size());
        for (const wchar_t code_unit : value) {
            if (code_unit > 0x7F) return {};
            identifier.push_back(static_cast<char>(code_unit));
        }
        if (!is_safe_locale_identifier(identifier)) return {};
        return {
            LocalizationLanguageMode::explicit_identifier,
            std::move(identifier)};
    } catch (...) {
        return {};
    }
}

} // namespace erui::host
