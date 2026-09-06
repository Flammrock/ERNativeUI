#include "storage_document.hpp"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <limits>
#include <utility>
#include <vector>

namespace erui::host {
namespace {

using Values = std::map<std::string, std::string, std::less<>>;
using Sections = std::map<std::string, Values, std::less<>>;

constexpr std::string_view kGeneratedHeader =
    "; ERNativeUI provider storage\r\n";

struct ParsedDocument {
    Sections sections{};
    std::size_t entry_count{};
    std::size_t decoded_bytes{};
};

bool valid_utf8(std::string_view value) noexcept {
    const auto* bytes = reinterpret_cast<const unsigned char*>(value.data());
    std::size_t index = 0;
    while (index < value.size()) {
        const unsigned char first = bytes[index++];
        if (first <= 0x7F) continue;

        std::size_t continuation_count = 0;
        unsigned char second_min = 0x80;
        unsigned char second_max = 0xBF;
        if (first >= 0xC2 && first <= 0xDF) {
            continuation_count = 1;
        } else if (first == 0xE0) {
            continuation_count = 2;
            second_min = 0xA0;
        } else if (first >= 0xE1 && first <= 0xEC) {
            continuation_count = 2;
        } else if (first == 0xED) {
            continuation_count = 2;
            second_max = 0x9F;
        } else if (first >= 0xEE && first <= 0xEF) {
            continuation_count = 2;
        } else if (first == 0xF0) {
            continuation_count = 3;
            second_min = 0x90;
        } else if (first >= 0xF1 && first <= 0xF3) {
            continuation_count = 3;
        } else if (first == 0xF4) {
            continuation_count = 3;
            second_max = 0x8F;
        } else {
            return false;
        }

        if (value.size() - index < continuation_count) return false;
        const unsigned char second = bytes[index];
        if (second < second_min || second > second_max) return false;
        ++index;
        for (std::size_t remaining = 1;
             remaining < continuation_count;
             ++remaining, ++index) {
            if (bytes[index] < 0x80 || bytes[index] > 0xBF) return false;
        }
    }
    return true;
}

bool identifier_byte(unsigned char value) noexcept {
    return (value >= 'a' && value <= 'z') ||
        (value >= 'A' && value <= 'Z') ||
        (value >= '0' && value <= '9') ||
        value == '.' || value == '_' || value == '-' || value == ' ';
}

StorageDocumentResult validate_identifier(
    std::string_view value,
    std::size_t maximum_bytes) noexcept {
    if (value.size() > maximum_bytes) {
        return StorageDocumentResult::limit_exceeded;
    }
    if (value.empty() || value.front() == ' ' || value.back() == ' ' ||
        value == "." || value == "..") {
        return StorageDocumentResult::invalid_argument;
    }
    for (const unsigned char byte : value) {
        if (!identifier_byte(byte)) {
            return StorageDocumentResult::invalid_argument;
        }
    }
    return StorageDocumentResult::ok;
}

StorageDocumentResult validate_value(
    std::string_view value,
    std::size_t maximum_bytes) noexcept {
    if (value.size() > maximum_bytes) {
        return StorageDocumentResult::limit_exceeded;
    }
    if (!valid_utf8(value) ||
        value.find_first_of(std::string_view{"\0\r\n", 3}) !=
            std::string_view::npos) {
        return StorageDocumentResult::invalid_argument;
    }
    return StorageDocumentResult::ok;
}

std::string_view trim_horizontal(std::string_view value) noexcept {
    while (!value.empty() &&
           (value.front() == ' ' || value.front() == '\t')) {
        value.remove_prefix(1);
    }
    while (!value.empty() &&
           (value.back() == ' ' || value.back() == '\t')) {
        value.remove_suffix(1);
    }
    return value;
}

StorageDocumentResult parse_document(
    std::string_view bytes,
    ParsedDocument& output) {
    if (bytes.size() > kStorageMaximumFileBytes) {
        return StorageDocumentResult::limit_exceeded;
    }
    if (bytes.size() >= 3 &&
        static_cast<unsigned char>(bytes[0]) == 0xEF &&
        static_cast<unsigned char>(bytes[1]) == 0xBB &&
        static_cast<unsigned char>(bytes[2]) == 0xBF) {
        bytes.remove_prefix(3);
    }
    if (!valid_utf8(bytes) || bytes.find('\0') != std::string_view::npos) {
        return StorageDocumentResult::format_error;
    }

    Values* current_values = nullptr;
    ParsedDocument parsed{};

    std::size_t cursor = 0;
    while (cursor <= bytes.size()) {
        const std::size_t newline = bytes.find('\n', cursor);
        std::string_view line = newline == std::string_view::npos
            ? bytes.substr(cursor)
            : bytes.substr(cursor, newline - cursor);
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        if (line.find('\r') != std::string_view::npos) {
            return StorageDocumentResult::format_error;
        }
        const std::string_view syntax = trim_horizontal(line);

        if (!syntax.empty() && syntax.front() != ';' &&
            syntax.front() != '#') {
            if (syntax.front() == '[') {
                if (syntax.size() < 2 || syntax.back() != ']') {
                    return StorageDocumentResult::format_error;
                }
                const std::string_view section = trim_horizontal(
                    syntax.substr(1, syntax.size() - 2));
                const StorageDocumentResult validated = validate_identifier(
                    section, kStorageMaximumSectionBytes);
                if (validated == StorageDocumentResult::limit_exceeded) {
                    return validated;
                }
                if (validated != StorageDocumentResult::ok) {
                    return StorageDocumentResult::format_error;
                }
                if (parsed.sections.size() >= kStorageMaximumSections) {
                    return StorageDocumentResult::limit_exceeded;
                }
                auto [position, inserted] =
                    parsed.sections.emplace(std::string(section), Values{});
                if (!inserted) return StorageDocumentResult::format_error;
                if (parsed.decoded_bytes >
                    kStorageMaximumDecodedBytes - position->first.size()) {
                    return StorageDocumentResult::limit_exceeded;
                }
                parsed.decoded_bytes += position->first.size();
                current_values = &position->second;
            } else if (current_values) {
                const std::size_t equals = line.find('=');
                if (equals == std::string_view::npos) {
                    return StorageDocumentResult::format_error;
                }
                const std::string_view key =
                    trim_horizontal(line.substr(0, equals));
                const std::string_view value = line.substr(equals + 1);
                const StorageDocumentResult key_result = validate_identifier(
                    key, kStorageMaximumKeyBytes);
                const StorageDocumentResult value_result = validate_value(
                    value, kStorageMaximumValueBytes);
                if (key_result == StorageDocumentResult::limit_exceeded ||
                    value_result == StorageDocumentResult::limit_exceeded) {
                    return StorageDocumentResult::limit_exceeded;
                }
                if (key_result != StorageDocumentResult::ok ||
                    value_result != StorageDocumentResult::ok) {
                    return StorageDocumentResult::format_error;
                }
                if (parsed.entry_count >= kStorageMaximumEntries) {
                    return StorageDocumentResult::limit_exceeded;
                }
                const std::size_t added = key.size() + value.size();
                if (parsed.decoded_bytes >
                    kStorageMaximumDecodedBytes - added) {
                    return StorageDocumentResult::limit_exceeded;
                }
                if (!current_values->emplace(
                        std::string(key), std::string(value)).second) {
                    return StorageDocumentResult::format_error;
                }
                parsed.decoded_bytes += added;
                ++parsed.entry_count;
            } else {
                return StorageDocumentResult::format_error;
            }
        }

        if (newline == std::string_view::npos) break;
        cursor = newline + 1;
    }
    output = std::move(parsed);
    return StorageDocumentResult::ok;
}

StorageDocumentResult serialize_document(
    const Sections& sections,
    std::string& output) {
    output.clear();
    output.append(kGeneratedHeader);
    for (const auto& [section, values] : sections) {
        output.append("\r\n[");
        output.append(section);
        output.append("]\r\n");
        for (const auto& [key, value] : values) {
            output.append(key);
            output.push_back('=');
            output.append(value);
            output.append("\r\n");
            if (output.size() > kStorageMaximumFileBytes) {
                output.clear();
                return StorageDocumentResult::limit_exceeded;
            }
        }
    }
    return output.size() <= kStorageMaximumFileBytes
        ? StorageDocumentResult::ok
        : StorageDocumentResult::limit_exceeded;
}

StorageDocumentResult read_file(
    const std::filesystem::path& path,
    std::string& output,
    bool& missing) noexcept {
    missing = false;
    HANDLE file = CreateFileW(
        path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        const DWORD error = GetLastError();
        missing = error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
        return missing ? StorageDocumentResult::ok
                       : StorageDocumentResult::io_error;
    }

    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file, &size) || size.QuadPart < 0) {
        CloseHandle(file);
        return StorageDocumentResult::io_error;
    }
    if (static_cast<std::uint64_t>(size.QuadPart) >
        kStorageMaximumFileBytes) {
        CloseHandle(file);
        return StorageDocumentResult::limit_exceeded;
    }

