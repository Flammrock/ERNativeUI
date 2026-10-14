#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace erui::host {

enum class LocalizationLanguageMode : std::uint8_t {
    automatic,
    explicit_identifier,
    invalid,
};

struct LocalizationLanguageSetting {
    LocalizationLanguageMode mode{LocalizationLanguageMode::invalid};
    std::string identifier{};
};

// Locale identifiers are also used as filenames below the fixed locales
// directory. Keep this policy deliberately narrower than a general filename.
[[nodiscard]] bool is_safe_locale_identifier(
    std::string_view identifier) noexcept;

// Parses [Localization] Language. "auto" selects Steam discovery; every
// explicit identifier is retained exactly so unknown future/community locales
// can flow through the public language API without an enum update.
[[nodiscard]] LocalizationLanguageSetting parse_localization_language(
    std::wstring_view configured_value) noexcept;

} // namespace erui::host
