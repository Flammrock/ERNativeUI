#pragma once

#include <ernativeui/erui.h>

#include "storage_document.hpp"

#include <Windows.h>

#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

namespace erui::host {

// ProviderStorage is the host-side adapter for the public storage ABI. It
// owns one in-memory document, but construction and destruction never touch
// the filesystem: clients must call load() and save() explicitly.
class ProviderStorage final {
public:
    [[nodiscard]] static ERUI_Result open(
        std::string_view provider_id,
        HMODULE host_module,
        HMODULE owner_module,
        const ERUI_StorageDesc* description,
        std::unique_ptr<ProviderStorage>& output) noexcept;

    ~ProviderStorage() = default;
    ProviderStorage(const ProviderStorage&) = delete;
    ProviderStorage& operator=(const ProviderStorage&) = delete;
    ProviderStorage(ProviderStorage&&) = delete;
    ProviderStorage& operator=(ProviderStorage&&) = delete;

    [[nodiscard]] ERUI_Result load() noexcept;
    [[nodiscard]] ERUI_Result save() noexcept;

    [[nodiscard]] ERUI_Result get_utf8(
        const ERUI_StorageKey* key,
        char* output,
        std::uint32_t output_capacity,
        std::uint32_t* out_length) const noexcept;
    [[nodiscard]] ERUI_Result set_utf8(
        const ERUI_StorageKey* key,
        const ERUI_StringView* value) noexcept;
    [[nodiscard]] ERUI_Result erase(
        const ERUI_StorageKey* key) noexcept;

    [[nodiscard]] ERUI_Result get_action_inputs(
        const ERUI_StorageKey* key,
        ERUI_ActionInputs* output) const noexcept;
    [[nodiscard]] ERUI_Result set_action_inputs(
        const ERUI_StorageKey* key,
        const ERUI_ActionInputs* value) noexcept;
    [[nodiscard]] ERUI_Result apply_assignment_changes(
        const ERUI_StringView* section,
        const ERUI_AssignmentChange* changes,
        std::uint32_t change_count) noexcept;

    [[nodiscard]] ERUI_Result get_info(ERUI_StorageInfo* output) const noexcept;

    [[nodiscard]] const std::filesystem::path& backing_path() const noexcept {
        return document_.path();
    }

private:
    explicit ProviderStorage(std::filesystem::path path)
        : document_(std::move(path)) {}

    // Serializes public ABI operations, including complete assignment-change
    // batches. StorageDocument remains independently thread-safe, while this
    // mutex prevents an observer from seeing an intermediate batch state.
    mutable std::mutex mutex_{};
    StorageDocument document_;
};

// Host-side std::string adapters around the public header-only ActionInputs
// codec. ProviderStorage uses these at its INI boundary; keeping the adapter
// thin ensures host storage and mod-owned storage share one grammar.
[[nodiscard]] ERUI_Result encode_action_inputs(
    const ERUI_ActionInputs& value,
    std::string& output) noexcept;
[[nodiscard]] ERUI_Result decode_action_inputs(
    std::string_view text,
    ERUI_ActionInputs& output) noexcept;

} // namespace erui::host