    try {
        output.assign(static_cast<std::size_t>(size.QuadPart), '\0');
    } catch (...) {
        CloseHandle(file);
        return StorageDocumentResult::out_of_memory;
    }

    std::size_t offset = 0;
    while (offset < output.size()) {
        const std::size_t remaining = output.size() - offset;
        const DWORD request = static_cast<DWORD>(std::min<std::size_t>(
            remaining, std::numeric_limits<DWORD>::max()));
        DWORD read{};
        if (!ReadFile(file, output.data() + offset, request, &read, nullptr) ||
            read == 0) {
            CloseHandle(file);
            output.clear();
            return StorageDocumentResult::io_error;
        }
        offset += read;
    }
    CloseHandle(file);
    return StorageDocumentResult::ok;
}

std::atomic_uint64_t g_temporary_counter{};

StorageDocumentResult write_file_atomically(
    const std::filesystem::path& path,
    std::string_view bytes) noexcept {
    std::error_code filesystem_error{};
    const std::filesystem::path parent = path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, filesystem_error);
        if (filesystem_error) return StorageDocumentResult::io_error;
    }

    std::filesystem::path temporary{};
    HANDLE file = INVALID_HANDLE_VALUE;
    try {
        for (unsigned attempt = 0; attempt < 16; ++attempt) {
            const std::uint64_t suffix =
                g_temporary_counter.fetch_add(1, std::memory_order_relaxed) + 1;
            temporary = path;
            temporary += L".tmp." + std::to_wstring(GetCurrentProcessId()) +
                L"." + std::to_wstring(GetTickCount64()) + L"." +
                std::to_wstring(suffix);
            file = CreateFileW(
                temporary.c_str(),
                GENERIC_WRITE,
                0,
                nullptr,
                CREATE_NEW,
                FILE_ATTRIBUTE_NORMAL,
                nullptr);
            if (file != INVALID_HANDLE_VALUE) break;
            if (GetLastError() != ERROR_FILE_EXISTS) {
                return StorageDocumentResult::io_error;
            }
        }
    } catch (...) {
        return StorageDocumentResult::out_of_memory;
    }
    if (file == INVALID_HANDLE_VALUE) return StorageDocumentResult::io_error;

    std::size_t offset = 0;
    bool wrote = true;
    while (offset < bytes.size()) {
        const std::size_t remaining = bytes.size() - offset;
        const DWORD request = static_cast<DWORD>(std::min<std::size_t>(
            remaining, std::numeric_limits<DWORD>::max()));
        DWORD written{};
        if (!WriteFile(file, bytes.data() + offset, request, &written, nullptr) ||
            written == 0) {
            wrote = false;
            break;
        }
        offset += written;
    }
    if (wrote) wrote = FlushFileBuffers(file) != FALSE;
    CloseHandle(file);
    if (!wrote) {
        DeleteFileW(temporary.c_str());
        return StorageDocumentResult::io_error;
    }

    if (ReplaceFileW(
            path.c_str(),
            temporary.c_str(),
            nullptr,
            REPLACEFILE_WRITE_THROUGH,
            nullptr,
            nullptr)) {
        return StorageDocumentResult::ok;
    }

    if (MoveFileExW(
            temporary.c_str(),
            path.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        return StorageDocumentResult::ok;
    }

    DeleteFileW(temporary.c_str());
    return StorageDocumentResult::io_error;
}

} // namespace

