#include "host_storage.hpp"

#include "input_binding_model.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <limits>
#include <map>
#include <new>
#include <utility>
#include <vector>

namespace erui::host {
namespace {

constexpr std::size_t kMaximumProviderIdBytes = 255;
constexpr std::size_t kMaximumPlainProviderDirectoryBytes = 128;
constexpr std::size_t kMaximumPathUnits = 32767;
constexpr std::uint32_t kMaximumAssignmentChanges =
    static_cast<std::uint32_t>(kStorageMaximumEntries);
static_assert(kStorageMaximumSectionBytes ==
    ERUI_STORAGE_MAX_IDENTIFIER_BYTES);
static_assert(kStorageMaximumKeyBytes ==
    ERUI_STORAGE_MAX_IDENTIFIER_BYTES);
static_assert(kStorageMaximumValueBytes == ERUI_STORAGE_MAX_VALUE_BYTES);
static_assert(kMaximumAssignmentChanges ==
    ERUI_STORAGE_MAX_ASSIGNMENT_CHANGES);

bool valid_utf8(std::string_view value) noexcept {
    if (value.empty() || value.size() >
            static_cast<std::size_t>((std::numeric_limits<int>::max)())) {
        return false;
    }
    return MultiByteToWideChar(
               CP_UTF8,
               MB_ERR_INVALID_CHARS,
               value.data(),
               static_cast<int>(value.size()),
               nullptr,
               0) > 0;
}

bool copy_utf16_path(ERUI_Utf16View view, std::wstring& output) {
    output.clear();
    if (view.reserved != 0 || view.length == 0 || !view.data ||
        view.length > kMaximumPathUnits) {
        return false;
    }
    static_assert(sizeof(wchar_t) == sizeof(std::uint16_t));
    output.assign(view.length, L'\0');
    std::memcpy(
        output.data(),
        view.data,
        static_cast<std::size_t>(view.length) * sizeof(std::uint16_t));
    if (std::find(output.begin(), output.end(), L'\0') != output.end()) {
        return false;
    }
    for (std::size_t index = 0; index < output.size(); ++index) {
        const auto unit = static_cast<std::uint16_t>(output[index]);
        if (unit >= 0xD800u && unit <= 0xDBFFu) {
            if (++index >= output.size()) return false;
            const auto low = static_cast<std::uint16_t>(output[index]);
            if (low < 0xDC00u || low > 0xDFFFu) return false;
        } else if (unit >= 0xDC00u && unit <= 0xDFFFu) {
            return false;
        }
    }
    return true;
}

bool valid_string_view(ERUI_StringView view, bool allow_empty) noexcept {
    if (view.reserved != 0 || (!view.data && view.length != 0)) return false;
    if (!allow_empty && view.length == 0) return false;
    if (view.length != 0 &&
        std::memchr(view.data, '\0', view.length) != nullptr) {
        return false;
    }
    return true;
}

bool copy_string_view(
    ERUI_StringView view,
    std::string& output,
    bool allow_empty) {
    if (!valid_string_view(view, allow_empty)) return false;
    output.assign(view.data ? view.data : "", view.length);
    return true;
}

bool valid_storage_desc(const ERUI_StorageDesc* description) noexcept {
    return description && description->size >= sizeof(*description) &&
        description->flags == 0 && description->reserved0 == 0 &&
        description->path.reserved == 0 &&
        description->reserved[0] == 0 && description->reserved[1] == 0;
}

bool valid_storage_key(const ERUI_StorageKey* key) noexcept {
    return key && key->size >= sizeof(*key) && key->flags == 0 &&
        key->reserved[0] == 0 && key->reserved[1] == 0 &&
        key->section.length <= kStorageMaximumSectionBytes &&
        key->key.length <= kStorageMaximumKeyBytes &&
        valid_string_view(key->section, false) &&
        valid_string_view(key->key, false);
}

bool module_path(HMODULE module, std::filesystem::path& output) {
    if (!module) return false;
    std::vector<wchar_t> buffer(512);
    while (buffer.size() <= kMaximumPathUnits + 1) {
        SetLastError(ERROR_SUCCESS);
        const DWORD length = GetModuleFileNameW(
            module, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) return false;
        if (length < buffer.size() - 1 ||
            (length < buffer.size() && GetLastError() != ERROR_INSUFFICIENT_BUFFER)) {
            output = std::filesystem::path(
                std::wstring_view(buffer.data(), length));
            return output.is_absolute() && output.has_filename();
        }
        buffer.resize(buffer.size() * 2);
    }
    return false;
}

char upper_ascii(char value) noexcept {
    return value >= 'a' && value <= 'z'
        ? static_cast<char>(value - 'a' + 'A')
        : value;
}

bool reserved_windows_name(std::string_view value) noexcept {
    const std::size_t dot = value.find('.');
    value = value.substr(0, dot);
    std::string upper{};
    upper.reserve(value.size());
    for (char byte : value) upper.push_back(upper_ascii(byte));
    if (upper == "CON" || upper == "PRN" || upper == "AUX" ||
        upper == "NUL") {
        return true;
    }
    if (upper.size() == 4 && upper[3] >= '1' && upper[3] <= '9') {
        return upper.starts_with("COM") || upper.starts_with("LPT");
    }
    return false;
}

char hex_digit(unsigned value) noexcept {
    return static_cast<char>(value < 10 ? '0' + value : 'a' + value - 10);
}

constexpr std::uint32_t rotate_right(
    std::uint32_t value,
    unsigned amount) noexcept {
    return (value >> amount) | (value << (32u - amount));
}

std::array<std::uint8_t, 32> sha256(std::string_view input) noexcept {
    static constexpr std::array<std::uint32_t, 64> constants{
        0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
        0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
        0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
        0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
        0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
        0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
        0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
        0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
        0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
        0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
        0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
        0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
        0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
        0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
        0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
        0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u,
    };
    std::array<std::uint32_t, 8> state{
        0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
        0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u,
    };

    // Provider IDs are bounded to 255 bytes. SHA-256 needs at most one extra
    // 64-byte block for its marker and 64-bit length field.
    std::array<std::uint8_t, 320> padded{};
    std::copy(input.begin(), input.end(), padded.begin());
    padded[input.size()] = 0x80u;
    const std::size_t padded_size = (input.size() + 9u + 63u) & ~63u;
    const std::uint64_t bit_count =
        static_cast<std::uint64_t>(input.size()) * 8u;
    for (unsigned byte = 0; byte < 8; ++byte) {
        padded[padded_size - 1u - byte] =
            static_cast<std::uint8_t>(bit_count >> (byte * 8u));
    }

    for (std::size_t block = 0; block < padded_size; block += 64) {
        std::array<std::uint32_t, 64> words{};
        for (std::size_t index = 0; index < 16; ++index) {
            const std::size_t offset = block + index * 4;
            words[index] =
                (static_cast<std::uint32_t>(padded[offset]) << 24u) |
                (static_cast<std::uint32_t>(padded[offset + 1]) << 16u) |
                (static_cast<std::uint32_t>(padded[offset + 2]) << 8u) |
                static_cast<std::uint32_t>(padded[offset + 3]);
        }
        for (std::size_t index = 16; index < words.size(); ++index) {
            const std::uint32_t first =
                rotate_right(words[index - 15], 7) ^
                rotate_right(words[index - 15], 18) ^
                (words[index - 15] >> 3u);
            const std::uint32_t second =
                rotate_right(words[index - 2], 17) ^
                rotate_right(words[index - 2], 19) ^
                (words[index - 2] >> 10u);
            words[index] = words[index - 16] + first +
                words[index - 7] + second;
        }

        std::uint32_t a = state[0];
        std::uint32_t b = state[1];
        std::uint32_t c = state[2];
        std::uint32_t d = state[3];
        std::uint32_t e = state[4];
        std::uint32_t f = state[5];
        std::uint32_t g = state[6];
        std::uint32_t h = state[7];
        for (std::size_t index = 0; index < words.size(); ++index) {
            const std::uint32_t choose = (e & f) ^ (~e & g);
            const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
            const std::uint32_t sum0 =
                rotate_right(a, 2) ^ rotate_right(a, 13) ^
                rotate_right(a, 22);
            const std::uint32_t sum1 =
                rotate_right(e, 6) ^ rotate_right(e, 11) ^
                rotate_right(e, 25);
            const std::uint32_t temporary1 = h + sum1 + choose +
                constants[index] + words[index];
            const std::uint32_t temporary2 = sum0 + majority;
            h = g;
            g = f;
            f = e;
            e = d + temporary1;
            d = c;
            c = b;
            b = a;
            a = temporary1 + temporary2;
        }
        state[0] += a;
        state[1] += b;
        state[2] += c;
        state[3] += d;
        state[4] += e;
        state[5] += f;
        state[6] += g;
        state[7] += h;
    }

    std::array<std::uint8_t, 32> digest{};
    for (std::size_t index = 0; index < state.size(); ++index) {
        digest[index * 4] = static_cast<std::uint8_t>(state[index] >> 24u);
        digest[index * 4 + 1] =
            static_cast<std::uint8_t>(state[index] >> 16u);
        digest[index * 4 + 2] =
            static_cast<std::uint8_t>(state[index] >> 8u);
        digest[index * 4 + 3] = static_cast<std::uint8_t>(state[index]);
    }
    return digest;
}

bool can_use_plain_provider_directory(std::string_view provider_id) noexcept {
    if (provider_id.empty() ||
        provider_id.size() > kMaximumPlainProviderDirectoryBytes ||
        provider_id.front() == '.' || provider_id.back() == '.' ||
        provider_id.find("..") != std::string_view::npos ||
        reserved_windows_name(provider_id)) {
        return false;
    }
    return std::all_of(
        provider_id.begin(), provider_id.end(), [](unsigned char byte) {
            return (byte >= 'a' && byte <= 'z') ||
                (byte >= '0' && byte <= '9') || byte == '-' ||
                byte == '_' || byte == '.';
        });
}

bool sanitized_provider_directory(
    std::string_view provider_id,
    std::filesystem::path& output) {
    if (provider_id.empty() || provider_id.size() > kMaximumProviderIdBytes ||
        !valid_utf8(provider_id) ||
        std::memchr(provider_id.data(), '\0', provider_id.size()) != nullptr) {
        return false;
    }
    if (can_use_plain_provider_directory(provider_id)) {
        output = std::filesystem::path(provider_id);
        return true;
    }

    // Valid provider IDs never begin with '~', so this reserved namespace
    // cannot overlap a plain directory name. Hashing the exact bytes keeps
    // Windows aliases (case, trailing dots, DOS device names) distinct without
    // producing oversized path components.
    const auto digest = sha256(provider_id);
    std::string safe(1 + digest.size() * 2, '~');
    for (std::size_t index = 0; index < digest.size(); ++index) {
        safe[1 + index * 2] = hex_digit(digest[index] >> 4u);
        safe[1 + index * 2 + 1] = hex_digit(digest[index] & 0x0fu);
    }
    output = std::filesystem::path(safe);
    return !output.empty() && output.has_filename();
}

bool path_component_equal(
    const std::filesystem::path& left,
    const std::filesystem::path& right) noexcept {
    return _wcsicmp(left.c_str(), right.c_str()) == 0;
}

bool is_strictly_beneath(
    const std::filesystem::path& child,
    const std::filesystem::path& parent) noexcept {
    auto child_it = child.begin();
    for (auto parent_it = parent.begin(); parent_it != parent.end();
         ++parent_it, ++child_it) {
        if (child_it == child.end() ||
            !path_component_equal(*child_it, *parent_it)) {
            return false;
        }
    }
    return child_it != child.end();
}

bool contains_parent_component(const std::filesystem::path& path) {
    return std::any_of(path.begin(), path.end(), [](const auto& component) {
        return component == L"..";
    });
}

bool resolve_storage_path(
    std::string_view provider_id,
    HMODULE host_module,
    HMODULE owner_module,
    const ERUI_StorageDesc& description,
    std::filesystem::path& output) {
    std::filesystem::path module{};
    std::wstring requested_text{};
    std::error_code error{};

    switch (description.location) {
    case ERUI_STORAGE_LOCATION_PROVIDER_DEFAULT: {
        if (description.path.length != 0) return false;
        if (!module_path(host_module, module)) return false;
        std::filesystem::path provider_directory{};
        if (!sanitized_provider_directory(provider_id, provider_directory)) {
            return false;
        }
        output = (module.parent_path() / L"mods" / provider_directory /
            L"config.ini").lexically_normal();
        return output.is_absolute() && output.has_filename();
    }
    case ERUI_STORAGE_LOCATION_OWNER_MODULE_DIRECTORY: {
        if (!copy_utf16_path(description.path, requested_text) ||
            !module_path(owner_module, module)) {
            return false;
        }
        const std::filesystem::path requested(requested_text);
        if (requested.is_absolute() || requested.has_root_name() ||
            requested.has_root_directory() || !requested.has_filename() ||
            contains_parent_component(requested) ||
            requested_text.find(L':') != std::wstring::npos) {
            return false;
        }
        const std::filesystem::path base =
            std::filesystem::weakly_canonical(module.parent_path(), error);
        if (error || base.empty()) return false;
        const std::filesystem::path candidate =
            std::filesystem::weakly_canonical(base / requested, error);
        if (error || !candidate.has_filename() ||
            !is_strictly_beneath(candidate, base)) {
            return false;
        }
        output = candidate;
        return true;
    }
    case ERUI_STORAGE_LOCATION_EXPLICIT_ABSOLUTE: {
        if (!copy_utf16_path(description.path, requested_text)) return false;
        const std::filesystem::path requested(requested_text);
        if (!requested.is_absolute() || !requested.has_filename() ||
            contains_parent_component(requested) ||
            requested != requested.lexically_normal()) {
            return false;
        }
        output = requested;
        return true;
    }
    default:
        return false;
    }
}

ERUI_Result map_document_result(
    StorageDocumentResult result,
    bool reading_file = false) noexcept {
    switch (result) {
    case StorageDocumentResult::ok: return ERUI_OK;
    case StorageDocumentResult::invalid_argument: return ERUI_INVALID_ARGUMENT;
    case StorageDocumentResult::not_loaded: return ERUI_STORAGE_NOT_LOADED;
    case StorageDocumentResult::io_error: return ERUI_STORAGE_IO_ERROR;
    case StorageDocumentResult::format_error: return ERUI_STORAGE_FORMAT_ERROR;
    case StorageDocumentResult::limit_exceeded:
        return reading_file ? static_cast<ERUI_Result>(ERUI_STORAGE_FORMAT_ERROR)
                            : static_cast<ERUI_Result>(ERUI_INVALID_ARGUMENT);
    case StorageDocumentResult::out_of_memory: return ERUI_OUT_OF_MEMORY;
    default: return ERUI_INTERNAL_ERROR;
    }
}

void make_empty_inputs(ERUI_ActionInputs& value) noexcept {
    value = {};
    value.size = sizeof(value);
}

bool valid_assignment_change(const ERUI_AssignmentChange& change) noexcept {
    if (change.size < sizeof(change) || change.flags != 0 ||
        change.action == ERUI_INVALID_INPUT_ACTION ||
        change.reserved[0] != 0 || change.reserved[1] != 0 ||
        change.action_id.length > kStorageMaximumKeyBytes ||
        !valid_string_view(change.action_id, false) ||
        !erui::detail::valid_action_inputs(change.previous) ||
        !erui::detail::valid_action_inputs(change.current) ||
        !erui::detail::same_supported_input_devices(
            change.previous, change.current)) {
        return false;
    }
    if (change.reason > ERUI_ASSIGNMENT_CHANGE_RESET_TO_DEFAULTS) {
        return false;
    }
    constexpr ERUI_InputDevices known = ERUI_INPUT_DEVICE_ALL;
    if (change.changed_devices == ERUI_INPUT_DEVICE_NONE ||
        (change.changed_devices & ~known) != 0 ||
        change.changed_devices != erui::detail::changed_input_devices(
            change.previous, change.current)) {
        return false;
    }
    return (change.changed_devices &
        ~erui::detail::supported_input_devices(change.current)) == 0;
}

void copy_slot_from_current(
    ERUI_ActionInputs& sparse,
    const ERUI_ActionInputs& current,
    ERUI_InputDevices devices,
    bool reset) noexcept {
    if ((devices & ERUI_INPUT_DEVICE_CONTROLLER) != 0) {
        sparse.controller = reset ? ERUI_ControllerInput{} : current.controller;
    }
    if ((devices & ERUI_INPUT_DEVICE_KEYBOARD) != 0) {
        sparse.keyboard = reset ? ERUI_KeyboardInput{} : current.keyboard;
    }
    if ((devices & ERUI_INPUT_DEVICE_MOUSE) != 0) {
        sparse.mouse = reset ? ERUI_MouseInput{} : current.mouse;
    }
}

struct BatchMutation {
    std::string key{};
    ERUI_ActionInputs value{};
    std::string encoded{};
};

} // namespace

ERUI_Result encode_action_inputs(
    const ERUI_ActionInputs& value,
    std::string& output) noexcept {
    std::uint32_t required = 0;
    ERUI_Result result = ERUI_FormatActionInputs(
        &value, nullptr, 0, &required);
    if (result != ERUI_OK) return result;
    try {
        std::string encoded(required, '\0');
        if (required != 0) {
            std::uint32_t written = 0;
            result = ERUI_FormatActionInputs(
                &value, encoded.data(), required, &written);
            if (result != ERUI_OK || written != required) {
                return result != ERUI_OK
                    ? result
                    : static_cast<ERUI_Result>(ERUI_INTERNAL_ERROR);
            }
        }
        output.swap(encoded);
        return ERUI_OK;
    } catch (const std::bad_alloc&) {
        return ERUI_OUT_OF_MEMORY;
    } catch (...) {
        return ERUI_INTERNAL_ERROR;
    }
}

ERUI_Result decode_action_inputs(
    std::string_view text,
    ERUI_ActionInputs& output) noexcept {
    if (text.size() > (std::numeric_limits<std::uint32_t>::max)()) {
        return ERUI_STORAGE_FORMAT_ERROR;
    }
    const ERUI_StringView view{
        text.data(), static_cast<std::uint32_t>(text.size()), 0};
    ERUI_ActionInputs parsed{};
    const ERUI_Result result = ERUI_ParseActionInputs(&view, &parsed);
    if (result != ERUI_OK) return result;
    output = parsed;
    return ERUI_OK;
}

ERUI_Result ProviderStorage::open(
    std::string_view provider_id,
    HMODULE host_module,
    HMODULE owner_module,
    const ERUI_StorageDesc* description,
    std::unique_ptr<ProviderStorage>& output) noexcept {
    if (!valid_storage_desc(description)) return ERUI_INVALID_ARGUMENT;
    try {
        std::filesystem::path path{};
        if (!resolve_storage_path(
                provider_id, host_module, owner_module, *description, path)) {
            return ERUI_INVALID_ARGUMENT;
        }
        auto storage = std::unique_ptr<ProviderStorage>(
            new ProviderStorage(std::move(path)));
        output = std::move(storage);
        return ERUI_OK;
    } catch (const std::bad_alloc&) {
        return ERUI_OUT_OF_MEMORY;
    } catch (...) {
        return ERUI_INVALID_ARGUMENT;
    }
}

ERUI_Result ProviderStorage::load() noexcept {
    std::lock_guard lock(mutex_);
    if (document_.is_loaded()) return ERUI_STORAGE_ALREADY_LOADED;
    return map_document_result(document_.load(), true);
}

ERUI_Result ProviderStorage::save() noexcept {
    std::lock_guard lock(mutex_);
    return map_document_result(document_.save());
}

ERUI_Result ProviderStorage::get_utf8(
    const ERUI_StorageKey* key,
    char* output,
    std::uint32_t output_capacity,
    std::uint32_t* out_length) const noexcept {
    if (!valid_storage_key(key) || !out_length ||
        (!output && output_capacity != 0)) {
        return ERUI_INVALID_ARGUMENT;
    }
    try {
        std::string section{};
        std::string name{};
        if (!copy_string_view(key->section, section, false) ||
            !copy_string_view(key->key, name, false)) {
            return ERUI_INVALID_ARGUMENT;
        }
        std::lock_guard lock(mutex_);
        std::string value{};
        bool found = false;
        const ERUI_Result result = map_document_result(
            document_.get(section, name, value, found));
        if (result != ERUI_OK) return result;
        if (!found) return ERUI_NOT_FOUND;
        if (value.size() > (std::numeric_limits<std::uint32_t>::max)()) {
            return ERUI_INTERNAL_ERROR;
        }
        const auto required = static_cast<std::uint32_t>(value.size());
        *out_length = required;
        if (!output) return ERUI_OK;
        if (output_capacity < required) return ERUI_BUFFER_TOO_SMALL;
        if (required != 0) std::memcpy(output, value.data(), required);
        return ERUI_OK;
    } catch (const std::bad_alloc&) {
        return ERUI_OUT_OF_MEMORY;
    } catch (...) {
        return ERUI_INTERNAL_ERROR;
    }
}

ERUI_Result ProviderStorage::set_utf8(
    const ERUI_StorageKey* key,
    const ERUI_StringView* value) noexcept {
    if (!valid_storage_key(key) || !value ||
        value->length > kStorageMaximumValueBytes ||
        !valid_string_view(*value, true)) {
        return ERUI_INVALID_ARGUMENT;
    }
    try {
        std::string section{};
        std::string name{};
        std::string copied_value{};
        if (!copy_string_view(key->section, section, false) ||
            !copy_string_view(key->key, name, false) ||
            !copy_string_view(*value, copied_value, true)) {
            return ERUI_INVALID_ARGUMENT;
        }
        std::lock_guard lock(mutex_);
        return map_document_result(document_.set(section, name, copied_value));
    } catch (const std::bad_alloc&) {
        return ERUI_OUT_OF_MEMORY;
    } catch (...) {
        return ERUI_INTERNAL_ERROR;
    }
}

ERUI_Result ProviderStorage::erase(const ERUI_StorageKey* key) noexcept {
    if (!valid_storage_key(key)) return ERUI_INVALID_ARGUMENT;
    try {
        std::string section{};
        std::string name{};
        if (!copy_string_view(key->section, section, false) ||
            !copy_string_view(key->key, name, false)) {
            return ERUI_INVALID_ARGUMENT;
        }
        std::lock_guard lock(mutex_);
        return map_document_result(document_.erase(section, name));
    } catch (const std::bad_alloc&) {
        return ERUI_OUT_OF_MEMORY;
    } catch (...) {
        return ERUI_INTERNAL_ERROR;
    }
}

ERUI_Result ProviderStorage::get_action_inputs(
    const ERUI_StorageKey* key,
    ERUI_ActionInputs* output) const noexcept {
    if (!valid_storage_key(key) || !output) {
        return ERUI_INVALID_ARGUMENT;
    }
    if (output->size != sizeof(*output)) {
        return output->size < sizeof(*output)
            ? static_cast<ERUI_Result>(ERUI_BUFFER_TOO_SMALL)
            : static_cast<ERUI_Result>(ERUI_INVALID_ARGUMENT);
    }
    try {
        std::string section{};
        std::string name{};
        if (!copy_string_view(key->section, section, false) ||
            !copy_string_view(key->key, name, false)) {
            return ERUI_INVALID_ARGUMENT;
        }
        std::lock_guard lock(mutex_);
        std::string text{};
        bool found = false;
        const ERUI_Result read_result = map_document_result(
            document_.get(section, name, text, found));
        if (read_result != ERUI_OK) return read_result;
        if (!found) return ERUI_NOT_FOUND;
        ERUI_ActionInputs decoded{};
        const ERUI_Result result = decode_action_inputs(text, decoded);
        if (result != ERUI_OK) return result;
        *output = decoded;
        return ERUI_OK;
    } catch (const std::bad_alloc&) {
        return ERUI_OUT_OF_MEMORY;
    } catch (...) {
        return ERUI_INTERNAL_ERROR;
    }
}

ERUI_Result ProviderStorage::set_action_inputs(
    const ERUI_StorageKey* key,
    const ERUI_ActionInputs* value) noexcept {
    if (!value || !erui::detail::valid_action_inputs(*value)) {
        return ERUI_INVALID_ARGUMENT;
    }
    std::string encoded{};
    const ERUI_Result result = encode_action_inputs(*value, encoded);
    if (result != ERUI_OK) return result;
    const ERUI_StringView view{
        encoded.data(), static_cast<std::uint32_t>(encoded.size()), 0};
    return set_utf8(key, &view);
}

ERUI_Result ProviderStorage::apply_assignment_changes(
    const ERUI_StringView* section_view,
    const ERUI_AssignmentChange* changes,
    std::uint32_t change_count) noexcept {
    if (!section_view ||
        section_view->length > kStorageMaximumSectionBytes ||
        !valid_string_view(*section_view, false) ||
        change_count > kMaximumAssignmentChanges ||
        (!changes && change_count != 0)) {
        return ERUI_INVALID_ARGUMENT;
    }
    try {
        std::string section{};
        if (!copy_string_view(*section_view, section, false)) {
            return ERUI_INVALID_ARGUMENT;
        }
        std::lock_guard lock(mutex_);
        if (!document_.is_loaded()) return ERUI_STORAGE_NOT_LOADED;

        std::vector<BatchMutation> mutations{};
        mutations.reserve(change_count);
        std::map<std::string, std::size_t, std::less<>> by_key{};
        for (std::uint32_t index = 0; index < change_count; ++index) {
            const ERUI_AssignmentChange& change = changes[index];
            if (!valid_assignment_change(change)) return ERUI_INVALID_ARGUMENT;
            std::string action_id{};
            if (!copy_string_view(change.action_id, action_id, false)) {
                return ERUI_INVALID_ARGUMENT;
            }

            auto existing = by_key.find(action_id);
            if (existing == by_key.end()) {
                BatchMutation mutation{};
                mutation.key = action_id;
                bool found = false;
                std::string original{};
                StorageDocumentResult read = document_.get(
                    section, mutation.key, original, found);
                const ERUI_Result read_result = map_document_result(read);
                if (read_result != ERUI_OK) return read_result;
                if (found) {
                    const ERUI_Result decoded = decode_action_inputs(
                        original, mutation.value);
                    if (decoded != ERUI_OK) return decoded;
                } else {
                    make_empty_inputs(mutation.value);
                }
                const std::size_t mutation_index = mutations.size();
                mutations.push_back(std::move(mutation));
                existing = by_key.emplace(action_id, mutation_index).first;
            }

            BatchMutation& mutation = mutations[existing->second];
            copy_slot_from_current(
                mutation.value,
                change.current,
                change.changed_devices,
                change.reason == ERUI_ASSIGNMENT_CHANGE_RESET_TO_DEFAULTS);
            if (!erui::detail::valid_action_inputs(mutation.value)) {
                return ERUI_INTERNAL_ERROR;
            }
        }

        // Finish every validation and encoding that the adapter controls,
        // then hand one owned transaction to StorageDocument. The document
        // commits a validated private copy, so failures cannot require a
        // second, potentially-failing rollback pass.
        std::vector<StorageMutation> document_mutations{};
        document_mutations.reserve(mutations.size());
        for (BatchMutation& mutation : mutations) {
            if (erui::detail::supported_input_devices(mutation.value) ==
                ERUI_INPUT_DEVICE_NONE) {
                document_mutations.push_back({
                    StorageMutationKind::erase,
                    std::move(mutation.key),
                    {},
                });
                continue;
            }
            const ERUI_Result encoded_result =
                encode_action_inputs(mutation.value, mutation.encoded);
            if (encoded_result != ERUI_OK) return encoded_result;
            document_mutations.push_back({
                StorageMutationKind::set,
                std::move(mutation.key),
                std::move(mutation.encoded),
            });
        }
        return map_document_result(
            document_.apply_batch(section, document_mutations));
    } catch (const std::bad_alloc&) {
        return ERUI_OUT_OF_MEMORY;
    } catch (...) {
        return ERUI_INTERNAL_ERROR;
    }
}

ERUI_Result ProviderStorage::get_info(ERUI_StorageInfo* output) const noexcept {
    if (!output) return ERUI_INVALID_ARGUMENT;
    if (output->size < sizeof(*output)) return ERUI_BUFFER_TOO_SMALL;
    std::lock_guard lock(mutex_);
    ERUI_StorageInfo value{};
    value.size = sizeof(value);
    value.loaded = document_.is_loaded() ? 1u : 0u;
    value.dirty = document_.is_dirty() ? 1u : 0u;
    value.current_revision = document_.current_revision();
    value.last_saved_revision = document_.last_saved_revision();
    *output = value;
    return ERUI_OK;
}

} // namespace erui::host
