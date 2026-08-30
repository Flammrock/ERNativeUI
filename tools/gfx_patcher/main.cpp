#include "gfx_patch.hpp"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <span>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <cwctype>

#ifndef ER_NATIVE_UI_GFX_PATCHER_VERSION
#define ER_NATIVE_UI_GFX_PATCHER_VERSION "dev"
#endif

namespace {

[[nodiscard]] std::wstring widen_ascii(std::string_view text) {
    return std::wstring(text.begin(), text.end());
}

struct Arguments {
    std::filesystem::path input{};
    std::filesystem::path output{};
    std::uint16_t controller_rows{7};
    bool inspect_only{};
    bool overwrite{};
};

void print_usage() {
    std::wcout
        << L"Inspect:\n"
        << L"  ERNativeUIGfxPatcher.exe --inspect --input <02_040_optionsetting.gfx>\n\n"
        << L"Patch the user's own PC GFX:\n"
        << L"  ERNativeUIGfxPatcher.exe --input <source.gfx> --output <patched.gfx> "
           L"--controller-rows 13 [--overwrite]\n\n"
        << L"The input file is never modified. The target row count may be 6..13.\n";
}

[[nodiscard]] std::optional<std::uint16_t> parse_u16(std::wstring_view text) {
    std::string narrow;
    narrow.reserve(text.size());
    for (const wchar_t character : text) {
        if (character < L'0' || character > L'9') {
            return std::nullopt;
        }
        narrow.push_back(static_cast<char>(character));
    }

    std::uint16_t value{};
    const auto result = std::from_chars(
        narrow.data(), narrow.data() + narrow.size(), value);
    if (result.ec != std::errc{} || result.ptr != narrow.data() + narrow.size()) {
        return std::nullopt;
    }
    return value;
}

[[nodiscard]] bool parse_arguments(
    const std::vector<std::wstring>& arguments,
    Arguments& parsed,
    std::wstring& error) {
    for (std::size_t index = 1; index < arguments.size(); ++index) {
        const std::wstring_view token = arguments[index];
        if (token == L"--help" || token == L"-h" || token == L"/?") {
            print_usage();
            return false;
        }
        if (token == L"--inspect") {
            parsed.inspect_only = true;
            continue;
        }
        if (token == L"--overwrite") {
            parsed.overwrite = true;
            continue;
        }
        if (token == L"--input" || token == L"--output" ||
            token == L"--controller-rows") {
            if (index + 1 >= arguments.size()) {
                error = L"missing value after " + std::wstring(token);
                return false;
            }
            const std::wstring& value = arguments[++index];
            if (token == L"--input") {
                parsed.input = value;
            } else if (token == L"--output") {
                parsed.output = value;
            } else {
                const std::optional<std::uint16_t> rows = parse_u16(value);
                if (!rows) {
                    error = L"--controller-rows requires an integer";
                    return false;
                }
                parsed.controller_rows = *rows;
            }
            continue;
        }
        error = L"unknown argument: " + std::wstring(token);
        return false;
    }

    if (parsed.input.empty()) {
        error = L"--input is required";
        return false;
    }
    if (!parsed.inspect_only && parsed.output.empty()) {
        error = L"--output is required unless --inspect is used";
        return false;
    }
    return true;
}

[[nodiscard]] bool read_file(
    const std::filesystem::path& path,
    std::vector<std::uint8_t>& bytes,
    std::wstring& error) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        error = L"could not open input file: " + path.wstring();
        return false;
    }
    stream.seekg(0, std::ios::end);
    const std::streamoff size = stream.tellg();
    if (size <= 0) {
        error = L"input file is empty or unreadable";
        return false;
    }
    stream.seekg(0, std::ios::beg);
    bytes.resize(static_cast<std::size_t>(size));
    if (!stream.read(
            reinterpret_cast<char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()))) {
        error = L"could not read the complete input file";
        return false;
    }
    return true;
}

[[nodiscard]] bool same_path(
    const std::filesystem::path& left,
    const std::filesystem::path& right) {
    std::error_code error{};
    const auto normalized_left = std::filesystem::absolute(left, error).lexically_normal();
    error.clear();
    const auto normalized_right = std::filesystem::absolute(right, error).lexically_normal();
#ifdef _WIN32
    std::wstring left_text = normalized_left.wstring();
    std::wstring right_text = normalized_right.wstring();
    const auto lower = [](wchar_t value) {
        return static_cast<wchar_t>(std::towlower(value));
    };
    std::transform(left_text.begin(), left_text.end(), left_text.begin(), lower);
    std::transform(right_text.begin(), right_text.end(), right_text.begin(), lower);
    return left_text == right_text;
#else
    return normalized_left == normalized_right;
#endif
}