StorageDocument::StorageDocument(std::filesystem::path path)
    : path_(std::move(path)) {}

void StorageDocument::advance_revision(std::uint64_t& revision) noexcept {
    ++revision;
    if (revision == 0) ++revision;
}

StorageDocumentResult StorageDocument::load() noexcept {
    if (path_.empty() || !path_.has_filename()) {
        return StorageDocumentResult::invalid_argument;
    }
    std::lock_guard io_lock(io_mutex_);
    try {
        std::string bytes{};
        bool missing = false;
        const StorageDocumentResult read = read_file(path_, bytes, missing);
        if (read != StorageDocumentResult::ok) return read;

        ParsedDocument parsed{};
        if (!missing) {
            const StorageDocumentResult result = parse_document(bytes, parsed);
            if (result != StorageDocumentResult::ok) return result;
        }

        std::lock_guard lock(mutex_);
        sections_ = std::move(parsed.sections);
        entry_count_ = parsed.entry_count;
        decoded_bytes_ = parsed.decoded_bytes;
        advance_revision(current_revision_);
        last_saved_revision_ = current_revision_;
        loaded_ = true;
        dirty_ = false;
        return StorageDocumentResult::ok;
    } catch (const std::bad_alloc&) {
        return StorageDocumentResult::out_of_memory;
    } catch (...) {
        return StorageDocumentResult::io_error;
    }
}

