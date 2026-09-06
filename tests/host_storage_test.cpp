#include "host_storage.hpp"

#include <Windows.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace {

#define CHECK(expression)                                                      \
    do {                                                                       \
        if (!(expression)) {                                                   \
            std::cerr << "check failed at line " << __LINE__ << ": "          \
                      << #expression << '\n';                                  \
            return false;                                                      \
        }                                                                      \
    } while (false)

ERUI_StringView bytes(std::string_view value) {
    return {value.data(), static_cast<std::uint32_t>(value.size()), 0};
}

ERUI_Utf16View text(std::wstring_view value) {
    return {
        reinterpret_cast<const std::uint16_t*>(value.data()),
        static_cast<std::uint32_t>(value.size()),
        0,
    };
}

ERUI_StorageDesc explicit_storage(std::wstring_view path) {
    ERUI_StorageDesc result{};
    result.size = sizeof(result);
    result.location = ERUI_STORAGE_LOCATION_EXPLICIT_ABSOLUTE;
    result.path = text(path);
    return result;
}

ERUI_StorageKey storage_key(
    std::string_view section,
    std::string_view key) {
    ERUI_StorageKey result{};
    result.size = sizeof(result);
    result.section = bytes(section);
    result.key = bytes(key);
    return result;
}

std::wstring default_storage_directory_name(std::string_view provider_id) {
    ERUI_StorageDesc description{};
    description.size = sizeof(description);
    description.location = ERUI_STORAGE_LOCATION_PROVIDER_DEFAULT;
    std::unique_ptr<erui::host::ProviderStorage> storage{};
    HMODULE process = GetModuleHandleW(nullptr);
    if (erui::host::ProviderStorage::open(
            provider_id,
            process,
            process,
            &description,
            storage) != ERUI_OK) {
        return {};
    }
    return storage->backing_path().parent_path().filename().wstring();
}

ERUI_ActionInputs empty_inputs() {
    ERUI_ActionInputs result{};
    result.size = sizeof(result);
    return result;
}

bool same_inputs(
    const ERUI_ActionInputs& left,
    const ERUI_ActionInputs& right) {
    return left.size == right.size && left.flags == right.flags &&
        left.controller.state == right.controller.state &&
        left.controller.input == right.controller.input &&
        left.keyboard.state == right.keyboard.state &&
        left.keyboard.input == right.keyboard.input &&
        left.mouse.state == right.mouse.state &&
        left.mouse.input == right.mouse.input &&
        left.reserved[0] == right.reserved[0] &&
        left.reserved[1] == right.reserved[1];
}

bool public_encode_action_inputs(
    const ERUI_ActionInputs& value,
    std::string& output) {
    std::uint32_t required{};
    CHECK(ERUI_FormatActionInputs(&value, nullptr, 0, &required) == ERUI_OK);
    std::string encoded(required, '\0');
    if (required != 0) {
        std::uint32_t written{};
        CHECK(ERUI_FormatActionInputs(
            &value, encoded.data(), required, &written) == ERUI_OK);
        CHECK(written == required);
    }
    output.swap(encoded);
    return true;
}

bool public_decode_action_inputs(
    std::string_view text_value,
    ERUI_ActionInputs& output) {
    const ERUI_StringView view = bytes(text_value);
    return ERUI_ParseActionInputs(&view, &output) == ERUI_OK;
}

ERUI_AssignmentChange change(
    std::string_view action_id,
    ERUI_ActionInputs previous,
    ERUI_ActionInputs current,
    ERUI_AssignmentChangeReason reason,
    ERUI_InputDevices devices) {
    ERUI_AssignmentChange result{};
    result.size = sizeof(result);
    result.action = 42;
    result.action_id = bytes(action_id);
    result.previous = previous;
    result.current = current;
    result.reason = reason;
    result.changed_devices = devices;
    return result;
}

