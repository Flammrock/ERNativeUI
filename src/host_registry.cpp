#include "host_registry.hpp"

#include "native_dialog.hpp"
#include "native_dialog_presentation.hpp"
#include "runtime_log.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>

namespace erui::host {
namespace {

constexpr std::size_t kMaxProviders = 256;
constexpr std::size_t kMaxPagesPerProvider = 256;
constexpr std::size_t kMaxRowsPerProvider = 4096;
constexpr std::size_t kMaxIdentifierBytes = 255;
constexpr std::size_t kMaxTextUnits = 4096;

std::atomic<ApiState> g_api_state{ApiState::initializing};

bool field_available(std::uint32_t size, std::size_t end) noexcept {
    return static_cast<std::size_t>(size) >= end;
}

#define ERUI_FIELD_END(type, field) \
    (offsetof(type, field) + sizeof(((type*)0)->field))

std::string copy_string(ERUI_StringView view) {
    if (view.length == 0 || view.length > kMaxIdentifierBytes || !view.data) {
        throw std::invalid_argument("invalid provider identifier");
    }
    if (std::memchr(view.data, '\0', view.length) != nullptr) {
        throw std::invalid_argument("provider identifier contains NUL");
    }
    return {view.data, view.data + view.length};
}

std::wstring copy_utf16(ERUI_Utf16View view, bool allow_empty = true) {
    static_assert(sizeof(wchar_t) == sizeof(std::uint16_t));
    if (view.length == 0) {
        if (allow_empty) return {};
        throw std::invalid_argument("required text is empty");
    }
    if (!view.data || view.length > kMaxTextUnits) {
        throw std::invalid_argument("invalid UTF-16 text");
    }
    std::wstring result(view.length, L'\0');
    std::memcpy(result.data(), view.data,
        static_cast<std::size_t>(view.length) * sizeof(std::uint16_t));
    if (std::find(result.begin(), result.end(), L'\0') != result.end()) {
        throw std::invalid_argument("UTF-16 text contains NUL");
    }
    for (std::size_t index = 0; index < result.size(); ++index) {
        const std::uint16_t unit = static_cast<std::uint16_t>(result[index]);
        if (unit >= 0xD800u && unit <= 0xDBFFu) {
            if (++index >= result.size()) {
                throw std::invalid_argument("UTF-16 text has an unpaired surrogate");
            }
            const std::uint16_t low = static_cast<std::uint16_t>(result[index]);
            if (low < 0xDC00u || low > 0xDFFFu) {
                throw std::invalid_argument("UTF-16 text has an unpaired surrogate");
            }
        } else if (unit >= 0xDC00u && unit <= 0xDFFFu) {
            throw std::invalid_argument("UTF-16 text has an unpaired surrogate");
        }
    }
    return result;
}

bool valid_enabled(std::uint32_t enabled) noexcept {
    return enabled == 0 || enabled == 1;
}

bool valid_view(ERUI_StringView view) noexcept {
    return view.reserved == 0;
}

bool valid_view(ERUI_Utf16View view) noexcept {
    return view.reserved == 0;
}

bool address_belongs_to_module(const void* address, HMODULE module) noexcept {
    if (!address || !module) return false;
    MEMORY_BASIC_INFORMATION memory{};
    return VirtualQuery(address, &memory, sizeof(memory)) == sizeof(memory) &&
        memory.AllocationBase == module;
}

void invoke_button_callback(ERUI_ButtonCallback callback, void* user_data) noexcept {
    if (!callback) return;
#if defined(_MSC_VER)
    __try {
        callback(user_data);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        erui::detail::logf(erui::LogLevel::error,
            "Provider button callback raised SEH exception 0x%08lX",
            GetExceptionCode());
    }
#else
    callback(user_data);
#endif
}

void invoke_value_callback(
    ERUI_ValueChangedCallback callback,
    void* user_data,
    std::uint8_t value) noexcept {
    if (!callback) return;
#if defined(_MSC_VER)
    __try {
        callback(user_data, value);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        erui::detail::logf(erui::LogLevel::error,
            "Provider value callback raised SEH exception 0x%08lX",
            GetExceptionCode());
    }
#else
    callback(user_data, value);
#endif
}

ERUI_Result invoke_page_title_formatter(
    ERUI_PageTitleFormatter formatter,
    void* user_data,
    const ERUI_PageTitleFormatContext* context,
    std::uint16_t* output,
    std::uint32_t output_capacity,
    std::uint32_t* out_length) noexcept {
    if (!formatter) return ERUI_INVALID_ARGUMENT;
#if defined(_MSC_VER)
    __try {
        return formatter(
            user_data, context, output, output_capacity, out_length);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return ERUI_CALLBACK_REJECTED;
    }
#else
    try {
        return formatter(
            user_data, context, output, output_capacity, out_length);
    } catch (...) {
        return ERUI_CALLBACK_REJECTED;
    }
#endif
}

} // namespace

Registry::Registry() = default;
Registry::~Registry() = default;

void Registry::set_host_locale(erui::MenuLocalization pagination) {
    std::lock_guard lock(mutex_);
    if (open_ || !providers_.empty()) {
        throw std::logic_error("host locale must be set before registration");
    }
    pagination_locale_ = std::move(pagination);
}

Registry& registry() noexcept {
    static Registry instance;
    return instance;
}

void set_api_state(ApiState state) noexcept {
    g_api_state.store(state, std::memory_order_release);
}

ApiState api_state() noexcept {
    return g_api_state.load(std::memory_order_acquire);
}

void Registry::open_registration() noexcept {
    std::lock_guard lock(mutex_);
    open_ = true;
    note_activity_locked();
}

bool Registry::registration_open() const noexcept {
    std::lock_guard lock(mutex_);
    return open_;
}

std::uint64_t Registry::generation() const noexcept {
    std::lock_guard lock(mutex_);
    return generation_;
}

std::size_t Registry::committed_provider_count() const noexcept {
    std::lock_guard lock(mutex_);
    return static_cast<std::size_t>(std::count_if(
        providers_.begin(), providers_.end(),
        [](const auto& item) { return item.second->committed; }));
}

std::size_t Registry::committed_root_row_count() const noexcept {
    std::lock_guard lock(mutex_);
    std::size_t result = 0;
    for (const auto& item : providers_) {
        const Provider& provider = *item.second;
        if (!provider.committed) continue;
        const auto found = provider.page_lookup.find(provider.root_page);
        if (found != provider.page_lookup.end()) result += found->second->rows.size();
    }
    return result;
}

Registry::Page& Registry::create_page_locked(
    Provider& provider,
    std::wstring title,
    std::wstring help) {
    if (provider.pages.size() >= kMaxPagesPerProvider) {
        throw std::length_error("provider page limit exceeded");
    }
    auto page = std::make_unique<Page>();
    page->handle = next_handle_++;
    page->provider = provider.handle;
    page->title = std::move(title);
    page->help = std::move(help);
    Page* result = page.get();
    provider.pages.push_back(std::move(page));
    try {
        const bool inserted = provider.page_lookup.emplace(
            result->handle, result).second;
        if (!inserted) throw std::logic_error("duplicate page handle");
    } catch (...) {
        provider.pages.pop_back();
        throw;
    }
    return *result;
}

Registry::Row& Registry::create_row_locked(Provider& provider, Page& page) {
    if (provider.rows.size() >= kMaxRowsPerProvider) {
        throw std::length_error("provider row limit exceeded");
    }
    auto row = std::make_unique<Row>();
    row->handle = next_handle_++;
    Row* result = row.get();
    provider.rows.push_back(std::move(row));
    try {
        const bool inserted = provider.row_lookup.emplace(
            result->handle, result).second;
        if (!inserted) throw std::logic_error("duplicate row handle");
        try {
            page.rows.push_back(result);
        } catch (...) {
            provider.row_lookup.erase(result->handle);
            throw;
        }
    } catch (...) {
        provider.rows.pop_back();
        throw;
    }
    return *result;
}

ERUI_Result Registry::validate_provider_and_page_locked(
    ERUI_ProviderHandle provider_handle,
    ERUI_PageHandle page_handle,
    Provider*& out_provider,
    Page*& out_page) noexcept {
    const auto provider_it = providers_.find(provider_handle);
    if (provider_it == providers_.end()) return ERUI_INVALID_HANDLE;
    Provider& provider = *provider_it->second;
    if (provider.committed) return ERUI_ALREADY_COMMITTED;
    const auto page_it = provider.page_lookup.find(page_handle);
    if (page_it == provider.page_lookup.end()) return ERUI_INVALID_HANDLE;
    out_provider = &provider;
    out_page = page_it->second;
    return ERUI_OK;
}

ERUI_Result Registry::register_provider(
    const ERUI_ProviderDesc* description,
    ERUI_ProviderHandle* out_provider,
    ERUI_PageHandle* out_root_page) noexcept {
    if (!out_provider || !out_root_page) return ERUI_INVALID_ARGUMENT;
    *out_provider = ERUI_INVALID_PROVIDER;
    *out_root_page = ERUI_INVALID_PAGE;
    if (!description ||
        !field_available(description->size,
            ERUI_FIELD_END(ERUI_ProviderDesc, display_name)) ||
        description->api_version != ERUI_API_VERSION_CURRENT ||
        description->flags != 0 || !description->owner_module ||
        !valid_view(description->provider_id) ||
        !valid_view(description->display_name)) {
        return ERUI_INVALID_ARGUMENT;
    }
    try {
        auto provider = std::make_unique<Provider>();
        provider->id = copy_string(description->provider_id);
        provider->display_name = copy_utf16(description->display_name, false);
        provider->priority = description->root_priority;
        provider->owner_module = static_cast<HMODULE>(description->owner_module);

        std::lock_guard lock(mutex_);
        if (!open_) return ERUI_REGISTRATION_CLOSED;
        if (providers_.size() >= kMaxProviders) return ERUI_OUT_OF_MEMORY;
        for (const auto& item : providers_) {
            if (item.second->id == provider->id) return ERUI_DUPLICATE_PROVIDER_ID;
        }
        provider->handle = next_handle_++;
        provider->ordinal = next_ordinal_++;
        Page& root = create_page_locked(
            *provider, provider->display_name, provider->display_name);
        provider->root_page = root.handle;
        const ERUI_ProviderHandle handle = provider->handle;
        const ERUI_PageHandle root_handle = provider->root_page;
        const bool inserted = providers_.emplace(
            handle, std::move(provider)).second;
        if (!inserted) return ERUI_INTERNAL_ERROR;
        *out_provider = handle;
        *out_root_page = root_handle;
        note_activity_locked();
        return ERUI_OK;
    } catch (const std::bad_alloc&) {
        return ERUI_OUT_OF_MEMORY;
    } catch (...) {
        return ERUI_INVALID_ARGUMENT;
    }
}

ERUI_Result Registry::add_button(
    ERUI_ProviderHandle provider_handle,
    ERUI_PageHandle page_handle,
    const ERUI_ButtonDesc* description,
    ERUI_RowHandle* out_row) noexcept {
    if (out_row) *out_row = ERUI_INVALID_ROW;
    if (!description || !field_available(description->size,
            ERUI_FIELD_END(ERUI_ButtonDesc, reserved)) ||
        description->flags != 0 || description->reserved != 0 ||
        !description->callback || !valid_enabled(description->enabled) ||
        !valid_view(description->label) || !valid_view(description->help)) {
        return ERUI_INVALID_ARGUMENT;
    }
    try {
        std::wstring label = copy_utf16(description->label, false);
        std::wstring help = copy_utf16(description->help);
        std::lock_guard lock(mutex_);
        if (!open_) return ERUI_REGISTRATION_CLOSED;
        Provider* provider{}; Page* page{};
        const ERUI_Result valid = validate_provider_and_page_locked(
            provider_handle, page_handle, provider, page);
        if (valid != ERUI_OK) return valid;
        Row& row = create_row_locked(*provider, *page);
        row.kind = RowKind::button;
        row.label = std::move(label); row.help = std::move(help);
        row.enabled = description->enabled != 0;
        row.button_callback = description->callback; row.user_data = description->user_data;
        if (out_row) *out_row = row.handle;
        note_activity_locked();
        return ERUI_OK;
    } catch (const std::bad_alloc&) { return ERUI_OUT_OF_MEMORY; }
      catch (...) { return ERUI_INVALID_ARGUMENT; }
}

ERUI_Result Registry::add_toggle(
    ERUI_ProviderHandle provider_handle,
    ERUI_PageHandle page_handle,
    const ERUI_ToggleDesc* description,
    ERUI_RowHandle* out_row) noexcept {
    if (out_row) *out_row = ERUI_INVALID_ROW;
    if (!description || !field_available(description->size,
            ERUI_FIELD_END(ERUI_ToggleDesc, enabled)) ||
        description->flags != 0 ||
        description->reserved8[0] != 0 || description->reserved8[1] != 0 ||
        description->reserved8[2] != 0 || !valid_enabled(description->enabled) ||
        !valid_view(description->label) || !valid_view(description->help)) {
        return ERUI_INVALID_ARGUMENT;
    }
    try {
        std::wstring label = copy_utf16(description->label, false);
        std::wstring help = copy_utf16(description->help);
        const std::uint8_t value = description->initial_value == 0 ? 0 : 1;
        std::lock_guard lock(mutex_);
        if (!open_) return ERUI_REGISTRATION_CLOSED;
        Provider* provider{}; Page* page{};
        const ERUI_Result valid = validate_provider_and_page_locked(
            provider_handle, page_handle, provider, page);
        if (valid != ERUI_OK) return valid;
        Row& row = create_row_locked(*provider, *page);
        row.kind = RowKind::toggle; row.label = std::move(label);
        row.help = std::move(help);
        row.enabled = description->enabled != 0; row.native_value = value;
        row.public_value.store(value); row.pending_value.store(value);
        row.changed_callback = description->changed_callback; row.user_data = description->user_data;
        if (out_row) *out_row = row.handle;
        note_activity_locked();
        return ERUI_OK;
    } catch (const std::bad_alloc&) { return ERUI_OUT_OF_MEMORY; }
      catch (...) { return ERUI_INVALID_ARGUMENT; }
}

ERUI_Result Registry::add_slider(
    ERUI_ProviderHandle provider_handle,
    ERUI_PageHandle page_handle,
    const ERUI_SliderDesc* description,
    ERUI_RowHandle* out_row) noexcept {
    if (out_row) *out_row = ERUI_INVALID_ROW;
    if (!description || !field_available(description->size,
            ERUI_FIELD_END(ERUI_SliderDesc, enabled)) ||
        description->flags != 0 ||
        description->reserved8[0] != 0 || description->reserved8[1] != 0 ||
        description->reserved8[2] != 0 || !valid_enabled(description->enabled) ||
        !valid_view(description->label) || !valid_view(description->help) ||
        description->minimum < 0 ||
        description->maximum > 255 || description->minimum > description->maximum ||
        description->step <= 0 || description->initial_value < description->minimum ||
        description->initial_value > description->maximum) return ERUI_INVALID_ARGUMENT;
    try {
        std::wstring label = copy_utf16(description->label, false);
        std::wstring help = copy_utf16(description->help);
        std::lock_guard lock(mutex_);
        if (!open_) return ERUI_REGISTRATION_CLOSED;
        Provider* provider{}; Page* page{};
        const ERUI_Result valid = validate_provider_and_page_locked(
            provider_handle, page_handle, provider, page);
        if (valid != ERUI_OK) return valid;
        Row& row = create_row_locked(*provider, *page);
        row.kind = RowKind::slider; row.label = std::move(label);
        row.help = std::move(help);
        row.enabled = description->enabled != 0; row.native_value = description->initial_value;
        row.public_value.store(description->initial_value);
        row.pending_value.store(description->initial_value);
        row.slider = {description->minimum, description->maximum, description->step};
        row.changed_callback = description->changed_callback; row.user_data = description->user_data;
        if (out_row) *out_row = row.handle;
        note_activity_locked();
        return ERUI_OK;
    } catch (const std::bad_alloc&) { return ERUI_OUT_OF_MEMORY; }
      catch (...) { return ERUI_INVALID_ARGUMENT; }
}

ERUI_Result Registry::add_inline_choice(
    ERUI_ProviderHandle provider_handle,
    ERUI_PageHandle page_handle,
    const ERUI_ChoiceDesc* description,
    ERUI_RowHandle* out_row) noexcept {
    return add_choice(
        provider_handle, page_handle, description, out_row,
        RowKind::inline_choice);
}

ERUI_Result Registry::add_popup_choice(
    ERUI_ProviderHandle provider_handle,
    ERUI_PageHandle page_handle,
    const ERUI_ChoiceDesc* description,
    ERUI_RowHandle* out_row) noexcept {
    return add_choice(
        provider_handle, page_handle, description, out_row,
        RowKind::popup_choice);
}

ERUI_Result Registry::add_choice(
    ERUI_ProviderHandle provider_handle, ERUI_PageHandle page_handle,
    const ERUI_ChoiceDesc* description, ERUI_RowHandle* out_row,
    RowKind kind) noexcept {
    if (out_row) *out_row = ERUI_INVALID_ROW;
    if (!description || !field_available(description->size,
            ERUI_FIELD_END(ERUI_ChoiceDesc, reserved8)) || description->flags != 0 ||
        description->reserved8[0] || description->reserved8[1] || description->reserved8[2] ||
        !valid_view(description->label) ||
        !valid_view(description->help) || !description->options ||
        description->option_count == 0 || description->option_count > 32 ||
        description->initial_index >= description->option_count) return ERUI_INVALID_ARGUMENT;
    try {
        std::wstring label = copy_utf16(description->label, false);
        std::wstring help = copy_utf16(description->help);
        std::vector<std::wstring> choices;
        choices.reserve(description->option_count);
        for (std::uint32_t i = 0; i < description->option_count; ++i) {
            if (!valid_view(description->options[i])) return ERUI_INVALID_ARGUMENT;
            choices.push_back(copy_utf16(description->options[i], false));
        }
        std::lock_guard lock(mutex_);
        if (!open_) return ERUI_REGISTRATION_CLOSED;
        Provider* provider{}; Page* page{};
        const ERUI_Result valid = validate_provider_and_page_locked(
            provider_handle, page_handle, provider, page);
        if (valid != ERUI_OK) return valid;
        Row& row = create_row_locked(*provider, *page);
        row.kind = kind; row.label = std::move(label);
        row.help = std::move(help); row.choices = std::move(choices);
        row.enabled = true;
        row.native_value = description->initial_index;
        row.public_value.store(description->initial_index);
        row.pending_value.store(description->initial_index);
        row.changed_callback = description->changed_callback;
        row.user_data = description->user_data;
        if (out_row) *out_row = row.handle;
        note_activity_locked();
        return ERUI_OK;
    } catch (const std::bad_alloc&) { return ERUI_OUT_OF_MEMORY; }
      catch (...) { return ERUI_INVALID_ARGUMENT; }
}

ERUI_Result Registry::add_submenu(
    ERUI_ProviderHandle provider_handle,
    ERUI_PageHandle parent_handle,
    const ERUI_SubmenuDesc* description,
    ERUI_PageHandle* out_child_page,
    ERUI_RowHandle* out_row) noexcept {
    if (!out_child_page) return ERUI_INVALID_ARGUMENT;
    *out_child_page = ERUI_INVALID_PAGE;
    if (out_row) *out_row = ERUI_INVALID_ROW;
    if (!description || !field_available(description->size,
            ERUI_FIELD_END(ERUI_SubmenuDesc, reserved)) ||
        description->flags != 0 || description->reserved != 0 ||
        !valid_enabled(description->enabled) || !valid_view(description->label) ||
        !valid_view(description->help) || !valid_view(description->page_title) ||
        !valid_view(description->page_help)) return ERUI_INVALID_ARGUMENT;
    try {
        std::wstring label = copy_utf16(description->label, false);
        std::wstring help = copy_utf16(description->help);
        std::wstring title = copy_utf16(description->page_title);
        std::wstring page_help = copy_utf16(description->page_help);
        if (title.empty()) title = label;
        if (page_help.empty()) page_help = help;
        std::lock_guard lock(mutex_);
        if (!open_) return ERUI_REGISTRATION_CLOSED;
        Provider* provider{}; Page* parent{};
        const ERUI_Result valid = validate_provider_and_page_locked(
            provider_handle, parent_handle, provider, parent);
        if (valid != ERUI_OK) return valid;
        Page& child = create_page_locked(*provider, std::move(title), std::move(page_help));
        Row* row_ptr{};
        try {
            row_ptr = &create_row_locked(*provider, *parent);
        } catch (...) {
            provider->page_lookup.erase(child.handle);
            provider->pages.pop_back();
            throw;
        }
        Row& row = *row_ptr;
        row.kind = RowKind::submenu; row.label = std::move(label);
        row.help = std::move(help);
        row.enabled = description->enabled != 0; row.child_page = child.handle;
        *out_child_page = child.handle;
        if (out_row) *out_row = row.handle;
        note_activity_locked();
        return ERUI_OK;
    } catch (const std::bad_alloc&) { return ERUI_OUT_OF_MEMORY; }
      catch (...) { return ERUI_INVALID_ARGUMENT; }
}

ERUI_Result Registry::set_page_presentation(
    ERUI_ProviderHandle provider_handle,
    ERUI_PageHandle page_handle,
    const ERUI_PagePresentationDesc* description) noexcept {
    if (!description || !field_available(description->size,
            ERUI_FIELD_END(ERUI_PagePresentationDesc, reserved)) ||
        description->flags != 0 ||
        description->reserved[0] != 0 || description->reserved[1] != 0 ||
        !valid_view(description->menu_title) ||
        !valid_view(description->page_title) ||
        (!description->formatter && description->user_data)) {
        return ERUI_INVALID_ARGUMENT;
    }
    try {
        std::wstring menu_title = copy_utf16(description->menu_title);
        std::wstring page_title = copy_utf16(description->page_title);
        std::lock_guard lock(mutex_);
        if (!open_) return ERUI_REGISTRATION_CLOSED;
        Provider* provider{};
        Page* page{};
        const ERUI_Result valid = validate_provider_and_page_locked(
            provider_handle, page_handle, provider, page);
        if (valid != ERUI_OK) return valid;
        if (page->handle == provider->root_page) {
            return ERUI_INVALID_ARGUMENT;
        }
        page->menu_title = std::move(menu_title);
        page->page_title = std::move(page_title);
        page->title_formatter = description->formatter;
        page->title_formatter_user_data = description->user_data;
        note_activity_locked();
        return ERUI_OK;
    } catch (const std::bad_alloc&) {
        return ERUI_OUT_OF_MEMORY;
    } catch (...) {
        return ERUI_INVALID_ARGUMENT;
    }
}

ERUI_Result Registry::commit_provider(ERUI_ProviderHandle handle) noexcept {
    std::lock_guard lock(mutex_);
    if (!open_) return ERUI_REGISTRATION_CLOSED;
    const auto found = providers_.find(handle);
    if (found == providers_.end()) return ERUI_INVALID_HANDLE;
    Provider& provider = *found->second;
    if (provider.committed) return ERUI_ALREADY_COMMITTED;

    for (const auto& row_record : provider.rows) {
        const Row& row = *row_record;
        const void* callback = nullptr;
        if (row.button_callback) {
            callback = reinterpret_cast<const void*>(row.button_callback);
        } else if (row.changed_callback) {
            callback = reinterpret_cast<const void*>(row.changed_callback);
        }
        if (callback && !address_belongs_to_module(callback, provider.owner_module)) {
            return ERUI_CALLBACK_REJECTED;
        }
    }
    for (const auto& page_record : provider.pages) {
        const Page& page = *page_record;
        if (page.title_formatter && !address_belongs_to_module(
                reinterpret_cast<const void*>(page.title_formatter),
                provider.owner_module)) {
            return ERUI_CALLBACK_REJECTED;
        }
    }

    HMODULE pinned{};
    if (!GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
            reinterpret_cast<LPCWSTR>(provider.owner_module), &pinned)) {
        return ERUI_CALLBACK_REJECTED;
    }
    provider.pinned_module = pinned;
    provider.committed = true;
    ++generation_;
    note_activity_locked();
    erui::detail::logf(erui::LogLevel::info,
        "Provider committed: id=%s priority=%d pages=%zu rows=%zu generation=%llu",
        provider.id.c_str(), provider.priority, provider.pages.size(), provider.rows.size(),
        static_cast<unsigned long long>(generation_));
    return ERUI_OK;
}