StorageDocumentResult StorageDocument::save() noexcept {
    if (path_.empty() || !path_.has_filename()) {
        return StorageDocumentResult::invalid_argument;
    }
    std::lock_guard io_lock(io_mutex_);
    try {
        Sections snapshot{};
        std::uint64_t snapshot_revision{};
        {
            std::lock_guard lock(mutex_);
            if (!loaded_) return StorageDocumentResult::not_loaded;
            if (!dirty_) return StorageDocumentResult::ok;
            snapshot = sections_;
            snapshot_revision = current_revision_;
        }

        std::string bytes{};
        const StorageDocumentResult serialized =
            serialize_document(snapshot, bytes);
        if (serialized != StorageDocumentResult::ok) return serialized;
        const StorageDocumentResult written =
            write_file_atomically(path_, bytes);
        if (written != StorageDocumentResult::ok) return written;

        std::lock_guard lock(mutex_);
        last_saved_revision_ = snapshot_revision;
        if (current_revision_ == snapshot_revision) dirty_ = false;
        return StorageDocumentResult::ok;
    } catch (const std::bad_alloc&) {
        return StorageDocumentResult::out_of_memory;
    } catch (...) {
        return StorageDocumentResult::io_error;
    }
}

StorageDocumentResult StorageDocument::get(
    std::string_view section,
    std::string_view key,
    std::string& output,
    bool& found) const noexcept {
    found = false;
    const StorageDocumentResult section_result =
        validate_identifier(section, kStorageMaximumSectionBytes);
    if (section_result != StorageDocumentResult::ok) return section_result;
    const StorageDocumentResult key_result =
        validate_identifier(key, kStorageMaximumKeyBytes);
    if (key_result != StorageDocumentResult::ok) return key_result;
    try {
        std::lock_guard lock(mutex_);
        if (!loaded_) return StorageDocumentResult::not_loaded;
        const auto section_it = sections_.find(section);
        if (section_it == sections_.end()) return StorageDocumentResult::ok;
        const auto value_it = section_it->second.find(key);
        if (value_it == section_it->second.end()) {
            return StorageDocumentResult::ok;
        }
        std::string value = value_it->second;
        output = std::move(value);
        found = true;
        return StorageDocumentResult::ok;
    } catch (const std::bad_alloc&) {
        return StorageDocumentResult::out_of_memory;
    } catch (...) {
        return StorageDocumentResult::io_error;
    }
}

