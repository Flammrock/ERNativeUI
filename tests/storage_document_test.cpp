#include "storage_document.hpp"

#include "test_assertions.hpp"

#include <Windows.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {

using erui::host::StorageDocument;
using erui::host::StorageDocumentResult;
using erui::host::StorageMutation;
using erui::host::StorageMutationKind;

class TemporaryDirectory final {
public:
    TemporaryDirectory() {
        static std::atomic_uint32_t sequence{};
        path_ = std::filesystem::temp_directory_path() /
            (L"ERNativeUI-storage-document-tests-" +
                std::to_wstring(GetCurrentProcessId()) + L"-" +
                std::to_wstring(sequence.fetch_add(
                    1, std::memory_order_relaxed)));
        std::error_code error{};
        std::filesystem::remove_all(path_, error);
        error.clear();
        ERUI_TEST_CHECK(std::filesystem::create_directories(path_, error));
        ERUI_TEST_CHECK(!error);
    }

    ~TemporaryDirectory() {
        std::error_code error{};
        std::filesystem::remove_all(path_, error);
    }

    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

    const std::filesystem::path& path() const noexcept { return path_; }

private:
    std::filesystem::path path_{};
};

void write_bytes(const std::filesystem::path& path, std::string_view bytes) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    ERUI_TEST_CHECK(stream.good());
    stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    stream.flush();
    ERUI_TEST_CHECK(stream.good());
}

std::string read_bytes(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    ERUI_TEST_CHECK(stream.good());
    return {
        std::istreambuf_iterator<char>(stream),
        std::istreambuf_iterator<char>()};
}

std::string value_of(
    const StorageDocument& document,
    std::string_view section,
    std::string_view key) {
    std::string value{"unchanged"};
    bool found = false;
    ERUI_TEST_CHECK(
        document.get(section, key, value, found) == StorageDocumentResult::ok);
    ERUI_TEST_CHECK(found);
    return value;
}

void test_explicit_lifecycle_and_revisions() {
    TemporaryDirectory temporary{};
    const auto path = temporary.path() / L"nested" / L"config.ini";

    {
        StorageDocument document(path);
        std::string output{"preserved"};
        bool found = true;
        bool erased = true;
        ERUI_TEST_CHECK(document.get("main", "name", output, found) ==
            StorageDocumentResult::not_loaded);
        ERUI_TEST_CHECK(!found);
        ERUI_TEST_CHECK(output == "preserved");
        ERUI_TEST_CHECK(document.set("main", "name", "value") ==
            StorageDocumentResult::not_loaded);
        ERUI_TEST_CHECK(document.erase("main", "name", &erased) ==
            StorageDocumentResult::not_loaded);
        ERUI_TEST_CHECK(erased);
        ERUI_TEST_CHECK(document.save() == StorageDocumentResult::not_loaded);

        ERUI_TEST_CHECK(document.load() == StorageDocumentResult::ok);
        ERUI_TEST_CHECK(document.is_loaded());
        ERUI_TEST_CHECK(!document.is_dirty());
        ERUI_TEST_CHECK(document.entry_count() == 0);
        ERUI_TEST_CHECK(document.current_revision() == 1);
        ERUI_TEST_CHECK(document.last_saved_revision() == 1);
        ERUI_TEST_CHECK(!std::filesystem::exists(path));

        output = "preserved";
        found = true;
        ERUI_TEST_CHECK(document.get("main", "missing", output, found) ==
            StorageDocumentResult::ok);
        ERUI_TEST_CHECK(!found);
        ERUI_TEST_CHECK(output == "preserved");

        ERUI_TEST_CHECK(document.set("main", "name", "value") ==
            StorageDocumentResult::ok);
        ERUI_TEST_CHECK(document.is_dirty());
        ERUI_TEST_CHECK(document.entry_count() == 1);
        ERUI_TEST_CHECK(document.current_revision() == 2);
        ERUI_TEST_CHECK(document.last_saved_revision() == 1);
        ERUI_TEST_CHECK(value_of(document, "main", "name") == "value");
        ERUI_TEST_CHECK(!std::filesystem::exists(path));

        const std::uint64_t unchanged = document.current_revision();
        ERUI_TEST_CHECK(document.set("main", "name", "value") ==
            StorageDocumentResult::ok);
        ERUI_TEST_CHECK(document.current_revision() == unchanged);
        erased = true;
        ERUI_TEST_CHECK(document.erase("main", "missing", &erased) ==
            StorageDocumentResult::ok);
        ERUI_TEST_CHECK(!erased);
        ERUI_TEST_CHECK(document.current_revision() == unchanged);
    }

    // Destruction does not save an explicitly dirty document.
    ERUI_TEST_CHECK(!std::filesystem::exists(path));
}