ERUI_Result Registry::abort_provider(ERUI_ProviderHandle handle) noexcept {
    std::lock_guard lock(mutex_);
    const auto found = providers_.find(handle);
    if (found == providers_.end()) return ERUI_INVALID_HANDLE;
    if (found->second->committed) return ERUI_ALREADY_COMMITTED;
    providers_.erase(found);
    if (open_) note_activity_locked();
    return ERUI_OK;
}

ERUI_Result Registry::set_row_value(
    ERUI_ProviderHandle provider_handle,
    ERUI_RowHandle row_handle,
    std::uint8_t value) noexcept {
    std::lock_guard lock(mutex_);
    const auto provider_it = providers_.find(provider_handle);
    if (provider_it == providers_.end() || !provider_it->second->committed) return ERUI_INVALID_HANDLE;
    const auto row_it = provider_it->second->row_lookup.find(row_handle);
    if (row_it == provider_it->second->row_lookup.end()) return ERUI_INVALID_HANDLE;
    Row& row = *row_it->second;
    if (row.kind == RowKind::toggle) value = value == 0 ? 0 : 1;
    else if (row.kind == RowKind::slider) {
        const int clamped = std::clamp<int>(value, row.slider.minimum, row.slider.maximum);
        const int offset = clamped - row.slider.minimum;
        value = static_cast<std::uint8_t>(row.slider.minimum +
            (offset / row.slider.step) * row.slider.step);
    } else if (row.kind == RowKind::inline_choice ||
        row.kind == RowKind::popup_choice) {
        if (value >= row.choices.size()) return ERUI_INVALID_ARGUMENT;
    } else return ERUI_INVALID_ARGUMENT;
    row.public_value.store(value, std::memory_order_release);
    row.pending_value.store(value, std::memory_order_release);
    row.pending_write.store(true, std::memory_order_release);
    return ERUI_OK;
}