StorageDocumentResult StorageDocument::set(
    std::string_view section,
    std::string_view key,
    std::string_view value) noexcept {
    const StorageDocumentResult section_result =
        validate_identifier(section, kStorageMaximumSectionBytes);
    if (section_result != StorageDocumentResult::ok) return section_result;
    const StorageDocumentResult key_result =
        validate_identifier(key, kStorageMaximumKeyBytes);
    if (key_result != StorageDocumentResult::ok) return key_result;
    const StorageDocumentResult value_result =
        validate_value(value, kStorageMaximumValueBytes);
    if (value_result != StorageDocumentResult::ok) return value_result;
    try {
        std::lock_guard lock(mutex_);
        if (!loaded_) return StorageDocumentResult::not_loaded;

        auto section_it = sections_.find(section);
        const bool new_section = section_it == sections_.end();
        if (new_section && sections_.size() >= kStorageMaximumSections) {
            return StorageDocumentResult::limit_exceeded;
        }
        const auto value_it = new_section
            ? Values::iterator{}
            : section_it->second.find(key);
        const bool new_entry = new_section || value_it == section_it->second.end();
        if (!new_entry && value_it->second == value) {
            return StorageDocumentResult::ok;
        }
        if (new_entry && entry_count_ >= kStorageMaximumEntries) {
            return StorageDocumentResult::limit_exceeded;
        }

        std::size_t next_bytes = decoded_bytes_;
        if (new_section) next_bytes += section.size();
        if (new_entry) {
            next_bytes += key.size() + value.size();
        } else {
            next_bytes -= value_it->second.size();
            next_bytes += value.size();
        }
        if (next_bytes > kStorageMaximumDecodedBytes) {
            return StorageDocumentResult::limit_exceeded;
        }

        // Materialize caller-owned views before changing the map. The
        // subsequent operations then preserve the strong guarantee:
        // allocation failure never leaves a newly-created empty section or a
        // partially replaced value behind.
        std::string section_copy(section);
        std::string key_copy(key);
        std::string value_copy(value);
        if (new_section) {
            Values values{};
            values.emplace(std::move(key_copy), std::move(value_copy));
            const auto inserted = sections_.emplace(
                std::move(section_copy), std::move(values));
            if (!inserted.second) return StorageDocumentResult::io_error;
            ++entry_count_;
        } else if (new_entry) {
            const auto inserted = section_it->second.emplace(
                std::move(key_copy), std::move(value_copy));
            if (!inserted.second) return StorageDocumentResult::io_error;
            ++entry_count_;
        } else {
            value_it->second.swap(value_copy);
        }
        decoded_bytes_ = next_bytes;
        advance_revision(current_revision_);
        dirty_ = true;
        return StorageDocumentResult::ok;
    } catch (const std::bad_alloc&) {
        return StorageDocumentResult::out_of_memory;
    } catch (...) {
        return StorageDocumentResult::io_error;
    }
}

StorageDocumentResult StorageDocument::erase(
    std::string_view section,
    std::string_view key,
    bool* erased) noexcept {
    const StorageDocumentResult section_result =
        validate_identifier(section, kStorageMaximumSectionBytes);
    if (section_result != StorageDocumentResult::ok) return section_result;
    const StorageDocumentResult key_result =
        validate_identifier(key, kStorageMaximumKeyBytes);
    if (key_result != StorageDocumentResult::ok) return key_result;
    std::lock_guard lock(mutex_);
    if (!loaded_) return StorageDocumentResult::not_loaded;
    const auto section_it = sections_.find(section);
    if (section_it == sections_.end()) {
        if (erased) *erased = false;
        return StorageDocumentResult::ok;
    }
    const auto value_it = section_it->second.find(key);
    if (value_it == section_it->second.end()) {
        if (erased) *erased = false;
        return StorageDocumentResult::ok;
    }

    decoded_bytes_ -= value_it->first.size() + value_it->second.size();
    section_it->second.erase(value_it);
    --entry_count_;
    if (section_it->second.empty()) {
        decoded_bytes_ -= section_it->first.size();
        sections_.erase(section_it);
    }
    advance_revision(current_revision_);
    dirty_ = true;
    if (erased) *erased = true;
    return StorageDocumentResult::ok;
}

