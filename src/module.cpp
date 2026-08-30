#include "module.hpp"

#include <algorithm>
#include <charconv>
#include <cstring>
#include <string>
#include <system_error>

namespace erui::native {
namespace {

struct PatternByte {
    std::uint8_t value{};
    bool wildcard{};
};

std::vector<PatternByte> parse_pattern(std::string_view pattern) {
    std::vector<PatternByte> bytes;
    bytes.reserve(pattern.size() / 2);

    std::size_t cursor = 0;
    while (cursor < pattern.size()) {
        while (cursor < pattern.size() && pattern[cursor] == ' ') {
            ++cursor;
        }
        if (cursor >= pattern.size()) {
            break;
        }

        const std::size_t token_begin = cursor;
        while (cursor < pattern.size() && pattern[cursor] != ' ') {
            ++cursor;
        }
        const auto token = pattern.substr(token_begin, cursor - token_begin);
        if (token == "?" || token == "??") {
            bytes.push_back(PatternByte{0, true});
            continue;
        }

        unsigned value = 0;
        const char* first = token.data();
        const char* last = token.data() + token.size();
        const auto parsed = std::from_chars(first, last, value, 16);
        if (parsed.ec != std::errc{} || parsed.ptr != last || value > 0xFFu) {
            return {};
        }
        bytes.push_back(PatternByte{static_cast<std::uint8_t>(value), false});
    }

    return bytes;
}

bool guarded_matches(
    const std::uint8_t* address,
    const std::vector<PatternByte>& pattern) noexcept {
#if defined(_MSC_VER)
    __try {
#endif
        for (std::size_t i = 0; i < pattern.size(); ++i) {
            if (!pattern[i].wildcard && address[i] != pattern[i].value) {
                return false;
            }
        }
        return true;
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#endif
}

} // namespace

bool ModuleView::initialize(HMODULE module) noexcept {
    sections_.clear();
    base_ = reinterpret_cast<std::uint8_t*>(module);
    image_size_ = 0;
    timestamp_ = 0;
    if (!base_) {
        return false;
    }

    // The main module handle is expected to point at a mapped PE image. Keep
    // every derived range inside SizeOfImage before exposing it to the scanner.
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base_);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 ||
        static_cast<std::uint64_t>(dos->e_lfanew) > 0x100000ull) {
        base_ = nullptr;
        return false;
    }

    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base_ + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
        nt->OptionalHeader.SizeOfImage < 0x1000) {
        base_ = nullptr;
        return false;
    }

    image_size_ = nt->OptionalHeader.SizeOfImage;
    timestamp_ = nt->FileHeader.TimeDateStamp;

    const auto* image_end = base_ + image_size_;
    const IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(nt);
    const auto* section_bytes = reinterpret_cast<const std::uint8_t*>(section);
    const std::size_t section_table_size =
        static_cast<std::size_t>(nt->FileHeader.NumberOfSections) * sizeof(IMAGE_SECTION_HEADER);
    if (section_bytes < base_ || section_bytes > image_end ||
        section_table_size > static_cast<std::size_t>(image_end - section_bytes)) {
        sections_.clear();
        base_ = nullptr;
        image_size_ = 0;
        timestamp_ = 0;
        return false;
    }

    try {
        sections_.reserve(nt->FileHeader.NumberOfSections);
        for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
            const std::size_t virtual_address = section[i].VirtualAddress;
            if (virtual_address >= image_size_) {
                continue;
            }

            const std::size_t declared_size = std::max<std::size_t>(
                section[i].Misc.VirtualSize,
                section[i].SizeOfRawData);
            const std::size_t bounded_size = std::min(
                declared_size,
                image_size_ - virtual_address);
            if (bounded_size == 0) {
                continue;
            }

            ModuleSection view{};
            view.begin = base_ + virtual_address;
            view.size = bounded_size;
            view.executable = (section[i].Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0;
            const std::size_t name_length = std::min<std::size_t>(8, strnlen_s(
                reinterpret_cast<const char*>(section[i].Name), 8));
            std::memcpy(view.name, section[i].Name, name_length);
            view.name[name_length] = '\0';
            sections_.push_back(view);
        }
    } catch (...) {
        sections_.clear();
        base_ = nullptr;
        image_size_ = 0;
        timestamp_ = 0;
        return false;
    }

    return !sections_.empty();
}