void populate_in_first_order(StorageDocument& document) {
    ERUI_TEST_CHECK(document.set(
        "bindings",
        "quick-action",
        "controller:right-trigger,keyboard:key-q,mouse:button4") ==
        StorageDocumentResult::ok);
    ERUI_TEST_CHECK(document.set("settings", "enabled", "true") ==
        StorageDocumentResult::ok);
    ERUI_TEST_CHECK(document.set(
        "User Preferences", "display.name", "  Tarnished = #1; ready \xE2\x82\xAC  ") ==
        StorageDocumentResult::ok);
    ERUI_TEST_CHECK(document.set("settings", "empty-value", "") ==
        StorageDocumentResult::ok);
}

void populate_in_reverse_order(StorageDocument& document) {
    ERUI_TEST_CHECK(document.set("settings", "empty-value", "") ==
        StorageDocumentResult::ok);
    ERUI_TEST_CHECK(document.set(
        "User Preferences", "display.name", "  Tarnished = #1; ready \xE2\x82\xAC  ") ==
        StorageDocumentResult::ok);
    ERUI_TEST_CHECK(document.set("settings", "enabled", "true") ==
        StorageDocumentResult::ok);
    ERUI_TEST_CHECK(document.set(
        "bindings",
        "quick-action",
        "controller:right-trigger,keyboard:key-q,mouse:button4") ==
        StorageDocumentResult::ok);
}

void test_deterministic_human_readable_round_trip() {
    TemporaryDirectory temporary{};
    const auto first_path = temporary.path() / L"first.ini";
    const auto second_path = temporary.path() / L"second.ini";

    StorageDocument first(first_path);
    StorageDocument second(second_path);
    ERUI_TEST_CHECK(first.load() == StorageDocumentResult::ok);
    ERUI_TEST_CHECK(second.load() == StorageDocumentResult::ok);
    populate_in_first_order(first);
    populate_in_reverse_order(second);
    ERUI_TEST_CHECK(first.entry_count() == 4);
    ERUI_TEST_CHECK(second.entry_count() == 4);

    const std::uint64_t first_revision = first.current_revision();
    ERUI_TEST_CHECK(first.save() == StorageDocumentResult::ok);
    ERUI_TEST_CHECK(second.save() == StorageDocumentResult::ok);
    ERUI_TEST_CHECK(!first.is_dirty());
    ERUI_TEST_CHECK(first.last_saved_revision() == first_revision);

    const std::string first_bytes = read_bytes(first_path);
    ERUI_TEST_CHECK(first_bytes == read_bytes(second_path));
    ERUI_TEST_CHECK(first_bytes.starts_with(
        "; ERNativeUI provider storage\r\n"));
    ERUI_TEST_CHECK(first_bytes.find("; schema =") == std::string::npos);
    ERUI_TEST_CHECK(first_bytes.find(
        "[bindings]\r\n"
        "quick-action=controller:right-trigger,keyboard:key-q,mouse:button4\r\n") !=
        std::string::npos);
    ERUI_TEST_CHECK(first_bytes.find(
        "[settings]\r\n"
        "empty-value=\r\n"
        "enabled=true\r\n") != std::string::npos);
    ERUI_TEST_CHECK(first_bytes.find(
        "display.name=  Tarnished = #1; ready \xE2\x82\xAC  \r\n") !=
        std::string::npos);
    ERUI_TEST_CHECK(first_bytes.find("[S:") == std::string::npos);
    ERUI_TEST_CHECK(first_bytes.find("K:") == std::string::npos);

    // Saving an unchanged document is a successful no-op.
    const auto write_time = std::filesystem::last_write_time(first_path);
    ERUI_TEST_CHECK(first.save() == StorageDocumentResult::ok);
    ERUI_TEST_CHECK(std::filesystem::last_write_time(first_path) == write_time);

    StorageDocument reloaded(first_path);
    ERUI_TEST_CHECK(reloaded.load() == StorageDocumentResult::ok);
    ERUI_TEST_CHECK(reloaded.entry_count() == 4);
    ERUI_TEST_CHECK(value_of(reloaded, "settings", "enabled") == "true");
    ERUI_TEST_CHECK(value_of(reloaded, "settings", "empty-value").empty());
    ERUI_TEST_CHECK(value_of(
        reloaded, "User Preferences", "display.name") ==
        "  Tarnished = #1; ready \xE2\x82\xAC  ");
}

