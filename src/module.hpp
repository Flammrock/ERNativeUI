#pragma once

#include <Windows.h>

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace erui::native {

struct ScanResult {
    std::uint8_t* address{};
    std::size_t matches{};
};

struct ModuleSection {
    std::uint8_t* begin{};
    std::size_t size{};
    bool executable{};
    char name[9]{};
};

class ModuleView {
public:
    bool initialize(HMODULE module) noexcept;

    [[nodiscard]] std::uint8_t* base() const noexcept { return base_; }
    [[nodiscard]] std::size_t image_size() const noexcept { return image_size_; }
    [[nodiscard]] std::uint32_t timestamp() const noexcept { return timestamp_; }
    [[nodiscard]] const std::vector<ModuleSection>& sections() const noexcept { return sections_; }

    [[nodiscard]] bool contains(const void* address, std::size_t size = 1) const noexcept;
    [[nodiscard]] bool is_executable(const void* address) const noexcept;
    [[nodiscard]] ScanResult scan_executable(std::string_view pattern) const;
    [[nodiscard]] std::vector<std::uint8_t*> scan_executable_all(
        std::string_view pattern) const;
    [[nodiscard]] bool matches(const void* address, std::string_view pattern) const noexcept;
    [[nodiscard]] std::uint8_t* at_rva(std::uintptr_t rva) const noexcept;

private:
    std::uint8_t* base_{};
    std::size_t image_size_{};
    std::uint32_t timestamp_{};
    std::vector<ModuleSection> sections_{};
};

[[nodiscard]] std::uint8_t* resolve_rel32_call(
    const ModuleView& module,
    std::uint8_t* instruction) noexcept;

[[nodiscard]] std::uint8_t* resolve_rip_relative(
    const ModuleView& module,
    std::uint8_t* instruction,
    std::size_t displacement_offset,
    std::size_t instruction_size) noexcept;

} // namespace erui::native