bool ModuleView::contains(const void* address, std::size_t size) const noexcept {
    if (!base_ || !address || size == 0) {
        return false;
    }
    const auto start = reinterpret_cast<std::uintptr_t>(address);
    const auto image_start = reinterpret_cast<std::uintptr_t>(base_);
    const auto image_end = image_start + image_size_;
    if (start < image_start || start >= image_end) {
        return false;
    }
    return size <= image_end - start;
}

bool ModuleView::is_executable(const void* address) const noexcept {
    const auto value = reinterpret_cast<std::uintptr_t>(address);
    for (const auto& section : sections_) {
        if (!section.executable) {
            continue;
        }
        const auto begin = reinterpret_cast<std::uintptr_t>(section.begin);
        const auto end = begin + section.size;
        if (value >= begin && value < end) {
            return true;
        }
    }
    return false;
}

ScanResult ModuleView::scan_executable(std::string_view text_pattern) const {
    const auto pattern = parse_pattern(text_pattern);
    if (pattern.empty()) {
        return {};
    }

    ScanResult result{};
    for (const auto& section : sections_) {
        if (!section.executable || section.size < pattern.size()) {
            continue;
        }

        const std::size_t last = section.size - pattern.size();
        for (std::size_t offset = 0; offset <= last; ++offset) {
            auto* candidate = section.begin + offset;
            if (guarded_matches(candidate, pattern)) {
                if (!result.address) {
                    result.address = candidate;
                }
                ++result.matches;
                if (result.matches >= 2) {
                    return result;
                }
            }
        }
    }
    return result;
}

std::vector<std::uint8_t*> ModuleView::scan_executable_all(
    std::string_view text_pattern) const {
    const auto pattern = parse_pattern(text_pattern);
    std::vector<std::uint8_t*> matches;
    if (pattern.empty()) return matches;
    for (const auto& section : sections_) {
        if (!section.executable || section.size < pattern.size()) continue;
        const std::size_t last = section.size - pattern.size();
        for (std::size_t offset = 0; offset <= last; ++offset) {
            auto* const candidate = section.begin + offset;
            if (guarded_matches(candidate, pattern)) {
                matches.push_back(candidate);
            }
        }
    }
    return matches;
}

bool ModuleView::matches(const void* address, std::string_view text_pattern) const noexcept {
    const auto pattern = parse_pattern(text_pattern);
    if (pattern.empty() || !contains(address, pattern.size())) {
        return false;
    }
    return guarded_matches(reinterpret_cast<const std::uint8_t*>(address), pattern);
}

std::uint8_t* ModuleView::at_rva(std::uintptr_t rva) const noexcept {
    if (!base_ || rva >= image_size_) {
        return nullptr;
    }
    return base_ + rva;
}

std::uint8_t* resolve_rel32_call(
    const ModuleView& module,
    std::uint8_t* instruction) noexcept {
    if (!instruction || !module.contains(instruction, 5) || instruction[0] != 0xE8) {
        return nullptr;
    }

    std::int32_t displacement = 0;
    std::memcpy(&displacement, instruction + 1, sizeof(displacement));
    auto* target = instruction + 5 + displacement;
    return module.is_executable(target) ? target : nullptr;
}


std::uint8_t* resolve_rip_relative(
    const ModuleView& module,
    std::uint8_t* instruction,
    std::size_t displacement_offset,
    std::size_t instruction_size) noexcept {
    if (!instruction || instruction_size < displacement_offset + sizeof(std::int32_t) ||
        !module.contains(instruction, instruction_size)) {
        return nullptr;
    }

    std::int32_t displacement = 0;
    std::memcpy(
        &displacement,
        instruction + displacement_offset,
        sizeof(displacement));
    auto* target = instruction + instruction_size + displacement;
    return module.contains(target) ? target : nullptr;
}

} // namespace erui::native