void test_strict_parser_and_failed_load_are_atomic() {
    TemporaryDirectory temporary{};
    const auto path = temporary.path() / L"config.ini";
    StorageDocument document(path);
    ERUI_TEST_CHECK(document.load() == StorageDocumentResult::ok);
    ERUI_TEST_CHECK(document.set("stable", "key", "disk") ==
        StorageDocumentResult::ok);
    ERUI_TEST_CHECK(document.save() == StorageDocumentResult::ok);

    const std::uint64_t stable_revision = document.current_revision();
    write_bytes(path,
        "; ordinary comments are ignored\r\n"
        "[stable]\r\nkey=first\r\nkey=duplicate\r\n");
    ERUI_TEST_CHECK(document.load() == StorageDocumentResult::format_error);
    ERUI_TEST_CHECK(document.current_revision() == stable_revision);
    ERUI_TEST_CHECK(document.last_saved_revision() == stable_revision);
    ERUI_TEST_CHECK(!document.is_dirty());
    ERUI_TEST_CHECK(value_of(document, "stable", "key") == "disk");

    write_bytes(path,
        "[stable]\r\nkey=\xC0\xAF\r\n");
    ERUI_TEST_CHECK(document.load() == StorageDocumentResult::format_error);
    ERUI_TEST_CHECK(value_of(document, "stable", "key") == "disk");

    write_bytes(path, "[bad/section]\r\nkey=value\r\n");
    ERUI_TEST_CHECK(document.load() == StorageDocumentResult::format_error);
    ERUI_TEST_CHECK(value_of(document, "stable", "key") == "disk");

    write_bytes(path, "key=outside-a-section\r\n");
    ERUI_TEST_CHECK(document.load() == StorageDocumentResult::format_error);
    ERUI_TEST_CHECK(value_of(document, "stable", "key") == "disk");

    ERUI_TEST_CHECK(document.set("stable", "key", "memory") ==
        StorageDocumentResult::ok);
    const std::uint64_t dirty_revision = document.current_revision();
    std::string oversized(
        erui::host::kStorageMaximumFileBytes + 1, 'x');
    write_bytes(path, oversized);
    ERUI_TEST_CHECK(document.load() == StorageDocumentResult::limit_exceeded);
    ERUI_TEST_CHECK(document.is_dirty());
    ERUI_TEST_CHECK(document.current_revision() == dirty_revision);
    ERUI_TEST_CHECK(document.last_saved_revision() == stable_revision);
    ERUI_TEST_CHECK(value_of(document, "stable", "key") == "memory");

    const std::string invalid_utf8{"\xED\xA0\x80", 3}; // UTF-8 surrogate
    ERUI_TEST_CHECK(document.set("stable", "bad", invalid_utf8) ==
        StorageDocumentResult::invalid_argument);
    ERUI_TEST_CHECK(document.current_revision() == dirty_revision);
}

void test_conventional_parser_whitespace_comments_and_exact_values() {
    TemporaryDirectory temporary{};
    const auto path = temporary.path() / L"config.ini";
    write_bytes(path,
        "\xEF\xBB\xBF"
        "  ; leading whitespace before a comment\n"
        "\t# another comment\r\n"
        "\r\n"
        "  [ User Preferences ] \r\n"
        "\t display.name \t=  Tarnished = #1; ready  \r\n"
        "empty=\r\n"
        "[bindings]\n"
        "quick-action=controller:right-trigger,keyboard:key-q,mouse:button4\n");

    StorageDocument document(path);
    ERUI_TEST_CHECK(document.load() == StorageDocumentResult::ok);
    ERUI_TEST_CHECK(document.entry_count() == 3);
    ERUI_TEST_CHECK(value_of(
        document, "User Preferences", "display.name") ==
        "  Tarnished = #1; ready  ");
    ERUI_TEST_CHECK(value_of(document, "User Preferences", "empty").empty());
    ERUI_TEST_CHECK(value_of(document, "bindings", "quick-action") ==
        "controller:right-trigger,keyboard:key-q,mouse:button4");

    // A save after a real mutation normalizes ordering and syntax while
    // preserving value bytes exactly.
    ERUI_TEST_CHECK(document.set("settings", "enabled", "true") ==
        StorageDocumentResult::ok);
    ERUI_TEST_CHECK(document.save() == StorageDocumentResult::ok);
    const std::string bytes = read_bytes(path);
    ERUI_TEST_CHECK(bytes.find("[User Preferences]\r\n") !=
        std::string::npos);
    ERUI_TEST_CHECK(bytes.find(
        "display.name=  Tarnished = #1; ready  \r\n") !=
        std::string::npos);
}