ERUI_Result Registry::get_row_value(
    ERUI_ProviderHandle provider_handle,
    ERUI_RowHandle row_handle,
    std::uint8_t* out_value) noexcept {
    if (!out_value) return ERUI_INVALID_ARGUMENT;
    std::lock_guard lock(mutex_);
    const auto provider_it = providers_.find(provider_handle);
    if (provider_it == providers_.end() || !provider_it->second->committed) return ERUI_INVALID_HANDLE;
    const auto row_it = provider_it->second->row_lookup.find(row_handle);
    if (row_it == provider_it->second->row_lookup.end()) return ERUI_INVALID_HANDLE;
    const Row& row = *row_it->second;
    if (row.kind != RowKind::toggle && row.kind != RowKind::slider &&
        row.kind != RowKind::inline_choice &&
        row.kind != RowKind::popup_choice) return ERUI_INVALID_ARGUMENT;
    *out_value = row.public_value.load(std::memory_order_acquire);
    return ERUI_OK;
}

ERUI_Result Registry::enqueue_alert(
    ERUI_ProviderHandle provider_handle,
    const ERUI_AlertDesc* description) noexcept {
    if (!description || !field_available(description->size,
            ERUI_FIELD_END(ERUI_AlertDesc, placement)) ||
        description->flags != 0 ||
        !erui::native::valid_alert_buttons(description->buttons) ||
        !erui::native::valid_alert_placement(description->placement) ||
        !valid_view(description->message) ||
        (!description->callback && description->user_data)) {
        return ERUI_INVALID_ARGUMENT;
    }
    try {
        std::wstring message = copy_utf16(description->message, false);
        {
            std::lock_guard lock(mutex_);
            const auto provider_it = providers_.find(provider_handle);
            if (provider_it == providers_.end() ||
                !provider_it->second->committed) {
                return ERUI_INVALID_HANDLE;
            }
            if (description->callback && !address_belongs_to_module(
                    reinterpret_cast<const void*>(description->callback),
                    provider_it->second->owner_module)) {
                return ERUI_CALLBACK_REJECTED;
            }
        }
        return erui::native::enqueue_native_alert(
            std::move(message), description->buttons,
            description->placement, description->callback,
            description->user_data);
    } catch (const std::bad_alloc&) {
        return ERUI_OUT_OF_MEMORY;
    } catch (...) {
        return ERUI_INVALID_ARGUMENT;
    }
}

