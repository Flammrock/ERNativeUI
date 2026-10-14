#include "host_locale.hpp"
#include "localization_config.hpp"

#include <Windows.h>

#include <fstream>
#include <iterator>
#include <string>
#include <unordered_map>

namespace erui::host {
namespace {

constexpr std::uintmax_t kMaximumLocaleBytes = 64 * 1024;

std::string_view trim(std::string_view value) noexcept {
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) {
        value.remove_prefix(1);
    }
    while (!value.empty() &&
        (value.back() == ' ' || value.back() == '\t' || value.back() == '\r')) {
        value.remove_suffix(1);
    }
    return value;
}

bool utf8_to_utf16(std::string_view input, std::wstring& output) {
    if (input.empty() || input.size() > static_cast<std::size_t>(INT_MAX) ||
        input.find('\0') != std::string_view::npos) {
        return false;
    }
    const int count = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, input.data(),
        static_cast<int>(input.size()), nullptr, 0);
    if (count <= 0) return false;
    std::wstring converted(static_cast<std::size_t>(count), L'\0');
    if (MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS, input.data(),
            static_cast<int>(input.size()), converted.data(), count) != count ||
        converted.find(L'\0') != std::wstring::npos) {
        return false;
    }
    output = std::move(converted);
    return true;
}

} // namespace

HostLocale load_host_locale(
    const std::filesystem::path& dll_directory,
    std::string_view language_identifier) noexcept {
    HostLocale result{};
    if (!is_safe_locale_identifier(language_identifier)) return result;
    try {
        const std::filesystem::path path = dll_directory / L"locales" /
            (std::filesystem::path(language_identifier) += L".ini");
        std::error_code error{};
        const std::uintmax_t size = std::filesystem::file_size(path, error);
        if (error || size == 0 || size > kMaximumLocaleBytes) return result;

        std::ifstream stream(path, std::ios::binary);
        if (!stream) return result;
        std::string bytes{
            std::istreambuf_iterator<char>(stream),
            std::istreambuf_iterator<char>()};
        if (bytes.size() != static_cast<std::size_t>(size)) return result;
        if (bytes.size() >= 3 &&
            static_cast<unsigned char>(bytes[0]) == 0xEF &&
            static_cast<unsigned char>(bytes[1]) == 0xBB &&
            static_cast<unsigned char>(bytes[2]) == 0xBF) {
            bytes.erase(0, 3);
        }

        std::unordered_map<std::string, std::string_view> values{};
        std::string section{};
        std::size_t cursor = 0;
        while (cursor <= bytes.size()) {
            const std::size_t end = bytes.find('\n', cursor);
            std::string_view line(bytes.data() + cursor,
                (end == std::string::npos ? bytes.size() : end) - cursor);
            line = trim(line);
            if (!line.empty() && line.front() != ';' && line.front() != '#') {
                if (line.size() >= 2 && line.front() == '[' && line.back() == ']') {
                    section.assign(trim(line.substr(1, line.size() - 2)));
                } else if (const std::size_t equals = line.find('=');
                           equals != std::string_view::npos) {
                    const std::string_view key = trim(line.substr(0, equals));
                    const std::string_view value = trim(line.substr(equals + 1));
                    if (!section.empty() && !key.empty() && !value.empty()) {
                        values.insert_or_assign(section + "." + std::string(key), value);
                    }
                }
            }
            if (end == std::string::npos) break;
            cursor = end + 1;
        }

        const auto apply = [&](const char* key, std::wstring& destination) {
            const auto found = values.find(key);
            if (found != values.end()) {
                (void)utf8_to_utf16(found->second, destination);
            }
        };
        apply("Pagination.PreviousLabel", result.pagination.previous_label);
        apply("Pagination.PreviousHelp", result.pagination.previous_help);
        apply("Pagination.NextLabel", result.pagination.next_label);
        apply("Pagination.NextHelp", result.pagination.next_help);
        result.loaded = true;
    } catch (...) {
        return HostLocale{};
    }
    return result;
}

} // namespace erui::host