[[nodiscard]] bool write_file_atomically(
    const std::filesystem::path& output,
    std::span<const std::uint8_t> bytes,
    bool overwrite,
    std::wstring& error) {
    std::error_code filesystem_error{};
    if (std::filesystem::exists(output, filesystem_error) && !overwrite) {
        error = L"output already exists; pass --overwrite to replace it";
        return false;
    }
    if (!output.parent_path().empty()) {
        std::filesystem::create_directories(output.parent_path(), filesystem_error);
        if (filesystem_error) {
            error = L"could not create output directory: " +
                output.parent_path().wstring();
            return false;
        }
    }

    std::filesystem::path temporary = output;
    temporary += L".erui.tmp";
    std::filesystem::remove(temporary, filesystem_error);
    filesystem_error.clear();

    {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        if (!stream || !stream.write(
                reinterpret_cast<const char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()))) {
            error = L"could not write temporary output file";
            std::filesystem::remove(temporary, filesystem_error);
            return false;
        }
    }

    if (overwrite) {
        std::filesystem::remove(output, filesystem_error);
        filesystem_error.clear();
    }
    std::filesystem::rename(temporary, output, filesystem_error);
    if (filesystem_error) {
        error = L"could not move the completed temporary file into place";
        std::filesystem::remove(temporary, filesystem_error);
        return false;
    }
    return true;
}

void print_inspection(const erui::gfx::Inspection& inspection) {
    std::wcout
        << L"GFX version:                 " << static_cast<unsigned>(inspection.gfx_version) << L'\n'
        << L"Declared movie length:       " << inspection.declared_length << L" bytes\n"
        << L"WindowList sprite ID:         " << inspection.window_list_sprite << L'\n'
        << L"ControllSetting sprite ID:    " << inspection.controller_sprite << L'\n'
        << L"Generic item character ID:    " << inspection.controller_item_character << L'\n'
        << L"Controller visual row slots:  " << inspection.controller_rows << L'\n';
}

int run(const std::vector<std::wstring>& command_line) {
    std::wcout << L"ERNativeUIGfxPatcher v"
               << widen_ascii(ER_NATIVE_UI_GFX_PATCHER_VERSION) << L"\n";

    Arguments arguments{};
    std::wstring error{};
    if (!parse_arguments(command_line, arguments, error)) {
        if (!error.empty()) {
            std::wcerr << L"ERROR: " << error << L"\n\n";
            print_usage();
            return 2;
        }
        return 0;
    }

    std::vector<std::uint8_t> input{};
    if (!read_file(arguments.input, input, error)) {
        std::wcerr << L"ERROR: " << error << L'\n';
        return 3;
    }

    const erui::gfx::Inspection inspection =
        erui::gfx::inspect_controller_panel(input);
    if (!inspection.success()) {
        std::wcerr << L"ERROR [" << widen_ascii(erui::gfx::error_name(inspection.error))
                   << L"]: " << widen_ascii(inspection.message) << L'\n';
        return 4;
    }

    std::wcout << L"Input: " << arguments.input.wstring() << L'\n';
    print_inspection(inspection);
    if (arguments.inspect_only) {
        return 0;
    }

    if (same_path(arguments.input, arguments.output)) {
        std::wcerr << L"ERROR: input and output must be different paths; the patcher never modifies the source file.\n";
        return 2;
    }

    const erui::gfx::PatchResult patch = erui::gfx::patch_controller_panel(
        input,
        {.controller_rows = arguments.controller_rows});
    if (!patch.success()) {
        std::wcerr << L"ERROR [" << widen_ascii(erui::gfx::error_name(patch.error))
                   << L"]: " << widen_ascii(patch.message) << L'\n';
        return 5;
    }

    if (!write_file_atomically(
            arguments.output,
            patch.output,
            arguments.overwrite,
            error)) {
        std::wcerr << L"ERROR: " << error << L'\n';
        return 6;
    }

    std::wcout
        << L"Controller rows:             " << patch.report.before.controller_rows
        << L" -> " << patch.report.after.controller_rows << L'\n'
        << L"Bytes added:                 " << patch.report.bytes_added << L'\n'
        << L"Status:                      "
        << (patch.report.already_satisfied ? L"already satisfied" : L"patched and verified")
        << L'\n';
    std::wcout
        << L"Output: " << arguments.output.wstring() << L"\n\n"
        << L"Install the output as:\n"
        << L"  <ModEngine2 mod path>\\menu\\win\\02_040_optionsetting.gfx\n";
    return 0;
}

} // namespace

#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
    std::vector<std::wstring> arguments{};
    arguments.reserve(static_cast<std::size_t>(argc));
    for (int index = 0; index < argc; ++index) {
        arguments.emplace_back(argv[index]);
    }
    return run(arguments);
}
#else
int main(int argc, char** argv) {
    std::vector<std::wstring> arguments{};
    arguments.reserve(static_cast<std::size_t>(argc));
    for (int index = 0; index < argc; ++index) {
        arguments.push_back(std::filesystem::path(argv[index]).wstring());
    }
    return run(arguments);
}
#endif