void Registry::apply_pending_values() noexcept {
    std::lock_guard lock(mutex_);
    for (const auto& provider_item : providers_) {
        for (const auto& row_item : provider_item.second->rows) {
            Row& row = *row_item;
            if (row.pending_write.exchange(false, std::memory_order_acq_rel)) {
                row.native_value = row.pending_value.load(std::memory_order_acquire);
            }
        }
    }
}

void Registry::button_bridge(void* user_data) noexcept {
    auto* row = static_cast<Row*>(user_data);
    if (row) invoke_button_callback(row->button_callback, row->user_data);
}

void Registry::value_bridge(std::uint8_t value, void* user_data) noexcept {
    auto* row = static_cast<Row*>(user_data);
    if (!row) return;
    row->public_value.store(value, std::memory_order_release);
    row->pending_value.store(value, std::memory_order_release);
    invoke_value_callback(row->changed_callback, row->user_data, value);
}

bool Registry::page_title_bridge(
    const erui::PageTitleFormatRequest& request,
    std::wstring& output,
    void* user_data) noexcept {
    auto* page = static_cast<Page*>(user_data);
    if (!page || !page->title_formatter || request.base_title.empty() ||
        request.base_title.size() >
            (std::numeric_limits<std::uint32_t>::max)() ||
        request.page_number == 0 || request.page_count == 0 ||
        request.page_number > request.page_count ||
        request.page_number >
            (std::numeric_limits<std::uint32_t>::max)() ||
        request.page_count >
            (std::numeric_limits<std::uint32_t>::max)()) {
        return false;
    }
    try {
        std::vector<std::uint16_t> base_title(request.base_title.size());
        std::memcpy(
            base_title.data(),
            request.base_title.data(),
            request.base_title.size() * sizeof(std::uint16_t));
        ERUI_PageTitleFormatContext context{};
        context.size = sizeof(context);
        context.provider = page->provider;
        context.page = page->handle;
        context.base_title = {
            base_title.data(),
            static_cast<std::uint32_t>(base_title.size()),
            0,
        };
        context.page_number = static_cast<std::uint32_t>(request.page_number);
        context.page_count = static_cast<std::uint32_t>(request.page_count);

        std::array<std::uint16_t, ERUI_PAGE_TITLE_BUFFER_CAPACITY> buffer{};
        std::uint32_t length{};
        const ERUI_Result result = invoke_page_title_formatter(
            page->title_formatter,
            page->title_formatter_user_data,
            &context,
            buffer.data(),
            static_cast<std::uint32_t>(buffer.size()),
            &length);
        if (result != ERUI_OK || length == 0 || length > buffer.size()) {
            if (!page->formatter_fallback_logged.exchange(
                    true, std::memory_order_relaxed)) {
                erui::detail::logf(erui::LogLevel::warning,
                    "Page-title formatter fallback: provider=%llu page=%llu slice=%zu/%zu result=%u length=%u",
                    static_cast<unsigned long long>(page->provider),
                    static_cast<unsigned long long>(page->handle),
                    request.page_number,
                    request.page_count,
                    static_cast<unsigned>(result),
                    static_cast<unsigned>(length));
            }
            return false;
        }
        output = copy_utf16({buffer.data(), length, 0}, false);
        return true;
    } catch (...) {
        if (!page->formatter_fallback_logged.exchange(
                true, std::memory_order_relaxed)) {
            erui::detail::logf(erui::LogLevel::warning,
                "Page-title formatter produced invalid UTF-16; using host fallback: provider=%llu page=%llu slice=%zu/%zu",
                static_cast<unsigned long long>(page->provider),
                static_cast<unsigned long long>(page->handle),
                request.page_number,
                request.page_count);
        }
        return false;
    }
}

