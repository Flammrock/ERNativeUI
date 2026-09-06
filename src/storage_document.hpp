#pragma once

#include <ernativeui/erui.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace erui::host {

// Storage is deliberately bounded before it reaches either the parser or the
// serializer. Section and key names use a conservative, unambiguous INI
// grammar: one or more ASCII letters/digits, '.', '_', '-', or internal spaces.
// Leading/trailing spaces, the special names "." and "..", and every other
// byte are rejected. Names are identifiers, never paths.
//
// Values are valid UTF-8 byte strings stored exactly after the first '=' on a
// line. Empty values, spaces, '=', '#', and ';' are preserved; NUL, CR, and LF
// are rejected so a value can never create another INI record.
inline constexpr std::size_t kStorageMaximumFileBytes =
    ERUI_STORAGE_MAX_FILE_BYTES;
inline constexpr std::size_t kStorageMaximumDecodedBytes =
    ERUI_STORAGE_MAX_DECODED_BYTES;
inline constexpr std::size_t kStorageMaximumSections =
    ERUI_STORAGE_MAX_SECTIONS;
inline constexpr std::size_t kStorageMaximumEntries =
    ERUI_STORAGE_MAX_ENTRIES;
inline constexpr std::size_t kStorageMaximumSectionBytes =
    ERUI_STORAGE_MAX_SECTION_BYTES;
inline constexpr std::size_t kStorageMaximumKeyBytes =
    ERUI_STORAGE_MAX_KEY_BYTES;
inline constexpr std::size_t kStorageMaximumValueBytes =
    ERUI_STORAGE_MAX_VALUE_BYTES;

enum class StorageDocumentResult : std::uint8_t {
    ok,
    invalid_argument,
    not_loaded,
    io_error,
    format_error,
    limit_exceeded,
    out_of_memory,
};

enum class StorageMutationKind : std::uint8_t {
    set,
    erase,
};

// One operation in an in-memory transaction. Strings are owned so callers can
// finish all of their validation and encoding before handing a batch to the
// document. For erase operations, value is ignored.
struct StorageMutation final {
    StorageMutationKind kind{StorageMutationKind::set};
    std::string key{};
    std::string value{};
};

// A thread-safe, explicitly persisted UTF-8 INI document. Construction and
// destruction never touch the filesystem. load() is the sole read operation;
// save() is the sole write operation.
class StorageDocument final {
public:
    explicit StorageDocument(std::filesystem::path path);
    ~StorageDocument() = default;

    StorageDocument(const StorageDocument&) = delete;
    StorageDocument& operator=(const StorageDocument&) = delete;
    StorageDocument(StorageDocument&&) = delete;
    StorageDocument& operator=(StorageDocument&&) = delete;

    [[nodiscard]] StorageDocumentResult load() noexcept;
    [[nodiscard]] StorageDocumentResult save() noexcept;

    // On success, found distinguishes a missing key from a stored empty value.
    // output is changed only when found is true.
    [[nodiscard]] StorageDocumentResult get(
        std::string_view section,
        std::string_view key,
        std::string& output,
        bool& found) const noexcept;

    // Setting an identical value and erasing a missing value are successful
    // no-ops: neither operation advances the revision nor marks the document
    // dirty. erase() writes its optional result only on success.
    [[nodiscard]] StorageDocumentResult set(
        std::string_view section,
        std::string_view key,
        std::string_view value) noexcept;
    [[nodiscard]] StorageDocumentResult erase(
        std::string_view section,
        std::string_view key,
        bool* erased = nullptr) noexcept;

    // Applies the complete batch as one transaction. Every operation is
    // validated and performed against a private copy before that copy is
    // committed. A failure leaves the values, revision, dirty flag, and entry
    // accounting unchanged. A successful batch advances the revision exactly
    // once when (and only when) its final state differs from the current one.
    // Duplicate keys use ordered/last-operation-wins semantics.
    [[nodiscard]] StorageDocumentResult apply_batch(
        std::string_view section,
        const std::vector<StorageMutation>& mutations) noexcept;

    [[nodiscard]] bool is_loaded() const noexcept;
    [[nodiscard]] bool is_dirty() const noexcept;
    [[nodiscard]] std::uint64_t current_revision() const noexcept;
    [[nodiscard]] std::uint64_t last_saved_revision() const noexcept;
    [[nodiscard]] std::size_t entry_count() const noexcept;
    [[nodiscard]] const std::filesystem::path& path() const noexcept {
        return path_;
    }

private:
    using Values = std::map<std::string, std::string, std::less<>>;
    using Sections = std::map<std::string, Values, std::less<>>;

    static void advance_revision(std::uint64_t& revision) noexcept;

    const std::filesystem::path path_;
    mutable std::mutex mutex_{};
    std::mutex io_mutex_{};
    Sections sections_{};
    std::size_t entry_count_{};
    std::size_t decoded_bytes_{};
    std::uint64_t current_revision_{};
    std::uint64_t last_saved_revision_{};
    bool loaded_{};
    bool dirty_{};
};

} // namespace erui::host