bool test_codec() {
    ERUI_ActionInputs value = empty_inputs();
    value.controller = {ERUI_INPUT_SLOT_BOUND,
        ERUI_CONTROLLER_BUTTON_RIGHT_TRIGGER};
    value.keyboard = {ERUI_INPUT_SLOT_BOUND, ERUI_KEYBOARD_KEY_Q};
    value.mouse = {ERUI_INPUT_SLOT_BOUND, ERUI_MOUSE_BUTTON_4};

    std::string encoded{};
    CHECK(erui::host::encode_action_inputs(value, encoded) == ERUI_OK);
    CHECK(encoded ==
        "controller:right-trigger,keyboard:key-q,mouse:button4");
    std::string public_encoded{};
    CHECK(public_encode_action_inputs(value, public_encoded));
    CHECK(public_encoded == encoded);
    ERUI_ActionInputs decoded{};
    CHECK(erui::host::decode_action_inputs(encoded, decoded) == ERUI_OK);
    CHECK(same_inputs(value, decoded));
    ERUI_ActionInputs public_decoded{};
    CHECK(public_decode_action_inputs(encoded, public_decoded));
    CHECK(same_inputs(decoded, public_decoded));

    value = empty_inputs();
    value.controller = {ERUI_INPUT_SLOT_UNBOUND,
        ERUI_CONTROLLER_BUTTON_INVALID};
    CHECK(erui::host::encode_action_inputs(value, encoded) == ERUI_OK);
    CHECK(encoded == "controller:unbound");
    CHECK(public_encode_action_inputs(value, public_encoded));
    CHECK(public_encoded == encoded);
    CHECK(erui::host::decode_action_inputs(encoded, decoded) == ERUI_OK);
    CHECK(same_inputs(value, decoded));
    CHECK(public_decode_action_inputs(encoded, public_decoded));
    CHECK(same_inputs(decoded, public_decoded));

    value = empty_inputs();
    CHECK(erui::host::encode_action_inputs(value, encoded) == ERUI_OK);
    CHECK(encoded.empty());
    CHECK(public_encode_action_inputs(value, public_encoded));
    CHECK(public_encoded == encoded);
    CHECK(erui::host::decode_action_inputs(encoded, decoded) == ERUI_OK);
    CHECK(same_inputs(value, decoded));
    CHECK(public_decode_action_inputs(encoded, public_decoded));
    CHECK(same_inputs(decoded, public_decoded));

    for (std::uint32_t input = 1;
         input < ERUI_CONTROLLER_BUTTON_COUNT; ++input) {
        value = empty_inputs();
        value.controller = {ERUI_INPUT_SLOT_BOUND, input};
        CHECK(erui::host::encode_action_inputs(value, encoded) == ERUI_OK);
        CHECK(public_encode_action_inputs(value, public_encoded));
        CHECK(public_encoded == encoded);
        CHECK(erui::host::decode_action_inputs(encoded, decoded) == ERUI_OK);
        CHECK(public_decode_action_inputs(encoded, public_decoded));
        CHECK(same_inputs(value, decoded));
        CHECK(same_inputs(decoded, public_decoded));
    }
    for (std::uint32_t input = 1; input < ERUI_KEYBOARD_KEY_COUNT; ++input) {
        value = empty_inputs();
        value.keyboard = {ERUI_INPUT_SLOT_BOUND, input};
        CHECK(erui::host::encode_action_inputs(value, encoded) == ERUI_OK);
        CHECK(public_encode_action_inputs(value, public_encoded));
        CHECK(public_encoded == encoded);
        CHECK(erui::host::decode_action_inputs(encoded, decoded) == ERUI_OK);
        CHECK(public_decode_action_inputs(encoded, public_decoded));
        CHECK(same_inputs(value, decoded));
        CHECK(same_inputs(decoded, public_decoded));
    }
    for (std::uint32_t input = 1; input < ERUI_MOUSE_BUTTON_COUNT; ++input) {
        value = empty_inputs();
        value.mouse = {ERUI_INPUT_SLOT_BOUND, input};
        CHECK(erui::host::encode_action_inputs(value, encoded) == ERUI_OK);
        CHECK(public_encode_action_inputs(value, public_encoded));
        CHECK(public_encoded == encoded);
        CHECK(erui::host::decode_action_inputs(encoded, decoded) == ERUI_OK);
        CHECK(public_decode_action_inputs(encoded, public_decoded));
        CHECK(same_inputs(value, decoded));
        CHECK(same_inputs(decoded, public_decoded));
    }

    // Every sparse state shape must stay byte-for-byte compatible between the
    // public header-only codec and provider storage's production codec.
    for (ERUI_InputSlotState controller = ERUI_INPUT_SLOT_ABSENT;
         controller <= ERUI_INPUT_SLOT_BOUND; ++controller) {
        for (ERUI_InputSlotState keyboard = ERUI_INPUT_SLOT_ABSENT;
             keyboard <= ERUI_INPUT_SLOT_BOUND; ++keyboard) {
            for (ERUI_InputSlotState mouse = ERUI_INPUT_SLOT_ABSENT;
                 mouse <= ERUI_INPUT_SLOT_BOUND; ++mouse) {
                value = empty_inputs();
                value.controller = {controller,
                    controller == ERUI_INPUT_SLOT_BOUND
                        ? ERUI_CONTROLLER_BUTTON_RIGHT_TRIGGER : 0u};
                value.keyboard = {keyboard,
                    keyboard == ERUI_INPUT_SLOT_BOUND
                        ? ERUI_KEYBOARD_KEY_Q : 0u};
                value.mouse = {mouse,
                    mouse == ERUI_INPUT_SLOT_BOUND
                        ? ERUI_MOUSE_BUTTON_4 : 0u};
                CHECK(erui::host::encode_action_inputs(value, encoded) ==
                    ERUI_OK);
                CHECK(public_encode_action_inputs(value, public_encoded));
                CHECK(public_encoded == encoded);
                CHECK(erui::host::decode_action_inputs(encoded, decoded) ==
                    ERUI_OK);
                CHECK(public_decode_action_inputs(encoded, public_decoded));
                CHECK(same_inputs(decoded, public_decoded));
            }
        }
    }

    CHECK(erui::host::decode_action_inputs(
        "keyboard:f7", decoded) == ERUI_STORAGE_FORMAT_ERROR);
    CHECK(erui::host::decode_action_inputs(
        "mouse:left,mouse:right", decoded) == ERUI_STORAGE_FORMAT_ERROR);
    CHECK(erui::host::decode_action_inputs(
        "keyboard:key-q,", decoded) == ERUI_STORAGE_FORMAT_ERROR);
    CHECK(erui::host::decode_action_inputs(
        "keyboard: key-q", decoded) == ERUI_STORAGE_FORMAT_ERROR);
    return true;
}