StorageDocumentResult StorageDocument::apply_batch(
    std::string_view section,
    const std::vector<StorageMutation>& mutations) noexcept {
    const StorageDocumentResult section_result =
        validate_identifier(section, kStorageMaximumSectionBytes);
    if (section_result != StorageDocumentResult::ok) return section_result;

    // Validate every caller-owned byte before copying or taking the document
    // lock. This also makes an invalid operation deterministic regardless of
    // whether an earlier operation in the batch would have been a no-op.
    for (const StorageMutation& mutation : mutations) {
        const StorageDocumentResult key_result =
            validate_identifier(mutation.key, kStorageMaximumKeyBytes);
        if (key_result != StorageDocumentResult::ok) return key_result;
        switch (mutation.kind) {
        case StorageMutationKind::set: {
            const StorageDocumentResult value_result = validate_value(
                mutation.value, kStorageMaximumValueBytes);
            if (value_result != StorageDocumentResult::ok) return value_result;
            break;
        }
        case StorageMutationKind::erase:
            break;
        default:
            return StorageDocumentResult::invalid_argument;
        }
    }

    try {
        std::lock_guard lock(mutex_);
        if (!loaded_) return StorageDocumentResult::not_loaded;
        if (mutations.empty()) return StorageDocumentResult::ok;

        // The live document is not touched until every allocation, mutation,
        // and bounds check succeeds. std::map::swap is noexcept for these
        // standard allocator/comparator types, so the commit cannot be partial.
        Sections next = sections_;
        for (const StorageMutation& mutation : mutations) {
            if (mutation.kind == StorageMutationKind::set) {
                next[std::string(section)][mutation.key] = mutation.value;
                continue;
            }

            const auto section_it = next.find(section);
            if (section_it == next.end()) continue;
            section_it->second.erase(mutation.key);
            if (section_it->second.empty()) next.erase(section_it);
        }

        if (next == sections_) return StorageDocumentResult::ok;
        if (next.size() > kStorageMaximumSections) {
            return StorageDocumentResult::limit_exceeded;
        }

        std::size_t next_entries = 0;
        std::size_t next_bytes = 0;
        for (const auto& [section_name, values] : next) {
            if (next_bytes >
                kStorageMaximumDecodedBytes - section_name.size()) {
                return StorageDocumentResult::limit_exceeded;
            }
            next_bytes += section_name.size();
            if (values.size() > kStorageMaximumEntries - next_entries) {
                return StorageDocumentResult::limit_exceeded;
            }
            next_entries += values.size();
            for (const auto& [key, value] : values) {
                const std::size_t added = key.size() + value.size();
                if (next_bytes > kStorageMaximumDecodedBytes - added) {
                    return StorageDocumentResult::limit_exceeded;
                }
                next_bytes += added;
            }
        }

        sections_.swap(next);
        entry_count_ = next_entries;
        decoded_bytes_ = next_bytes;
        advance_revision(current_revision_);
        dirty_ = true;
        return StorageDocumentResult::ok;
    } catch (const std::bad_alloc&) {
        return StorageDocumentResult::out_of_memory;
    } catch (...) {
        return StorageDocumentResult::io_error;
    }
}

bool StorageDocument::is_loaded() const noexcept {
    std::lock_guard lock(mutex_);
    return loaded_;
}

bool StorageDocument::is_dirty() const noexcept {
    std::lock_guard lock(mutex_);
    return dirty_;
}

std::uint64_t StorageDocument::current_revision() const noexcept {
    std::lock_guard lock(mutex_);
    return current_revision_;
}

std::uint64_t StorageDocument::last_saved_revision() const noexcept {
    std::lock_guard lock(mutex_);
    return last_saved_revision_;
}

std::size_t StorageDocument::entry_count() const noexcept {
    std::lock_guard lock(mutex_);
    return entry_count_;
}

} // namespace erui::host