void Registry::append_page_locked(
    const Provider& provider,
    const Page& source,
    erui::Page& destination) {
    if (source.handle != provider.root_page) {
        destination.set_presentation({
            .menu_title = source.menu_title.empty()
                ? provider.display_name
                : source.menu_title,
            .page_title = source.page_title,
            .formatter = source.title_formatter
                ? &Registry::page_title_bridge
                : nullptr,
            .user_data = source.title_formatter
                ? const_cast<Page*>(&source)
                : nullptr,
        });
    }
    for (Row* row : source.rows) {
        switch (row->kind) {
        case RowKind::button:
            destination.add_button(row->label, row->help,
                {.callback = &Registry::button_bridge, .user_data = row}, row->enabled);
            break;
        case RowKind::toggle:
            destination.add_toggle(row->label, row->help, row->native_value, row->enabled,
                {.callback = &Registry::value_bridge, .user_data = row});
            break;
        case RowKind::slider:
            destination.add_slider(row->label, row->help, row->native_value, row->slider,
                row->enabled, {.callback = &Registry::value_bridge, .user_data = row});
            break;
        case RowKind::inline_choice:
            destination.add_inline_choice(
                row->label, row->help, row->native_value,
                row->choices, row->enabled,
                {.callback = &Registry::value_bridge, .user_data = row});
            break;
        case RowKind::popup_choice:
            destination.add_popup_choice(
                row->label, row->help, row->native_value,
                row->choices, row->enabled,
                {.callback = &Registry::value_bridge, .user_data = row});
            break;
        case RowKind::submenu: {
            const auto child = provider.page_lookup.find(row->child_page);
            if (child == provider.page_lookup.end()) {
                throw std::logic_error("submenu target is missing");
            }
            erui::Page& target = destination.add_submenu(
                row->label, row->help, child->second->title, child->second->help, row->enabled);
            append_page_locked(provider, *child->second, target);
            break;
        }
        }
    }
}