bool test_paths() {
    HMODULE process = GetModuleHandleW(nullptr);
    CHECK(process != nullptr);

    ERUI_StorageDesc description{};
    description.size = sizeof(description);
    description.location = ERUI_STORAGE_LOCATION_PROVIDER_DEFAULT;
    std::unique_ptr<erui::host::ProviderStorage> storage{};
    CHECK(erui::host::ProviderStorage::open(
        "storage-test", process, process, &description, storage) ==
        ERUI_OK);
    CHECK(storage != nullptr);
    CHECK(storage->backing_path().filename() == L"config.ini");
    CHECK(storage->backing_path().parent_path().filename() ==
        L"storage-test");
    CHECK(storage->backing_path().parent_path().parent_path().filename() ==
        L"mods");

    // Filesystem-safe lowercase identifiers remain readable.
    CHECK(default_storage_directory_name("my-mod") == L"my-mod");

    // Anything Windows could alias, or which cannot safely be a plain
    // directory, uses a digest of its exact UTF-8 bytes in a reserved '~'
    // namespace. The known vector also independently checks the SHA-256
    // implementation rather than merely checking that names differ.
    const std::wstring lowercase = default_storage_directory_name("foo");
    const std::wstring uppercase = default_storage_directory_name("Foo");
    const std::wstring trailing_dot = default_storage_directory_name("foo.");
    CHECK(lowercase == L"foo");
    CHECK(uppercase.size() == 65 && uppercase.front() == L'~');
    CHECK(trailing_dot.size() == 65 && trailing_dot.front() == L'~');
    CHECK(lowercase != uppercase);
    CHECK(lowercase != trailing_dot);
    CHECK(uppercase != trailing_dot);
    CHECK(default_storage_directory_name("ABC") ==
        L"~b5d4045c3f466fa91fe2cc6abe79232a1a57cdf104f7a26e716e0a1e2789df78");

    const std::wstring reserved_lower =
        default_storage_directory_name("con");
    const std::wstring reserved_upper =
        default_storage_directory_name("CON");
    CHECK(reserved_lower.size() == 65 && reserved_lower.front() == L'~');
    CHECK(reserved_upper.size() == 65 && reserved_upper.front() == L'~');
    CHECK(reserved_lower != reserved_upper);

    std::string long_provider(255, 'a');
    std::string other_long_provider = long_provider;
    other_long_provider.back() = 'b';
    const std::wstring long_directory =
        default_storage_directory_name(long_provider);
    const std::wstring other_long_directory =
        default_storage_directory_name(other_long_provider);
    CHECK(long_directory.size() == 65 && long_directory.front() == L'~');
    CHECK(other_long_directory.size() == 65 &&
        other_long_directory.front() == L'~');
    CHECK(long_directory != other_long_directory);
    CHECK(default_storage_directory_name(std::string(256, 'a')).empty());

    std::wstring relative = L"settings\\my-mod.ini";
    description.location = ERUI_STORAGE_LOCATION_OWNER_MODULE_DIRECTORY;
    description.path = text(relative);
    storage.reset();
    CHECK(erui::host::ProviderStorage::open(
        "storage-test", process, process, &description, storage) ==
        ERUI_OK);
    CHECK(storage->backing_path().filename() == L"my-mod.ini");

    relative = L"..\\escape.ini";
    description.path = text(relative);
    CHECK(erui::host::ProviderStorage::open(
        "storage-test", process, process, &description, storage) ==
        ERUI_INVALID_ARGUMENT);

    wchar_t temporary[MAX_PATH]{};
    CHECK(GetTempPathW(MAX_PATH, temporary) != 0);
    relative = std::filesystem::path(temporary).append(L"absolute.ini").wstring();
    description.path = text(relative);
    CHECK(erui::host::ProviderStorage::open(
        "storage-test", process, process, &description, storage) ==
        ERUI_INVALID_ARGUMENT);

    description = {};
    description.size = sizeof(description);
    description.location = ERUI_STORAGE_LOCATION_PROVIDER_DEFAULT;
    description.reserved[1] = 1;
    CHECK(erui::host::ProviderStorage::open(
        "storage-test", process, process, &description, storage) ==
        ERUI_INVALID_ARGUMENT);
    return true;
}