void test_erase_and_reload() {
    TemporaryDirectory temporary{};
    const auto path = temporary.path() / L"config.ini";
    StorageDocument document(path);
    ERUI_TEST_CHECK(document.load() == StorageDocumentResult::ok);
    ERUI_TEST_CHECK(document.set("one", "first", "1") ==
        StorageDocumentResult::ok);
    ERUI_TEST_CHECK(document.set("one", "second", "2") ==
        StorageDocumentResult::ok);
    ERUI_TEST_CHECK(document.set("two", "first", "3") ==
        StorageDocumentResult::ok);

    bool erased = false;
    ERUI_TEST_CHECK(document.erase("one", "first", &erased) ==
        StorageDocumentResult::ok);
    ERUI_TEST_CHECK(erased);
    ERUI_TEST_CHECK(document.entry_count() == 2);
    ERUI_TEST_CHECK(document.erase("one", "second", &erased) ==
        StorageDocumentResult::ok);
    ERUI_TEST_CHECK(erased);
    ERUI_TEST_CHECK(document.entry_count() == 1);
    ERUI_TEST_CHECK(document.save() == StorageDocumentResult::ok);

    StorageDocument reloaded(path);
    ERUI_TEST_CHECK(reloaded.load() == StorageDocumentResult::ok);
    ERUI_TEST_CHECK(reloaded.entry_count() == 1);
    ERUI_TEST_CHECK(value_of(reloaded, "two", "first") == "3");
    std::string output{};
    bool found = true;
    ERUI_TEST_CHECK(reloaded.get("one", "second", output, found) ==
        StorageDocumentResult::ok);
    ERUI_TEST_CHECK(!found);
}

void test_failed_save_preserves_memory_and_dirty_state() {
    TemporaryDirectory temporary{};
    const auto path = temporary.path() / L"blocked.ini";
    StorageDocument document(path);
    ERUI_TEST_CHECK(document.load() == StorageDocumentResult::ok);
    ERUI_TEST_CHECK(document.set("main", "key", "value") ==
        StorageDocumentResult::ok);
    const std::uint64_t revision = document.current_revision();
    const std::uint64_t last_saved = document.last_saved_revision();

    std::error_code error{};
    ERUI_TEST_CHECK(std::filesystem::create_directory(path, error));
    ERUI_TEST_CHECK(!error);
    ERUI_TEST_CHECK(document.save() == StorageDocumentResult::io_error);
    ERUI_TEST_CHECK(document.is_dirty());
    ERUI_TEST_CHECK(document.current_revision() == revision);
    ERUI_TEST_CHECK(document.last_saved_revision() == last_saved);
    ERUI_TEST_CHECK(value_of(document, "main", "key") == "value");

    ERUI_TEST_CHECK(std::filesystem::remove(path, error));
    ERUI_TEST_CHECK(!error);
    ERUI_TEST_CHECK(document.save() == StorageDocumentResult::ok);
    ERUI_TEST_CHECK(!document.is_dirty());
    ERUI_TEST_CHECK(document.last_saved_revision() == revision);
    ERUI_TEST_CHECK(std::filesystem::is_regular_file(path));

    for (const auto& entry : std::filesystem::directory_iterator(
             temporary.path())) {
        ERUI_TEST_CHECK(
            entry.path().filename().wstring().find(L".tmp.") ==
            std::wstring::npos);
    }
}