void Registry::note_activity_locked() noexcept {
    last_activity_tick_ = GetTickCount64();
}

std::unique_ptr<erui::Menu> Registry::freeze_and_build_locked() {
    if (!open_) throw std::logic_error("registration is not open");
    open_ = false;

    auto menu = std::make_unique<erui::Menu>(
        L"ERNativeUI", L"Settings registered by ERNativeUI client mods.",
        pagination_locale_);
    std::vector<const Provider*> ordered{};
    for (const auto& item : providers_) if (item.second->committed) ordered.push_back(item.second.get());
    std::sort(ordered.begin(), ordered.end(), [](const Provider* left, const Provider* right) {
        if (left->priority != right->priority) return left->priority < right->priority;
        if (left->id != right->id) return left->id < right->id;
        return left->ordinal < right->ordinal;
    });
    for (const Provider* provider : ordered) {
        const auto root = provider->page_lookup.find(provider->root_page);
        if (root == provider->page_lookup.end()) throw std::logic_error("provider root missing");
        append_page_locked(*provider, *root->second, menu->root());
    }
    return menu;
}

std::unique_ptr<erui::Menu> Registry::freeze_and_build() {
    std::lock_guard lock(mutex_);
    return freeze_and_build_locked();
}

std::unique_ptr<erui::Menu> Registry::try_freeze_and_build(
    std::uint64_t opened_at,
    std::uint64_t quiet_ms,
    std::uint64_t maximum_wait_ms) {
    std::lock_guard lock(mutex_);
    if (!open_) throw std::logic_error("registration is not open");

    std::size_t committed = 0;
    for (const auto& item : providers_) {
        if (item.second->committed) ++committed;
    }
    const std::size_t drafts = providers_.size() - committed;
    const std::uint64_t now = GetTickCount64();
    const bool maximum_reached = now - opened_at >= maximum_wait_ms;
    const bool quiet = committed != 0 && drafts == 0 &&
        now - last_activity_tick_ >= quiet_ms;
    if (!maximum_reached && !quiet) return {};
    return freeze_and_build_locked();
}

} // namespace erui::host