bool test_document_and_batches() {
    wchar_t temporary[MAX_PATH]{};
    CHECK(GetTempPathW(MAX_PATH, temporary) != 0);
    const std::filesystem::path directory =
        std::filesystem::path(temporary) /
        (L"ERNativeUI-host-storage-test-" +
            std::to_wstring(GetCurrentProcessId()) + L"-" +
            std::to_wstring(GetTickCount64()));
    const std::filesystem::path path = directory / L"config.ini";
    std::error_code error{};
    std::filesystem::remove_all(directory, error);

    const std::wstring path_text = path.wstring();
    ERUI_StorageDesc description = explicit_storage(path_text);
    std::unique_ptr<erui::host::ProviderStorage> storage{};
    HMODULE process = GetModuleHandleW(nullptr);
    CHECK(erui::host::ProviderStorage::open(
        "storage-test", process, process, &description, storage) ==
        ERUI_OK);
    CHECK(!std::filesystem::exists(path));
    CHECK(storage->save() == ERUI_STORAGE_NOT_LOADED);

    ERUI_StorageInfo info{};
    info.size = sizeof(info);
    CHECK(storage->get_info(&info) == ERUI_OK);
    CHECK(info.loaded == 0 && info.dirty == 0);
    CHECK(storage->load() == ERUI_OK);
    CHECK(!std::filesystem::exists(path));
    CHECK(storage->load() == ERUI_STORAGE_ALREADY_LOADED);

    // Public ABI lengths are rejected before they can cause unbounded copies.
    std::string long_section(
        erui::host::kStorageMaximumSectionBytes + 1, 's');
    std::string long_key(erui::host::kStorageMaximumKeyBytes + 1, 'k');
    std::string long_value(erui::host::kStorageMaximumValueBytes + 1, 'v');
    ERUI_StorageKey oversized_key = storage_key(long_section, "key");
    ERUI_StringView short_value = bytes("value");
    CHECK(storage->set_utf8(&oversized_key, &short_value) ==
        ERUI_INVALID_ARGUMENT);
    oversized_key = storage_key("section", long_key);
    CHECK(storage->set_utf8(&oversized_key, &short_value) ==
        ERUI_INVALID_ARGUMENT);
    ERUI_StorageKey bounded_key = storage_key("section", "key");
    ERUI_StringView oversized_value = bytes(long_value);
    CHECK(storage->set_utf8(&bounded_key, &oversized_value) ==
        ERUI_INVALID_ARGUMENT);

    ERUI_StorageKey plain = storage_key("general", "name");
    const std::string tarnished = "Tarnished";
    ERUI_StringView tarnished_view = bytes(tarnished);
    CHECK(storage->set_utf8(&plain, &tarnished_view) == ERUI_OK);
    std::uint32_t required{};
    CHECK(storage->get_utf8(&plain, nullptr, 0, &required) == ERUI_OK);
    CHECK(required == tarnished.size());
    std::array<char, 3> too_small{'x', 'x', 'x'};
    CHECK(storage->get_utf8(
        &plain, too_small.data(), too_small.size(), &required) ==
        ERUI_BUFFER_TOO_SMALL);
    CHECK((too_small == std::array<char, 3>{'x', 'x', 'x'}));
    std::string read(required, '\0');
    CHECK(storage->get_utf8(
        &plain, read.data(), read.size(), &required) == ERUI_OK);
    CHECK(read == tarnished);

    ERUI_StorageKey missing = storage_key("general", "missing");
    CHECK(storage->get_utf8(&missing, nullptr, 0, &required) ==
        ERUI_NOT_FOUND);

    ERUI_ActionInputs persisted = empty_inputs();
    persisted.controller = {ERUI_INPUT_SLOT_BOUND,
        ERUI_CONTROLLER_BUTTON_RIGHT_TRIGGER};
    ERUI_StorageKey action_key = storage_key("bindings", "quick-action");
    CHECK(storage->set_action_inputs(&action_key, &persisted) == ERUI_OK);
    std::string expected_action_text{};
    CHECK(public_encode_action_inputs(persisted, expected_action_text));
    required = 0;
    CHECK(storage->get_utf8(&action_key, nullptr, 0, &required) == ERUI_OK);
    CHECK(required == expected_action_text.size());
    std::string raw_action_text(required, '\0');
    CHECK(storage->get_utf8(&action_key, raw_action_text.data(),
        static_cast<std::uint32_t>(raw_action_text.size()), &required) ==
        ERUI_OK);
    CHECK(raw_action_text == expected_action_text);
    ERUI_ActionInputs read_inputs{};
    read_inputs.size = sizeof(read_inputs);
    CHECK(storage->get_action_inputs(&action_key, &read_inputs) == ERUI_OK);
    CHECK(same_inputs(read_inputs, persisted));

    // Raw strings from a mod's own storage may list devices in any order.
    // The typed storage accessor must parse them exactly like the public
    // helper does.
    ERUI_StorageKey reordered_key = storage_key("bindings", "reordered");
    const std::string reordered_text =
        "mouse:button4,controller:right-trigger,keyboard:key-q";
    ERUI_StringView reordered_view = bytes(reordered_text);
    CHECK(storage->set_utf8(&reordered_key, &reordered_view) == ERUI_OK);
    ERUI_ActionInputs expected_reordered{};
    CHECK(public_decode_action_inputs(reordered_text, expected_reordered));
    read_inputs = {};
    read_inputs.size = sizeof(read_inputs);
    CHECK(storage->get_action_inputs(&reordered_key, &read_inputs) == ERUI_OK);
    CHECK(same_inputs(read_inputs, expected_reordered));

    ERUI_StorageKey malformed_key = storage_key("bindings", "malformed");
    const std::string malformed_text = "keyboard:key-q,keyboard:key-w";
    ERUI_StringView malformed_view = bytes(malformed_text);
    CHECK(storage->set_utf8(&malformed_key, &malformed_view) == ERUI_OK);
    read_inputs = empty_inputs();
    read_inputs.controller = {
        ERUI_INPUT_SLOT_BOUND, ERUI_CONTROLLER_BUTTON_FACE_NORTH};
    const ERUI_ActionInputs read_sentinel = read_inputs;
    CHECK(storage->get_action_inputs(&malformed_key, &read_inputs) ==
        ERUI_STORAGE_FORMAT_ERROR);
    CHECK(same_inputs(read_inputs, read_sentinel));

    ERUI_ActionInputs previous = empty_inputs();
    previous.controller = {ERUI_INPUT_SLOT_BOUND,
        ERUI_CONTROLLER_BUTTON_RIGHT_TRIGGER};
    previous.keyboard = {ERUI_INPUT_SLOT_BOUND, ERUI_KEYBOARD_KEY_Q};
    previous.mouse = {ERUI_INPUT_SLOT_BOUND, ERUI_MOUSE_BUTTON_4};
    ERUI_ActionInputs current = previous;
    current.keyboard.input = ERUI_KEYBOARD_KEY_W;
    ERUI_AssignmentChange assignment = change(
        "quick-action", previous, current,
        ERUI_ASSIGNMENT_CHANGE_PLAYER_ASSIGNMENT,
        ERUI_INPUT_DEVICE_KEYBOARD);
    const ERUI_StringView bindings = bytes("bindings");
    CHECK(storage->apply_assignment_changes(
        &bindings, &assignment, 1) == ERUI_OK);
    read_inputs = {};
    read_inputs.size = sizeof(read_inputs);
    CHECK(storage->get_action_inputs(&action_key, &read_inputs) == ERUI_OK);
    CHECK(read_inputs.controller.state == ERUI_INPUT_SLOT_BOUND);
    CHECK(read_inputs.keyboard.state == ERUI_INPUT_SLOT_BOUND);
    CHECK(read_inputs.keyboard.input == ERUI_KEYBOARD_KEY_W);
    CHECK(read_inputs.mouse.state == ERUI_INPUT_SLOT_ABSENT);
    CHECK(public_encode_action_inputs(read_inputs, expected_action_text));
    required = 0;
    CHECK(storage->get_utf8(&action_key, nullptr, 0, &required) == ERUI_OK);
    raw_action_text.assign(required, '\0');
    CHECK(storage->get_utf8(&action_key, raw_action_text.data(),
        static_cast<std::uint32_t>(raw_action_text.size()), &required) ==
        ERUI_OK);
    CHECK(raw_action_text == expected_action_text);

    previous = current;
    current.mouse = {ERUI_INPUT_SLOT_UNBOUND, ERUI_MOUSE_BUTTON_INVALID};
    ERUI_AssignmentChange cleared = change(
        "quick-action", previous, current,
        ERUI_ASSIGNMENT_CHANGE_PLAYER_CLEAR, ERUI_INPUT_DEVICE_MOUSE);
    CHECK(storage->apply_assignment_changes(&bindings, &cleared, 1) == ERUI_OK);
    read_inputs = {};
    read_inputs.size = sizeof(read_inputs);
    CHECK(storage->get_action_inputs(&action_key, &read_inputs) == ERUI_OK);
    CHECK(read_inputs.mouse.state == ERUI_INPUT_SLOT_UNBOUND);

    previous = current;
    current.controller.input = ERUI_CONTROLLER_BUTTON_LEFT_TRIGGER;
    ERUI_AssignmentChange reset_controller = change(
        "quick-action", previous, current,
        ERUI_ASSIGNMENT_CHANGE_RESET_TO_DEFAULTS,
        ERUI_INPUT_DEVICE_CONTROLLER);
    CHECK(storage->apply_assignment_changes(
        &bindings, &reset_controller, 1) == ERUI_OK);
    read_inputs = {};
    read_inputs.size = sizeof(read_inputs);
    CHECK(storage->get_action_inputs(&action_key, &read_inputs) == ERUI_OK);
    CHECK(read_inputs.controller.state == ERUI_INPUT_SLOT_ABSENT);

    // Multiple changes for one action merge by device and commit as one
    // document revision rather than one revision per internal set.
    ERUI_ActionInputs keyboard_previous = empty_inputs();
    keyboard_previous.keyboard = {
        ERUI_INPUT_SLOT_UNBOUND, ERUI_KEYBOARD_KEY_INVALID};
    ERUI_ActionInputs keyboard_current = keyboard_previous;
    keyboard_current.keyboard = {
        ERUI_INPUT_SLOT_BOUND, ERUI_KEYBOARD_KEY_A};
    ERUI_ActionInputs mouse_previous = empty_inputs();
    mouse_previous.mouse = {
        ERUI_INPUT_SLOT_UNBOUND, ERUI_MOUSE_BUTTON_INVALID};
    ERUI_ActionInputs mouse_current = mouse_previous;
    mouse_current.mouse = {
        ERUI_INPUT_SLOT_BOUND, ERUI_MOUSE_BUTTON_4};
    std::array<ERUI_AssignmentChange, 2> merged_batch{
        change("merged", keyboard_previous, keyboard_current,
            ERUI_ASSIGNMENT_CHANGE_PLAYER_ASSIGNMENT,
            ERUI_INPUT_DEVICE_KEYBOARD),
        change("merged", mouse_previous, mouse_current,
            ERUI_ASSIGNMENT_CHANGE_PLAYER_ASSIGNMENT,
            ERUI_INPUT_DEVICE_MOUSE),
    };
    ERUI_StorageInfo before_batch{};
    before_batch.size = sizeof(before_batch);
    CHECK(storage->get_info(&before_batch) == ERUI_OK);
    CHECK(storage->apply_assignment_changes(
        &bindings, merged_batch.data(), merged_batch.size()) == ERUI_OK);
    ERUI_StorageInfo after_batch{};
    after_batch.size = sizeof(after_batch);
    CHECK(storage->get_info(&after_batch) == ERUI_OK);
    CHECK(after_batch.current_revision == before_batch.current_revision + 1);
    ERUI_StorageKey merged_key = storage_key("bindings", "merged");
    read_inputs = {};
    read_inputs.size = sizeof(read_inputs);
    CHECK(storage->get_action_inputs(&merged_key, &read_inputs) == ERUI_OK);
    CHECK(read_inputs.keyboard.state == ERUI_INPUT_SLOT_BOUND);
    CHECK(read_inputs.keyboard.input == ERUI_KEYBOARD_KEY_A);
    CHECK(read_inputs.mouse.state == ERUI_INPUT_SLOT_BOUND);
    CHECK(read_inputs.mouse.input == ERUI_MOUSE_BUTTON_4);

    std::vector<ERUI_AssignmentChange> excessive_changes(
        erui::host::kStorageMaximumEntries + 1, merged_batch.front());
    CHECK(storage->apply_assignment_changes(
        &bindings,
        excessive_changes.data(),
        static_cast<std::uint32_t>(excessive_changes.size())) ==
        ERUI_INVALID_ARGUMENT);
    ERUI_StorageInfo after_rejected_count{};
    after_rejected_count.size = sizeof(after_rejected_count);
    CHECK(storage->get_info(&after_rejected_count) == ERUI_OK);
    CHECK(after_rejected_count.current_revision ==
        after_batch.current_revision);

    // The invalid second element rejects the complete batch before the first
    // element can change the document.
    ERUI_StorageKey untouched_key = storage_key("bindings", "untouched");
    ERUI_ActionInputs untouched = empty_inputs();
    untouched.keyboard = {ERUI_INPUT_SLOT_BOUND, ERUI_KEYBOARD_KEY_A};
    CHECK(storage->set_action_inputs(&untouched_key, &untouched) == ERUI_OK);
    ERUI_ActionInputs altered = untouched;
    altered.keyboard.input = ERUI_KEYBOARD_KEY_B;
    std::array<ERUI_AssignmentChange, 2> invalid_batch{
        change("untouched", untouched, altered,
            ERUI_ASSIGNMENT_CHANGE_PLAYER_ASSIGNMENT,
            ERUI_INPUT_DEVICE_KEYBOARD),
        change("other", untouched, altered,
            ERUI_ASSIGNMENT_CHANGE_PLAYER_ASSIGNMENT,
            ERUI_INPUT_DEVICE_KEYBOARD),
    };
    invalid_batch[1].flags = 1;
    ERUI_StorageInfo before_invalid{};
    before_invalid.size = sizeof(before_invalid);
    CHECK(storage->get_info(&before_invalid) == ERUI_OK);
    CHECK(storage->apply_assignment_changes(
        &bindings, invalid_batch.data(), invalid_batch.size()) ==
        ERUI_INVALID_ARGUMENT);
    read_inputs = {};
    read_inputs.size = sizeof(read_inputs);
    CHECK(storage->get_action_inputs(&untouched_key, &read_inputs) == ERUI_OK);
    CHECK(same_inputs(read_inputs, untouched));
    ERUI_StorageInfo after_invalid{};
    after_invalid.size = sizeof(after_invalid);
    CHECK(storage->get_info(&after_invalid) == ERUI_OK);
    CHECK(after_invalid.current_revision == before_invalid.current_revision);
    CHECK(after_invalid.last_saved_revision ==
        before_invalid.last_saved_revision);
    CHECK(after_invalid.dirty == before_invalid.dirty);

    CHECK(storage->get_info(&info) == ERUI_OK);
    CHECK(info.loaded == 1 && info.dirty == 1);
    CHECK(storage->save() == ERUI_OK);
    CHECK(std::filesystem::exists(path));
    CHECK(storage->get_info(&info) == ERUI_OK);
    CHECK(info.dirty == 0 && info.current_revision == info.last_saved_revision);

    storage.reset();
    CHECK(erui::host::ProviderStorage::open(
        "storage-test", process, process, &description, storage) ==
        ERUI_OK);
    CHECK(storage->load() == ERUI_OK);
    required = 0;
    CHECK(storage->get_utf8(&plain, nullptr, 0, &required) == ERUI_OK);
    read.assign(required, '\0');
    CHECK(storage->get_utf8(
        &plain, read.data(), read.size(), &required) == ERUI_OK);
    CHECK(read == tarnished);
    read_inputs = {};
    read_inputs.size = sizeof(read_inputs);
    CHECK(storage->get_action_inputs(&untouched_key, &read_inputs) == ERUI_OK);
    CHECK(same_inputs(read_inputs, untouched));

    storage.reset();
    std::filesystem::remove_all(directory, error);
    return true;
}

} // namespace

int main() {
    if (!test_codec() || !test_paths() || !test_document_and_batches()) {
        return 1;
    }
    return 0;
}