void test_concurrent_mutations_preserve_revisions() {
    TemporaryDirectory temporary{};
    const auto path = temporary.path() / L"config.ini";
    StorageDocument document(path);
    ERUI_TEST_CHECK(document.load() == StorageDocumentResult::ok);

    constexpr std::size_t kWorkerCount = 4;
    constexpr std::size_t kEntriesPerWorker = 100;
    std::atomic_bool succeeded{true};
    std::array<std::thread, kWorkerCount> workers{};
    for (std::size_t worker = 0; worker < workers.size(); ++worker) {
        workers[worker] = std::thread([&, worker] {
            const std::string section = "worker-" + std::to_string(worker);
            for (std::size_t entry = 0; entry < kEntriesPerWorker; ++entry) {
                const std::string key = "key-" + std::to_string(entry);
                const std::string value = "value-" + std::to_string(entry);
                if (document.set(section, key, value) !=
                    StorageDocumentResult::ok) {
                    succeeded.store(false, std::memory_order_relaxed);
                    return;
                }
            }
        });
    }
    for (std::thread& worker : workers) worker.join();

    ERUI_TEST_CHECK(succeeded.load(std::memory_order_relaxed));
    ERUI_TEST_CHECK(document.entry_count() ==
        kWorkerCount * kEntriesPerWorker);
    ERUI_TEST_CHECK(document.current_revision() ==
        1 + kWorkerCount * kEntriesPerWorker);
    ERUI_TEST_CHECK(document.is_dirty());
    ERUI_TEST_CHECK(document.save() == StorageDocumentResult::ok);

    StorageDocument reloaded(path);
    ERUI_TEST_CHECK(reloaded.load() == StorageDocumentResult::ok);
    ERUI_TEST_CHECK(reloaded.entry_count() ==
        kWorkerCount * kEntriesPerWorker);
    ERUI_TEST_CHECK(value_of(reloaded, "worker-3", "key-99") == "value-99");
}

void test_failure_atomic_batches_and_single_revision_commit() {
    TemporaryDirectory temporary{};
    const auto path = temporary.path() / L"config.ini";
    StorageDocument document(path);
    ERUI_TEST_CHECK(document.load() == StorageDocumentResult::ok);
    ERUI_TEST_CHECK(document.set("bindings", "stable", "original") ==
        StorageDocumentResult::ok);
    ERUI_TEST_CHECK(document.set("bindings", "remove-me", "present") ==
        StorageDocumentResult::ok);
    ERUI_TEST_CHECK(document.save() == StorageDocumentResult::ok);

    const std::uint64_t saved_revision = document.current_revision();
    ERUI_TEST_CHECK(!document.is_dirty());
    ERUI_TEST_CHECK(document.last_saved_revision() == saved_revision);

    // A late invalid operation cannot expose the earlier valid set or alter
    // any transaction metadata.
    std::vector<StorageMutation> invalid{
        {StorageMutationKind::set, "stable", "changed"},
        {StorageMutationKind::set, "bad/key", "invalid"},
    };
    ERUI_TEST_CHECK(document.apply_batch("bindings", invalid) ==
        StorageDocumentResult::invalid_argument);
    ERUI_TEST_CHECK(value_of(document, "bindings", "stable") == "original");
    ERUI_TEST_CHECK(document.entry_count() == 2);
    ERUI_TEST_CHECK(document.current_revision() == saved_revision);
    ERUI_TEST_CHECK(document.last_saved_revision() == saved_revision);
    ERUI_TEST_CHECK(!document.is_dirty());

    // This batch is individually valid but its final state exceeds the
    // document entry limit. The copy is discarded without observable change.
    std::vector<StorageMutation> excessive{};
    excessive.reserve(erui::host::kStorageMaximumEntries + 1);
    for (std::size_t index = 0;
         index < erui::host::kStorageMaximumEntries + 1;
         ++index) {
        excessive.push_back({
            StorageMutationKind::set,
            "extra-" + std::to_string(index),
            "value",
        });
    }
    ERUI_TEST_CHECK(document.apply_batch("overflow", excessive) ==
        StorageDocumentResult::limit_exceeded);
    ERUI_TEST_CHECK(value_of(document, "bindings", "stable") == "original");
    ERUI_TEST_CHECK(document.entry_count() == 2);
    ERUI_TEST_CHECK(document.current_revision() == saved_revision);
    ERUI_TEST_CHECK(document.last_saved_revision() == saved_revision);
    ERUI_TEST_CHECK(!document.is_dirty());

    // Multiple sets, an erase, and duplicate keys become one visible commit.
    // The duplicate key uses ordered, last-operation-wins semantics.
    std::vector<StorageMutation> valid{
        {StorageMutationKind::set, "stable", "temporary"},
        {StorageMutationKind::set, "added", "first"},
        {StorageMutationKind::erase, "remove-me", {}},
        {StorageMutationKind::set, "stable", "final"},
        {StorageMutationKind::set, "added", "last"},
    };
    ERUI_TEST_CHECK(document.apply_batch("bindings", valid) ==
        StorageDocumentResult::ok);
    ERUI_TEST_CHECK(value_of(document, "bindings", "stable") == "final");
    ERUI_TEST_CHECK(value_of(document, "bindings", "added") == "last");
    std::string missing_output{"preserved"};
    bool found = true;
    ERUI_TEST_CHECK(document.get(
        "bindings", "remove-me", missing_output, found) ==
        StorageDocumentResult::ok);
    ERUI_TEST_CHECK(!found && missing_output == "preserved");
    ERUI_TEST_CHECK(document.entry_count() == 2);
    ERUI_TEST_CHECK(document.current_revision() == saved_revision + 1);
    ERUI_TEST_CHECK(document.last_saved_revision() == saved_revision);
    ERUI_TEST_CHECK(document.is_dirty());

    // A non-empty transaction whose final state is identical is a true no-op.
    const std::uint64_t committed_revision = document.current_revision();
    std::vector<StorageMutation> no_op{
        {StorageMutationKind::set, "stable", "intermediate"},
        {StorageMutationKind::set, "stable", "final"},
        {StorageMutationKind::erase, "missing", {}},
    };
    ERUI_TEST_CHECK(document.apply_batch("bindings", no_op) ==
        StorageDocumentResult::ok);
    ERUI_TEST_CHECK(document.current_revision() == committed_revision);
    ERUI_TEST_CHECK(value_of(document, "bindings", "stable") == "final");
}

void test_bounds_and_empty_path() {
    TemporaryDirectory temporary{};
    StorageDocument document(temporary.path() / L"config.ini");
    ERUI_TEST_CHECK(document.load() == StorageDocumentResult::ok);
    const std::uint64_t revision = document.current_revision();

    std::string long_section(
        erui::host::kStorageMaximumSectionBytes + 1, 's');
    std::string long_key(erui::host::kStorageMaximumKeyBytes + 1, 'k');
    std::string long_value(erui::host::kStorageMaximumValueBytes + 1, 'v');
    ERUI_TEST_CHECK(document.set(long_section, "key", "value") ==
        StorageDocumentResult::limit_exceeded);
    ERUI_TEST_CHECK(document.set("section", long_key, "value") ==
        StorageDocumentResult::limit_exceeded);
    ERUI_TEST_CHECK(document.set("section", "key", long_value) ==
        StorageDocumentResult::limit_exceeded);
    ERUI_TEST_CHECK(document.current_revision() == revision);

    const std::vector<std::string> invalid_names{
        "", " leading", "trailing ", ".", "..", "has/slash",
        "has\\slash", "has[bracket", "has=equals", "has;comment",
        "has#comment", "has\ttab", "non-ascii-\xC3\xA9"};
    for (const std::string& name : invalid_names) {
        ERUI_TEST_CHECK(document.set(name, "key", "value") ==
            StorageDocumentResult::invalid_argument);
        ERUI_TEST_CHECK(document.set("section", name, "value") ==
            StorageDocumentResult::invalid_argument);
    }

    std::string nul_value{"before\0after", 12};
    ERUI_TEST_CHECK(document.set("section", "key", nul_value) ==
        StorageDocumentResult::invalid_argument);
    ERUI_TEST_CHECK(document.set("section", "key", "line\rbreak") ==
        StorageDocumentResult::invalid_argument);
    ERUI_TEST_CHECK(document.set("section", "key", "line\nbreak") ==
        StorageDocumentResult::invalid_argument);
    ERUI_TEST_CHECK(document.current_revision() == revision);

    StorageDocument empty_path({});
    ERUI_TEST_CHECK(empty_path.load() ==
        StorageDocumentResult::invalid_argument);
    ERUI_TEST_CHECK(empty_path.save() ==
        StorageDocumentResult::invalid_argument);
}

} // namespace

int main() {
    test_explicit_lifecycle_and_revisions();
    test_deterministic_human_readable_round_trip();
    test_strict_parser_and_failed_load_are_atomic();
    test_conventional_parser_whitespace_comments_and_exact_values();
    test_erase_and_reload();
    test_failed_save_preserves_memory_and_dirty_state();
    test_concurrent_mutations_preserve_revisions();
    test_failure_atomic_batches_and_single_revision_commit();
    test_bounds_and_empty_path();
    return 0;
}
