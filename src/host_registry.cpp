#include "host_registry.hpp"

#include "input_binding_model.hpp"
#include "native_input_bindings.hpp"
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
constexpr std::size_t kMaxBindingSections = 4096;
constexpr std::size_t kMaxBindingSectionsPerProvider = 4096;
constexpr std::size_t kMaxBindingsPerProvider = 4096;
constexpr std::size_t kMaxBindings = 4096;
constexpr std::size_t kMaxIdentifierBytes = 255;
constexpr std::size_t kMaxTextUnits = 4096;

static_assert(ERUI_BUILTIN_PAGE_GAME_OPTIONS ==
    static_cast<ERUI_BuiltinPage>(
        erui::detail::BuiltinPage::game_options));
static_assert(ERUI_BUILTIN_PAGE_CAMERA_OPTIONS ==
    static_cast<ERUI_BuiltinPage>(
        erui::detail::BuiltinPage::camera_options));
static_assert(ERUI_BUILTIN_PAGE_DISPLAY ==
    static_cast<ERUI_BuiltinPage>(erui::detail::BuiltinPage::display));
static_assert(ERUI_BUILTIN_PAGE_SOUND ==
    static_cast<ERUI_BuiltinPage>(erui::detail::BuiltinPage::sound));
static_assert(ERUI_BUILTIN_PAGE_NETWORK ==
    static_cast<ERUI_BuiltinPage>(erui::detail::BuiltinPage::network));
static_assert(ERUI_BUILTIN_PAGE_KEYBOARD_MOUSE ==
    static_cast<ERUI_BuiltinPage>(
        erui::detail::BuiltinPage::keyboard_mouse));
static_assert(ERUI_BUILTIN_PAGE_GRAPHICS ==
    static_cast<ERUI_BuiltinPage>(erui::detail::BuiltinPage::graphics));
static_assert(ERUI_BUILTIN_PAGE_COUNT == erui::detail::builtin_page_count);

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

bool valid_machine_identifier(ERUI_StringView view) noexcept {
    if (!valid_view(view) || !view.data || view.length == 0 ||
        view.length > kMaxIdentifierBytes) {
        return false;
    }
    for (std::uint32_t index = 0; index < view.length; ++index) {
        const unsigned char byte =
            static_cast<unsigned char>(view.data[index]);
        if (!((byte >= 'a' && byte <= 'z') ||
              (byte >= 'A' && byte <= 'Z') ||
              (byte >= '0' && byte <= '9') ||
              byte == '.' || byte == '_' || byte == '-')) {
            return false;
        }
    }
    return true;
}

bool valid_view(ERUI_Utf16View view) noexcept {
    return view.reserved == 0;
}

bool valid_color(ERUI_Color color) noexcept {
    return color.reserved == 0;
}

erui::detail::RgbColor to_internal_color(ERUI_Color color) noexcept {
    return {color.red, color.green, color.blue};
}

ERUI_Color to_public_color(erui::detail::RgbColor color) noexcept {
    return {color.red, color.green, color.blue, 0};
}

bool address_belongs_to_module(const void* address, HMODULE module) noexcept {
    if (!address || !module) return false;
    MEMORY_BASIC_INFORMATION memory{};
    return VirtualQuery(address, &memory, sizeof(memory)) == sizeof(memory) &&
        memory.AllocationBase == module;
}

HMODULE module_containing(const void* address) noexcept {
    if (!address) return nullptr;
    MEMORY_BASIC_INFORMATION memory{};
    return VirtualQuery(address, &memory, sizeof(memory)) == sizeof(memory)
        ? static_cast<HMODULE>(memory.AllocationBase)
        : nullptr;
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

void invoke_text_input_callback(
    ERUI_TextInputChangedCallback callback,
    void* user_data,
    const ERUI_TextInputChangeContext* context) noexcept {
    if (!callback) return;
#if defined(_MSC_VER)
    __try {
        callback(user_data, context);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        erui::detail::logf(erui::LogLevel::error,
            "Provider TextInput callback raised SEH exception 0x%08lX",
            GetExceptionCode());
    }
#else
    try {
        callback(user_data, context);
    } catch (...) {
        erui::detail::logf(erui::LogLevel::error,
            "Provider TextInput callback raised a C++ exception");
    }
#endif
}

void invoke_color_picker_callback(
    ERUI_ColorPickerChangedCallback callback,
    void* user_data,
    const ERUI_ColorPickerChangeContext* context) noexcept {
    if (!callback) return;
#if defined(_MSC_VER)
    __try {
        callback(user_data, context);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        erui::detail::logf(erui::LogLevel::error,
            "Provider ColorPicker callback raised SEH exception 0x%08lX",
            GetExceptionCode());
    }
#else
    try {
        callback(user_data, context);
    } catch (...) {
        erui::detail::logf(erui::LogLevel::error,
            "Provider ColorPicker callback raised a C++ exception");
    }
#endif
}

void invoke_input_action_callback(
    ERUI_InputActionActivatedCallback callback,
    void* user_data,
    const ERUI_InputActionActivatedContext* context) noexcept {
    if (!callback) return;
#if defined(_MSC_VER)
    __try {
        callback(user_data, context);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        erui::detail::logf(erui::LogLevel::error,
            "Provider input-binding callback raised SEH exception 0x%08lX",
            GetExceptionCode());
    }
#else
    try {
        callback(user_data, context);
    } catch (...) {
        erui::detail::logf(erui::LogLevel::error,
            "Provider input-binding callback raised a C++ exception");
    }
#endif
}

void invoke_assignments_changed_callback(
    ERUI_AssignmentsChangedCallback callback,
    void* user_data,
    const ERUI_AssignmentsChangedContext* context) noexcept {
    if (!callback) return;
#if defined(_MSC_VER)
    __try {
        callback(user_data, context);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        erui::detail::logf(erui::LogLevel::error,
            "Provider assignments-changed callback raised SEH exception 0x%08lX",
            GetExceptionCode());
    }
#else
    try {
        callback(user_data, context);
    } catch (...) {
        erui::detail::logf(erui::LogLevel::error,
            "Provider assignments-changed callback raised a C++ exception");
    }
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
    if (frozen_) {
        throw std::logic_error("host locale must be set before registration freezes");
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
    row->provider = provider.handle;
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

ERUI_Result Registry::require_provider_api_version(
    ERUI_ProviderHandle provider_handle,
    std::uint32_t minimum_version) const noexcept {
    std::lock_guard lock(mutex_);
    const auto provider_it = providers_.find(provider_handle);
    if (provider_it == providers_.end()) return ERUI_INVALID_HANDLE;
    return provider_it->second->api_version >= minimum_version
        ? static_cast<ERUI_Result>(ERUI_OK)
        : static_cast<ERUI_Result>(ERUI_NOT_SUPPORTED);
}

ERUI_Result Registry::register_provider(
    std::uint32_t negotiated_api_version,
    const ERUI_ProviderDesc* description,
    ERUI_ProviderHandle* out_provider,
    ERUI_PageHandle* out_root_page) noexcept {
    if (!out_provider || !out_root_page) return ERUI_INVALID_ARGUMENT;
    *out_provider = ERUI_INVALID_PROVIDER;
    *out_root_page = ERUI_INVALID_PAGE;
    if (!description ||
        !field_available(description->size,
            ERUI_FIELD_END(ERUI_ProviderDesc, display_name)) ||
        (negotiated_api_version != ERUI_API_VERSION_1_0 &&
            negotiated_api_version != ERUI_API_VERSION_1_1) ||
        description->api_version != negotiated_api_version ||
        description->flags != 0 || !description->owner_module ||
        !valid_view(description->provider_id) ||
        (negotiated_api_version == ERUI_API_VERSION_1_1 &&
            !valid_machine_identifier(description->provider_id)) ||
        !valid_view(description->display_name)) {
        return ERUI_INVALID_ARGUMENT;
    }
    try {
        auto provider = std::make_unique<Provider>();
        provider->id = copy_string(description->provider_id);
        provider->display_name = copy_utf16(description->display_name, false);
        provider->priority = description->root_priority;
        provider->owner_module = static_cast<HMODULE>(description->owner_module);
        provider->api_version = negotiated_api_version;

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
        root.builtin_page = erui::detail::BuiltinPage::game_options;
        provider->builtin_pages[erui::detail::builtin_page_offset(
            erui::detail::BuiltinPage::game_options)] = root.handle;
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

ERUI_Result Registry::get_builtin_page(
    ERUI_ProviderHandle provider_handle,
    ERUI_BuiltinPage builtin_page,
    ERUI_PageHandle* out_page) noexcept {
    const ERUI_Result version = require_provider_api_version(
        provider_handle, ERUI_API_VERSION_1_1);
    if (version != ERUI_OK) return version;
    if (!out_page) return ERUI_INVALID_ARGUMENT;
    *out_page = ERUI_INVALID_PAGE;
    if (builtin_page >= ERUI_BUILTIN_PAGE_COUNT) {
        return ERUI_INVALID_ARGUMENT;
    }

    try {
        std::lock_guard lock(mutex_);
        if (!open_) return ERUI_REGISTRATION_CLOSED;
        const auto provider_it = providers_.find(provider_handle);
        if (provider_it == providers_.end()) return ERUI_INVALID_HANDLE;
        Provider& provider = *provider_it->second;
        if (provider.api_version < ERUI_API_VERSION_1_1) {
            return ERUI_NOT_SUPPORTED;
        }
        if (provider.committed) return ERUI_ALREADY_COMMITTED;

        const auto destination =
            static_cast<erui::detail::BuiltinPage>(builtin_page);
        ERUI_PageHandle& handle = provider.builtin_pages[
            erui::detail::builtin_page_offset(destination)];
        if (handle == ERUI_INVALID_PAGE) {
            Page& page = create_page_locked(
                provider, provider.display_name, provider.display_name);
            page.builtin_page = destination;
            handle = page.handle;
        }
        *out_page = handle;
        note_activity_locked();
        return ERUI_OK;
    } catch (const std::bad_alloc&) {
        return ERUI_OUT_OF_MEMORY;
    } catch (...) {
        return ERUI_INVALID_ARGUMENT;
    }
}

ERUI_Result Registry::add_input_section(
    ERUI_ProviderHandle provider_handle,
    const ERUI_InputSectionDesc* description,
    ERUI_InputSectionHandle* out_section) noexcept {
    // Version-gate before dereferencing any 1.1-only pointer. This preserves
    // the frozen 1.0 contract even if a caller obtains this entry elsewhere.
    {
        std::lock_guard lock(mutex_);
        const auto provider_it = providers_.find(provider_handle);
        if (provider_it == providers_.end()) return ERUI_INVALID_HANDLE;
        if (provider_it->second->api_version < ERUI_API_VERSION_1_1) {
            return ERUI_NOT_SUPPORTED;
        }
    }

    if (!out_section) return ERUI_INVALID_ARGUMENT;
    *out_section = ERUI_INVALID_INPUT_SECTION;
    if (!description || !field_available(description->size,
            ERUI_FIELD_END(ERUI_InputSectionDesc, reserved)) ||
        description->flags != 0 ||
        description->reserved[0] != 0 || description->reserved[1] != 0 ||
        !valid_view(description->label)) {
        return ERUI_INVALID_ARGUMENT;
    }

    try {
        std::wstring label = copy_utf16(description->label, false);
        std::lock_guard lock(mutex_);
        if (!open_) return ERUI_REGISTRATION_CLOSED;
        const auto provider_it = providers_.find(provider_handle);
        if (provider_it == providers_.end()) return ERUI_INVALID_HANDLE;
        Provider& provider = *provider_it->second;
        if (provider.api_version < ERUI_API_VERSION_1_1) {
            return ERUI_NOT_SUPPORTED;
        }
        if (provider.committed) return ERUI_ALREADY_COMMITTED;
        if (provider.binding_sections.size() >=
            kMaxBindingSectionsPerProvider) {
            return ERUI_OUT_OF_MEMORY;
        }
        std::size_t total_sections = 0;
        for (const auto& item : providers_) {
            total_sections += item.second->binding_sections.size();
        }
        if (total_sections >= kMaxBindingSections) {
            return ERUI_OUT_OF_MEMORY;
        }

        auto section = std::make_unique<BindingSection>();
        section->handle = next_handle_++;
        section->provider = provider.handle;
        section->label = std::move(label);
        BindingSection* const raw = section.get();
        provider.binding_sections.push_back(std::move(section));
        try {
            if (!provider.binding_section_lookup.emplace(
                    raw->handle, raw).second) {
                throw std::logic_error("duplicate input-binding section handle");
            }
        } catch (...) {
            provider.binding_sections.pop_back();
            throw;
        }
        *out_section = raw->handle;
        note_activity_locked();
        return ERUI_OK;
    } catch (const std::bad_alloc&) {
        return ERUI_OUT_OF_MEMORY;
    } catch (...) {
        return ERUI_INVALID_ARGUMENT;
    }
}

ERUI_Result Registry::add_input_action(
    ERUI_ProviderHandle provider_handle,
    ERUI_InputSectionHandle section_handle,
    const ERUI_InputActionDesc* description,
    ERUI_InputActionHandle* out_action) noexcept {
    {
        std::lock_guard lock(mutex_);
        const auto provider_it = providers_.find(provider_handle);
        if (provider_it == providers_.end()) return ERUI_INVALID_HANDLE;
        if (provider_it->second->api_version < ERUI_API_VERSION_1_1) {
            return ERUI_NOT_SUPPORTED;
        }
    }

    if (out_action) *out_action = ERUI_INVALID_INPUT_ACTION;
    if (!description || !field_available(description->size,
            ERUI_FIELD_END(ERUI_InputActionDesc, reserved)) ||
        description->flags != 0 ||
        description->reserved[0] != 0 || description->reserved[1] != 0 ||
        !valid_machine_identifier(description->action_id) ||
        !valid_view(description->label) ||
        !description->activated_callback ||
        !erui::detail::valid_action_inputs(description->default_inputs) ||
        erui::detail::supported_input_devices(
            description->default_inputs) == ERUI_INPUT_DEVICE_NONE) {
        return ERUI_INVALID_ARGUMENT;
    }

    try {
        std::string binding_id = copy_string(description->action_id);
        std::wstring label = copy_utf16(description->label, false);
        std::lock_guard lock(mutex_);
        if (!open_) return ERUI_REGISTRATION_CLOSED;
        const auto provider_it = providers_.find(provider_handle);
        if (provider_it == providers_.end()) return ERUI_INVALID_HANDLE;
        Provider& provider = *provider_it->second;
        if (provider.api_version < ERUI_API_VERSION_1_1) {
            return ERUI_NOT_SUPPORTED;
        }
        if (provider.committed) return ERUI_ALREADY_COMMITTED;
        const auto section_it =
            provider.binding_section_lookup.find(section_handle);
        if (section_it == provider.binding_section_lookup.end()) {
            return ERUI_INVALID_HANDLE;
        }
        if (provider.binding_id_lookup.find(binding_id) !=
            provider.binding_id_lookup.end()) {
            return ERUI_DUPLICATE_ACTION_ID;
        }
        if (provider.bindings.size() >= kMaxBindingsPerProvider) {
            return ERUI_OUT_OF_MEMORY;
        }
        std::size_t total_bindings = 0;
        for (const auto& item : providers_) {
            total_bindings += item.second->bindings.size();
        }
        if (total_bindings >= kMaxBindings) {
            return ERUI_OUT_OF_MEMORY;
        }

        auto binding = std::make_unique<Binding>();
        binding->handle = next_handle_++;
        binding->provider = provider.handle;
        binding->section = section_handle;
        binding->id = std::move(binding_id);
        binding->label = std::move(label);
        binding->default_inputs = description->default_inputs;
        binding->current_inputs = description->default_inputs;
        binding->activated_callback = description->activated_callback;
        binding->user_data = description->user_data;
        Binding* const raw = binding.get();
        provider.bindings.push_back(std::move(binding));
        try {
            if (!provider.binding_lookup.emplace(raw->handle, raw).second) {
                throw std::logic_error("duplicate input-binding handle");
            }
            try {
                if (!provider.binding_id_lookup.emplace(raw->id, raw).second) {
                    throw std::logic_error("duplicate input-binding ID");
                }
                try {
                    section_it->second->bindings.push_back(raw);
                } catch (...) {
                    provider.binding_id_lookup.erase(raw->id);
                    throw;
                }
            } catch (...) {
                provider.binding_lookup.erase(raw->handle);
                throw;
            }
        } catch (...) {
            provider.bindings.pop_back();
            throw;
        }
        if (out_action) *out_action = raw->handle;
        note_activity_locked();
        return ERUI_OK;
    } catch (const std::bad_alloc&) {
        return ERUI_OUT_OF_MEMORY;
    } catch (...) {
        return ERUI_INVALID_ARGUMENT;
    }
}

ERUI_Result Registry::set_assignments_changed_handler(
    ERUI_ProviderHandle provider_handle,
    const ERUI_AssignmentsChangedHandlerDesc* description) noexcept {
    const ERUI_Result version = require_provider_api_version(
        provider_handle, ERUI_API_VERSION_1_1);
    if (version != ERUI_OK) return version;
    if (!description || !field_available(description->size,
            ERUI_FIELD_END(ERUI_AssignmentsChangedHandlerDesc, reserved)) ||
        description->flags != 0 ||
        description->reserved[0] != 0 || description->reserved[1] != 0 ||
        (!description->callback && description->user_data)) {
        return ERUI_INVALID_ARGUMENT;
    }

    std::lock_guard lock(mutex_);
    if (!open_) return ERUI_REGISTRATION_CLOSED;
    const auto provider_it = providers_.find(provider_handle);
    if (provider_it == providers_.end()) return ERUI_INVALID_HANDLE;
    Provider& provider = *provider_it->second;
    if (provider.api_version < ERUI_API_VERSION_1_1) {
        return ERUI_NOT_SUPPORTED;
    }
    if (provider.committed) return ERUI_ALREADY_COMMITTED;
    provider.assignments_changed_callback = description->callback;
    provider.assignments_changed_user_data = description->user_data;
    note_activity_locked();
    return ERUI_OK;
}

ERUI_Result Registry::set_action_inputs(
    ERUI_ProviderHandle provider_handle,
    ERUI_InputActionHandle action_handle,
    const ERUI_ActionInputs* inputs) noexcept {
    const ERUI_Result version = require_provider_api_version(
        provider_handle, ERUI_API_VERSION_1_1);
    if (version != ERUI_OK) return version;
    if (!inputs || !erui::detail::valid_action_inputs(*inputs)) {
        return ERUI_INVALID_ARGUMENT;
    }

    std::unique_lock lock(mutex_);
    const auto provider_it = providers_.find(provider_handle);
    if (provider_it == providers_.end()) return ERUI_INVALID_HANDLE;
    Provider& provider = *provider_it->second;
    if (provider.api_version < ERUI_API_VERSION_1_1) {
        return ERUI_NOT_SUPPORTED;
    }
    const auto action_it = provider.binding_lookup.find(action_handle);
    if (action_it == provider.binding_lookup.end()) return ERUI_INVALID_HANDLE;

    Binding& action = *action_it->second;
    if (frozen_) {
        ERUI_ActionInputs complete{};
        lock.unlock();
        const ERUI_Result result =
            erui::native::stage_native_input_action_inputs(
                action_handle, *inputs, complete);
        return result;
    }

    ERUI_ActionInputs next{};
    if (!erui::detail::overlay_action_inputs(
            action.current_inputs, *inputs, next)) {
        return ERUI_INVALID_ARGUMENT;
    }
    action.current_inputs = next;
    return ERUI_OK;
}

ERUI_Result Registry::get_action_inputs(
    ERUI_ProviderHandle provider_handle,
    ERUI_InputActionHandle action_handle,
    ERUI_ActionInputs* out_inputs) noexcept {
    const ERUI_Result version = require_provider_api_version(
        provider_handle, ERUI_API_VERSION_1_1);
    if (version != ERUI_OK) return version;
    if (!out_inputs) return ERUI_INVALID_ARGUMENT;
    if (out_inputs->size != sizeof(*out_inputs)) {
        return out_inputs->size < sizeof(*out_inputs)
            ? static_cast<ERUI_Result>(ERUI_BUFFER_TOO_SMALL)
            : static_cast<ERUI_Result>(ERUI_INVALID_ARGUMENT);
    }
    std::unique_lock lock(mutex_);
    const auto provider_it = providers_.find(provider_handle);
    if (provider_it == providers_.end()) return ERUI_INVALID_HANDLE;
    const Provider& provider = *provider_it->second;
    if (provider.api_version < ERUI_API_VERSION_1_1) {
        return ERUI_NOT_SUPPORTED;
    }
    const auto action_it = provider.binding_lookup.find(action_handle);
    if (action_it == provider.binding_lookup.end()) return ERUI_INVALID_HANDLE;
    if (frozen_) {
        lock.unlock();
        ERUI_ActionInputs complete{};
        const ERUI_Result result =
            erui::native::get_native_input_action_inputs(
                action_handle, complete);
        if (result != ERUI_OK) return result;
        *out_inputs = complete;
        return ERUI_OK;
    }
    *out_inputs = action_it->second->current_inputs;
    return ERUI_OK;
}

ERUI_Result Registry::get_action_default_inputs(
    ERUI_ProviderHandle provider_handle,
    ERUI_InputActionHandle action_handle,
    ERUI_ActionInputs* out_inputs) noexcept {
    const ERUI_Result version = require_provider_api_version(
        provider_handle, ERUI_API_VERSION_1_1);
    if (version != ERUI_OK) return version;
    if (!out_inputs) return ERUI_INVALID_ARGUMENT;
    if (out_inputs->size != sizeof(*out_inputs)) {
        return out_inputs->size < sizeof(*out_inputs)
            ? static_cast<ERUI_Result>(ERUI_BUFFER_TOO_SMALL)
            : static_cast<ERUI_Result>(ERUI_INVALID_ARGUMENT);
    }
    std::unique_lock lock(mutex_);
    const auto provider_it = providers_.find(provider_handle);
    if (provider_it == providers_.end()) return ERUI_INVALID_HANDLE;
    const Provider& provider = *provider_it->second;
    if (provider.api_version < ERUI_API_VERSION_1_1) {
        return ERUI_NOT_SUPPORTED;
    }
    const auto action_it = provider.binding_lookup.find(action_handle);
    if (action_it == provider.binding_lookup.end()) return ERUI_INVALID_HANDLE;
    *out_inputs = action_it->second->default_inputs;
    return ERUI_OK;
}

ERUI_Result Registry::reset_action_inputs(
    ERUI_ProviderHandle provider_handle,
    ERUI_InputActionHandle action_handle,
    ERUI_InputDevices devices) noexcept {
    const ERUI_Result version = require_provider_api_version(
        provider_handle, ERUI_API_VERSION_1_1);
    if (version != ERUI_OK) return version;
    if ((devices & ~static_cast<ERUI_InputDevices>(
            ERUI_INPUT_DEVICE_ALL)) != 0 ||
        devices == ERUI_INPUT_DEVICE_NONE) {
        return ERUI_INVALID_ARGUMENT;
    }

    std::unique_lock lock(mutex_);
    const auto provider_it = providers_.find(provider_handle);
    if (provider_it == providers_.end()) return ERUI_INVALID_HANDLE;
    Provider& provider = *provider_it->second;
    if (provider.api_version < ERUI_API_VERSION_1_1) {
        return ERUI_NOT_SUPPORTED;
    }
    const auto action_it = provider.binding_lookup.find(action_handle);
    if (action_it == provider.binding_lookup.end()) return ERUI_INVALID_HANDLE;

    Binding& action = *action_it->second;
    if (frozen_) {
        ERUI_ActionInputs complete{};
        lock.unlock();
        const ERUI_Result result =
            erui::native::reset_native_input_action_inputs(
                action_handle, devices, complete);
        return result;
    }

    ERUI_ActionInputs next{};
    if (!erui::detail::reset_action_inputs_to_defaults(
            action.current_inputs, action.default_inputs, devices, next)) {
        return ERUI_INVALID_ARGUMENT;
    }
    action.current_inputs = next;
    return ERUI_OK;
}

ERUI_Result Registry::open_storage(
    ERUI_ProviderHandle provider_handle,
    const ERUI_StorageDesc* description,
    ERUI_StorageHandle* out_storage) noexcept {
    // Preserve the 1.0 prefix contract: reject the provider version before
    // touching any pointer that exists only in the 1.1 ABI.
    {
        std::lock_guard lock(mutex_);
        const auto provider_it = providers_.find(provider_handle);
        if (provider_it == providers_.end()) return ERUI_INVALID_HANDLE;
        if (provider_it->second->api_version < ERUI_API_VERSION_1_1) {
            return ERUI_NOT_SUPPORTED;
        }
    }

    if (!out_storage) return ERUI_INVALID_ARGUMENT;
    *out_storage = ERUI_INVALID_STORAGE;

    try {
        std::lock_guard lock(mutex_);
        if (!open_) return ERUI_REGISTRATION_CLOSED;
        const auto provider_it = providers_.find(provider_handle);
        if (provider_it == providers_.end()) return ERUI_INVALID_HANDLE;
        Provider& provider = *provider_it->second;
        if (provider.api_version < ERUI_API_VERSION_1_1) {
            return ERUI_NOT_SUPPORTED;
        }
        if (provider.committed) return ERUI_ALREADY_COMMITTED;

        std::unique_ptr<ProviderStorage> candidate{};
        const ERUI_Result opened = ProviderStorage::open(
            provider.id,
            module_containing(&g_api_state),
            provider.owner_module,
            description,
            candidate);
        if (opened != ERUI_OK) return opened;

        if (provider.storage) {
            if (provider.storage->backing_path() != candidate->backing_path()) {
                return ERUI_INVALID_ARGUMENT;
            }
            *out_storage = provider.storage_handle;
            return ERUI_OK;
        }

        provider.storage = std::shared_ptr<ProviderStorage>(
            std::move(candidate));
        provider.storage_handle = next_handle_++;
        *out_storage = provider.storage_handle;
        note_activity_locked();
        return ERUI_OK;
    } catch (const std::bad_alloc&) {
        return ERUI_OUT_OF_MEMORY;
    } catch (...) {
        return ERUI_INVALID_ARGUMENT;
    }
}

ERUI_Result Registry::find_storage_locked(
    ERUI_ProviderHandle provider_handle,
    ERUI_StorageHandle storage_handle,
    std::shared_ptr<ProviderStorage>& output) const noexcept {
    output.reset();
    const auto provider_it = providers_.find(provider_handle);
    if (provider_it == providers_.end()) return ERUI_INVALID_HANDLE;
    const Provider& provider = *provider_it->second;
    if (provider.api_version < ERUI_API_VERSION_1_1) {
        return ERUI_NOT_SUPPORTED;
    }
    if (!provider.storage || storage_handle == ERUI_INVALID_STORAGE ||
        storage_handle != provider.storage_handle) {
        return ERUI_INVALID_HANDLE;
    }
    output = provider.storage;
    return ERUI_OK;
}

ERUI_Result Registry::storage_load(
    ERUI_ProviderHandle provider_handle,
    ERUI_StorageHandle storage_handle) noexcept {
    std::shared_ptr<ProviderStorage> storage{};
    {
        std::lock_guard lock(mutex_);
        const ERUI_Result found = find_storage_locked(
            provider_handle, storage_handle, storage);
        if (found != ERUI_OK) return found;
    }
    return storage->load();
}

ERUI_Result Registry::storage_save(
    ERUI_ProviderHandle provider_handle,
    ERUI_StorageHandle storage_handle) noexcept {
    std::shared_ptr<ProviderStorage> storage{};
    {
        std::lock_guard lock(mutex_);
        const ERUI_Result found = find_storage_locked(
            provider_handle, storage_handle, storage);
        if (found != ERUI_OK) return found;
    }
    return storage->save();
}

ERUI_Result Registry::storage_get_utf8(
    ERUI_ProviderHandle provider_handle,
    ERUI_StorageHandle storage_handle,
    const ERUI_StorageKey* key,
    char* output,
    std::uint32_t output_capacity,
    std::uint32_t* out_length) noexcept {
    std::shared_ptr<ProviderStorage> storage{};
    {
        std::lock_guard lock(mutex_);
        const ERUI_Result found = find_storage_locked(
            provider_handle, storage_handle, storage);
        if (found != ERUI_OK) return found;
    }
    return storage->get_utf8(key, output, output_capacity, out_length);
}

ERUI_Result Registry::storage_set_utf8(
    ERUI_ProviderHandle provider_handle,
    ERUI_StorageHandle storage_handle,
    const ERUI_StorageKey* key,
    const ERUI_StringView* value) noexcept {
    std::shared_ptr<ProviderStorage> storage{};
    {
        std::lock_guard lock(mutex_);
        const ERUI_Result found = find_storage_locked(
            provider_handle, storage_handle, storage);
        if (found != ERUI_OK) return found;
    }
    return storage->set_utf8(key, value);
}

ERUI_Result Registry::storage_erase(
    ERUI_ProviderHandle provider_handle,
    ERUI_StorageHandle storage_handle,
    const ERUI_StorageKey* key) noexcept {
    std::shared_ptr<ProviderStorage> storage{};
    {
        std::lock_guard lock(mutex_);
        const ERUI_Result found = find_storage_locked(
            provider_handle, storage_handle, storage);
        if (found != ERUI_OK) return found;
    }
    return storage->erase(key);
}

ERUI_Result Registry::storage_get_action_inputs(
    ERUI_ProviderHandle provider_handle,
    ERUI_StorageHandle storage_handle,
    const ERUI_StorageKey* key,
    ERUI_ActionInputs* out_inputs) noexcept {
    std::shared_ptr<ProviderStorage> storage{};
    {
        std::lock_guard lock(mutex_);
        const ERUI_Result found = find_storage_locked(
            provider_handle, storage_handle, storage);
        if (found != ERUI_OK) return found;
    }
    return storage->get_action_inputs(key, out_inputs);
}

ERUI_Result Registry::storage_set_action_inputs(
    ERUI_ProviderHandle provider_handle,
    ERUI_StorageHandle storage_handle,
    const ERUI_StorageKey* key,
    const ERUI_ActionInputs* inputs) noexcept {
    std::shared_ptr<ProviderStorage> storage{};
    {
        std::lock_guard lock(mutex_);
        const ERUI_Result found = find_storage_locked(
            provider_handle, storage_handle, storage);
        if (found != ERUI_OK) return found;
    }
    return storage->set_action_inputs(key, inputs);
}

ERUI_Result Registry::storage_apply_assignment_changes(
    ERUI_ProviderHandle provider_handle,
    ERUI_StorageHandle storage_handle,
    const ERUI_StringView* section,
    const ERUI_AssignmentChange* changes,
    std::uint32_t change_count) noexcept {
    std::shared_ptr<ProviderStorage> storage{};
    {
        std::lock_guard lock(mutex_);
        const ERUI_Result found = find_storage_locked(
            provider_handle, storage_handle, storage);
        if (found != ERUI_OK) return found;
    }
    return storage->apply_assignment_changes(
        section, changes, change_count);
}

ERUI_Result Registry::storage_get_info(
    ERUI_ProviderHandle provider_handle,
    ERUI_StorageHandle storage_handle,
    ERUI_StorageInfo* out_info) noexcept {
    std::shared_ptr<ProviderStorage> storage{};
    {
        std::lock_guard lock(mutex_);
        const ERUI_Result found = find_storage_locked(
            provider_handle, storage_handle, storage);
        if (found != ERUI_OK) return found;
    }
    return storage->get_info(out_info);
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
        const int initial_offset =
            static_cast<int>(description->initial_value) -
            description->minimum;
        const std::uint8_t initial_value = static_cast<std::uint8_t>(
            description->minimum +
            (initial_offset / description->step) * description->step);
        std::lock_guard lock(mutex_);
        if (!open_) return ERUI_REGISTRATION_CLOSED;
        Provider* provider{}; Page* page{};
        const ERUI_Result valid = validate_provider_and_page_locked(
            provider_handle, page_handle, provider, page);
        if (valid != ERUI_OK) return valid;
        Row& row = create_row_locked(*provider, *page);
        row.kind = RowKind::slider; row.label = std::move(label);
        row.help = std::move(help);
        row.enabled = description->enabled != 0; row.native_value = initial_value;
        row.public_value.store(initial_value);
        row.pending_value.store(initial_value);
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

ERUI_Result Registry::add_text_input(
    ERUI_ProviderHandle provider_handle,
    ERUI_PageHandle page_handle,
    const ERUI_TextInputDesc* description,
    ERUI_RowHandle* out_row) noexcept {
    // Bind feature availability to the version-specific registration
    // trampoline. In particular, do not inspect any 1.1-only pointer supplied
    // with a provider created through the frozen 1.0 table.
    {
        std::lock_guard lock(mutex_);
        const auto provider_it = providers_.find(provider_handle);
        if (provider_it == providers_.end()) return ERUI_INVALID_HANDLE;
        if (provider_it->second->api_version < ERUI_API_VERSION_1_1) {
            return ERUI_NOT_SUPPORTED;
        }
    }

    if (out_row) *out_row = ERUI_INVALID_ROW;
    if (!description || !field_available(description->size,
            ERUI_FIELD_END(ERUI_TextInputDesc, reserved)) ||
        description->flags != 0 || description->reserved != 0 ||
        !valid_view(description->label) ||
        !valid_view(description->help) ||
        !valid_view(description->initial_value) ||
        !valid_view(description->placeholder) ||
        (!description->changed_callback && description->user_data)) {
        return ERUI_INVALID_ARGUMENT;
    }

    const std::uint32_t maximum_length = description->maximum_length == 0
        ? erui::detail::text_input_default_maximum_length
        : description->maximum_length;
    if (maximum_length > erui::detail::text_input_maximum_length) {
        return ERUI_INVALID_ARGUMENT;
    }

    try {
        std::wstring label = copy_utf16(description->label, false);
        std::wstring help = copy_utf16(description->help);
        std::wstring initial_value = copy_utf16(description->initial_value);
        std::wstring placeholder = copy_utf16(description->placeholder);
        if (initial_value.size() > maximum_length) {
            return ERUI_INVALID_ARGUMENT;
        }
        auto state = std::make_unique<erui::detail::TextInputState>(
            std::move(initial_value), std::move(placeholder), maximum_length);

        std::lock_guard lock(mutex_);
        if (!open_) return ERUI_REGISTRATION_CLOSED;
        Provider* provider{};
        Page* page{};
        const ERUI_Result valid = validate_provider_and_page_locked(
            provider_handle, page_handle, provider, page);
        if (valid != ERUI_OK) return valid;
        if (provider->api_version < ERUI_API_VERSION_1_1) {
            return ERUI_NOT_SUPPORTED;
        }
        Row& row = create_row_locked(*provider, *page);
        row.kind = RowKind::text_input;
        row.label = std::move(label);
        row.help = std::move(help);
        row.text_changed_callback = description->changed_callback;
        row.user_data = description->user_data;
        row.text_input_state = std::move(state);
        if (out_row) *out_row = row.handle;
        note_activity_locked();
        return ERUI_OK;
    } catch (const std::bad_alloc&) {
        return ERUI_OUT_OF_MEMORY;
    } catch (...) {
        return ERUI_INVALID_ARGUMENT;
    }
}

ERUI_Result Registry::add_color_picker(
    ERUI_ProviderHandle provider_handle,
    ERUI_PageHandle page_handle,
    const ERUI_ColorPickerDesc* description,
    ERUI_RowHandle* out_row) noexcept {
    // Do not inspect a 1.1-only descriptor supplied through a provider bound
    // to the frozen 1.0 table.
    {
        std::lock_guard lock(mutex_);
        const auto provider_it = providers_.find(provider_handle);
        if (provider_it == providers_.end()) return ERUI_INVALID_HANDLE;
        if (provider_it->second->api_version < ERUI_API_VERSION_1_1) {
            return ERUI_NOT_SUPPORTED;
        }
    }

    if (out_row) *out_row = ERUI_INVALID_ROW;
    if (!description || !field_available(description->size,
            ERUI_FIELD_END(ERUI_ColorPickerDesc, enabled)) ||
        description->flags != 0 ||
        !valid_view(description->label) ||
        !valid_view(description->help) ||
        !valid_color(description->initial_value) ||
        !valid_enabled(description->enabled) ||
        (!description->changed_callback && description->user_data)) {
        return ERUI_INVALID_ARGUMENT;
    }

    try {
        std::wstring label = copy_utf16(description->label, false);
        std::wstring help = copy_utf16(description->help);
        auto state = std::make_unique<erui::detail::ColorPickerState>(
            to_internal_color(description->initial_value));

        std::lock_guard lock(mutex_);
        if (!open_) return ERUI_REGISTRATION_CLOSED;
        Provider* provider{};
        Page* page{};
        const ERUI_Result valid = validate_provider_and_page_locked(
            provider_handle, page_handle, provider, page);
        if (valid != ERUI_OK) return valid;
        if (provider->api_version < ERUI_API_VERSION_1_1) {
            return ERUI_NOT_SUPPORTED;
        }
        Row& row = create_row_locked(*provider, *page);
        row.kind = RowKind::color_picker;
        row.label = std::move(label);
        row.help = std::move(help);
        row.enabled = description->enabled != 0;
        row.color_changed_callback = description->changed_callback;
        row.user_data = description->user_data;
        row.color_picker_state = std::move(state);
        if (out_row) *out_row = row.handle;
        note_activity_locked();
        return ERUI_OK;
    } catch (const std::bad_alloc&) {
        return ERUI_OUT_OF_MEMORY;
    } catch (...) {
        return ERUI_INVALID_ARGUMENT;
    }
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
        if (erui::detail::valid_builtin_page(page->builtin_page)) {
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
        } else if (row.text_changed_callback) {
            callback = reinterpret_cast<const void*>(
                row.text_changed_callback);
        } else if (row.color_changed_callback) {
            callback = reinterpret_cast<const void*>(
                row.color_changed_callback);
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
    for (const auto& binding_record : provider.bindings) {
        const Binding& binding = *binding_record;
        if (!binding.activated_callback || !address_belongs_to_module(
                reinterpret_cast<const void*>(binding.activated_callback),
                provider.owner_module)) {
            return ERUI_CALLBACK_REJECTED;
        }
    }
    if (provider.assignments_changed_callback && !address_belongs_to_module(
            reinterpret_cast<const void*>(
                provider.assignments_changed_callback),
            provider.owner_module)) {
        return ERUI_CALLBACK_REJECTED;
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
        "Provider committed: id=%s priority=%d pages=%zu rows=%zu bindingSections=%zu bindings=%zu generation=%llu",
        provider.id.c_str(), provider.priority, provider.pages.size(),
        provider.rows.size(), provider.binding_sections.size(),
        provider.bindings.size(),
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
    row.pending_value.store(value, std::memory_order_release);
    row.pending_write.store(true, std::memory_order_release);
    row.public_value.store(value, std::memory_order_release);
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

ERUI_Result Registry::set_text_input_value(
    ERUI_ProviderHandle provider_handle,
    ERUI_RowHandle row_handle,
    const ERUI_Utf16View* value) noexcept {
    erui::detail::TextInputState* state{};
    {
        std::lock_guard lock(mutex_);
        const auto provider_it = providers_.find(provider_handle);
        if (provider_it == providers_.end()) return ERUI_INVALID_HANDLE;
        Provider& provider = *provider_it->second;
        if (provider.api_version < ERUI_API_VERSION_1_1) {
            return ERUI_NOT_SUPPORTED;
        }
        if (!provider.committed) return ERUI_INVALID_HANDLE;
        const auto row_it = provider.row_lookup.find(row_handle);
        if (row_it == provider.row_lookup.end()) return ERUI_INVALID_HANDLE;
        if (row_it->second->kind != RowKind::text_input ||
            !row_it->second->text_input_state) {
            return ERUI_INVALID_ARGUMENT;
        }
        state = row_it->second->text_input_state.get();
    }

    if (!value || !valid_view(*value)) return ERUI_INVALID_ARGUMENT;
    try {
        const std::wstring copied = copy_utf16(*value);
        return state->set_programmatic(copied)
            ? static_cast<ERUI_Result>(ERUI_OK)
            : static_cast<ERUI_Result>(ERUI_INVALID_ARGUMENT);
    } catch (const std::bad_alloc&) {
        return ERUI_OUT_OF_MEMORY;
    } catch (...) {
        return ERUI_INVALID_ARGUMENT;
    }
}

ERUI_Result Registry::get_text_input_value(
    ERUI_ProviderHandle provider_handle,
    ERUI_RowHandle row_handle,
    std::uint16_t* output,
    std::uint32_t output_capacity,
    std::uint32_t* out_length) noexcept {
    erui::detail::TextInputState* state{};
    {
        std::lock_guard lock(mutex_);
        const auto provider_it = providers_.find(provider_handle);
        if (provider_it == providers_.end()) return ERUI_INVALID_HANDLE;
        Provider& provider = *provider_it->second;
        if (provider.api_version < ERUI_API_VERSION_1_1) {
            return ERUI_NOT_SUPPORTED;
        }
        if (!provider.committed) return ERUI_INVALID_HANDLE;
        const auto row_it = provider.row_lookup.find(row_handle);
        if (row_it == provider.row_lookup.end()) return ERUI_INVALID_HANDLE;
        if (row_it->second->kind != RowKind::text_input ||
            !row_it->second->text_input_state) {
            return ERUI_INVALID_ARGUMENT;
        }
        state = row_it->second->text_input_state.get();
    }

    if (!out_length || (!output && output_capacity != 0)) {
        return ERUI_INVALID_ARGUMENT;
    }
    try {
        const erui::detail::TextInputState::Snapshot snapshot =
            state->snapshot();
        if (snapshot.value.size() >
            (std::numeric_limits<std::uint32_t>::max)()) {
            return ERUI_INTERNAL_ERROR;
        }
        const auto required = static_cast<std::uint32_t>(snapshot.value.size());
        *out_length = required;
        if (!output) return ERUI_OK;
        if (output_capacity < required) return ERUI_BUFFER_TOO_SMALL;
        if (required != 0) {
            std::memcpy(output, snapshot.value.data(),
                static_cast<std::size_t>(required) * sizeof(std::uint16_t));
        }
        return ERUI_OK;
    } catch (const std::bad_alloc&) {
        return ERUI_OUT_OF_MEMORY;
    } catch (...) {
        return ERUI_INTERNAL_ERROR;
    }
}

ERUI_Result Registry::set_color_picker_value(
    ERUI_ProviderHandle provider_handle,
    ERUI_RowHandle row_handle,
    const ERUI_Color* value) noexcept {
    erui::detail::ColorPickerState* state{};
    {
        std::lock_guard lock(mutex_);
        const auto provider_it = providers_.find(provider_handle);
        if (provider_it == providers_.end()) return ERUI_INVALID_HANDLE;
        Provider& provider = *provider_it->second;
        if (provider.api_version < ERUI_API_VERSION_1_1) {
            return ERUI_NOT_SUPPORTED;
        }
        if (!provider.committed) return ERUI_INVALID_HANDLE;
        const auto row_it = provider.row_lookup.find(row_handle);
        if (row_it == provider.row_lookup.end()) return ERUI_INVALID_HANDLE;
        if (row_it->second->kind != RowKind::color_picker ||
            !row_it->second->color_picker_state) {
            return ERUI_INVALID_ARGUMENT;
        }
        state = row_it->second->color_picker_state.get();
    }

    if (!value || !valid_color(*value)) return ERUI_INVALID_ARGUMENT;
    (void)state->set_programmatic(to_internal_color(*value));
    return ERUI_OK;
}

ERUI_Result Registry::get_color_picker_value(
    ERUI_ProviderHandle provider_handle,
    ERUI_RowHandle row_handle,
    ERUI_Color* out_value) noexcept {
    erui::detail::ColorPickerState* state{};
    {
        std::lock_guard lock(mutex_);
        const auto provider_it = providers_.find(provider_handle);
        if (provider_it == providers_.end()) return ERUI_INVALID_HANDLE;
        Provider& provider = *provider_it->second;
        if (provider.api_version < ERUI_API_VERSION_1_1) {
            return ERUI_NOT_SUPPORTED;
        }
        if (!provider.committed) return ERUI_INVALID_HANDLE;
        const auto row_it = provider.row_lookup.find(row_handle);
        if (row_it == provider.row_lookup.end()) return ERUI_INVALID_HANDLE;
        if (row_it->second->kind != RowKind::color_picker ||
            !row_it->second->color_picker_state) {
            return ERUI_INVALID_ARGUMENT;
        }
        state = row_it->second->color_picker_state.get();
    }

    if (!out_value) return ERUI_INVALID_ARGUMENT;
    *out_value = to_public_color(state->value());
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
                const std::uint8_t value =
                    row.pending_value.load(std::memory_order_acquire);
                // Mark the exact native transition before publishing it. The
                // worker's subsequent poll must update presentation without
                // reporting a client write as player input.
                row.silent_native_value.store(value, std::memory_order_relaxed);
                row.silent_native_transition.store(
                    true, std::memory_order_release);
                row.native_value = value;
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

    if (row->silent_native_transition.exchange(
            false, std::memory_order_acq_rel) &&
        row->silent_native_value.load(std::memory_order_acquire) == value) {
        return;
    }

    // A newer programmatic write remains the canonical value while it waits
    // for the next safe native update. Otherwise this is an observed player
    // change and becomes the new canonical value.
    if (!row->pending_write.load(std::memory_order_acquire)) {
        std::uint8_t expected =
            row->public_value.load(std::memory_order_acquire);
        if (row->public_value.compare_exchange_strong(
                expected,
                value,
                std::memory_order_acq_rel,
                std::memory_order_acquire) &&
            row->pending_write.load(std::memory_order_acquire)) {
            // A setter raced the observation after the first pending check.
            // Restore the newer client-owned canonical value.
            row->public_value.store(
                row->pending_value.load(std::memory_order_acquire),
                std::memory_order_release);
        }
    }
    invoke_value_callback(row->changed_callback, row->user_data, value);
}

void Registry::text_input_bridge(
    std::wstring_view value,
    void* user_data) noexcept {
    auto* row = static_cast<Row*>(user_data);
    if (!row || !row->text_changed_callback ||
        value.size() > (std::numeric_limits<std::uint32_t>::max)()) {
        return;
    }
    ERUI_TextInputChangeContext context{};
    context.size = sizeof(context);
    context.provider = row->provider;
    context.row = row->handle;
    context.value = {
        reinterpret_cast<const std::uint16_t*>(value.data()),
        static_cast<std::uint32_t>(value.size()),
        0,
    };
    invoke_text_input_callback(
        row->text_changed_callback, row->user_data, &context);
}

void Registry::color_picker_bridge(
    erui::detail::RgbColor value,
    void* user_data) noexcept {
    auto* row = static_cast<Row*>(user_data);
    if (!row || !row->color_changed_callback) return;
    ERUI_ColorPickerChangeContext context{};
    context.size = sizeof(context);
    context.provider = row->provider;
    context.row = row->handle;
    context.value = to_public_color(value);
    invoke_color_picker_callback(
        row->color_changed_callback, row->user_data, &context);
}

void Registry::input_action_bridge(
    std::uint32_t devices,
    void* user_data) noexcept {
    auto* const binding = static_cast<Binding*>(user_data);
    if (!binding || !binding->activated_callback ||
        devices == ERUI_INPUT_DEVICE_NONE ||
        (devices & ~static_cast<std::uint32_t>(
            ERUI_INPUT_DEVICE_ALL)) != 0) {
        return;
    }
    ERUI_InputActionActivatedContext context{};
    context.size = sizeof(context);
    context.provider = binding->provider;
    context.action = binding->handle;
    context.devices = devices;
    invoke_input_action_callback(
        binding->activated_callback,
        binding->user_data,
        &context);
}

void Registry::input_assignments_bridge(
    const ERUI_ActionInputs& previous,
    const ERUI_ActionInputs& current,
    ERUI_AssignmentChangeReason reason,
    ERUI_InputDevices changed_devices,
    void* user_data) noexcept {
    auto* const binding = static_cast<Binding*>(user_data);
    if (!binding || !erui::detail::valid_action_inputs(previous) ||
        !erui::detail::valid_action_inputs(current) ||
        changed_devices == ERUI_INPUT_DEVICE_NONE ||
        (changed_devices & ~static_cast<ERUI_InputDevices>(
            ERUI_INPUT_DEVICE_ALL)) != 0) {
        return;
    }

    ERUI_AssignmentsChangedCallback callback{};
    void* callback_user_data{};
    ERUI_ProviderHandle provider_handle{};
    {
        Registry& owner = registry();
        std::lock_guard lock(owner.mutex_);
        const auto provider_it = owner.providers_.find(binding->provider);
        if (provider_it == owner.providers_.end()) return;
        Provider& provider = *provider_it->second;
        const auto action_it = provider.binding_lookup.find(binding->handle);
        if (action_it == provider.binding_lookup.end() ||
            action_it->second != binding) {
            return;
        }
        callback = provider.assignments_changed_callback;
        callback_user_data = provider.assignments_changed_user_data;
        provider_handle = provider.handle;
    }

    if (!callback) return;
    ERUI_AssignmentChange change{};
    change.size = sizeof(change);
    change.action = binding->handle;
    change.action_id = {
        binding->id.data(),
        static_cast<std::uint32_t>(binding->id.size()),
        0,
    };
    change.previous = previous;
    change.current = current;
    change.reason = reason;
    change.changed_devices = changed_devices;

    ERUI_AssignmentsChangedContext context{};
    context.size = sizeof(context);
    context.provider = provider_handle;
    context.changes = &change;
    context.change_count = 1;
    invoke_assignments_changed_callback(
        callback, callback_user_data, &context);
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
    if (!erui::detail::valid_builtin_page(source.builtin_page)) {
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
        case RowKind::text_input:
            if (!row->text_input_state) {
                throw std::logic_error("TextInput state is missing");
            }
            destination.add_text_input(
                row->label,
                row->help,
                *row->text_input_state,
                {.callback = &Registry::text_input_bridge,
                    .user_data = row});
            break;
        case RowKind::color_picker:
            if (!row->color_picker_state) {
                throw std::logic_error("ColorPicker state is missing");
            }
            destination.add_color_picker(
                row->label,
                row->help,
                *row->color_picker_state,
                row->enabled,
                {.callback = &Registry::color_picker_bridge,
                    .user_data = row});
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
    frozen_ = true;

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
    for (std::size_t destination_index = 0;
         destination_index < erui::detail::builtin_page_count;
         ++destination_index) {
        erui::Page* destination{};
        for (const Provider* provider : ordered) {
            const ERUI_PageHandle source_handle =
                provider->builtin_pages[destination_index];
            if (source_handle == ERUI_INVALID_PAGE) continue;
            const auto source = provider->page_lookup.find(source_handle);
            if (source == provider->page_lookup.end()) {
                throw std::logic_error("provider built-in page missing");
            }
            const auto builtin_page =
                static_cast<erui::detail::BuiltinPage>(destination_index);
            if (source->second->builtin_page != builtin_page) {
                throw std::logic_error(
                    "provider built-in page destination mismatch");
            }
            if (!destination) {
                destination = &menu->builtin_page(builtin_page);
            }
            append_page_locked(*provider, *source->second, *destination);
        }
    }
    for (const Provider* provider : ordered) {
        for (const auto& section_record : provider->binding_sections) {
            const BindingSection& section = *section_record;
            if (section.bindings.empty()) continue;
            erui::InputBindingSection& destination =
                menu->add_input_binding_section(provider->id, section.label);
            for (const Binding* binding : section.bindings) {
                if (!binding) {
                    throw std::logic_error(
                        "provider input-binding section contains a null action");
                }
                destination.add_binding(
                    binding->handle,
                    binding->id,
                    binding->label,
                    binding->default_inputs,
                    binding->current_inputs,
                    {
                        .callback = &Registry::input_action_bridge,
                        .user_data = const_cast<Binding*>(binding),
                    },
                    {
                        .callback = &Registry::input_assignments_bridge,
                        .user_data = const_cast<Binding*>(binding),
                    });
            }
        }
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
        now - opened_at >= quiet_ms &&
        now - last_activity_tick_ >= quiet_ms;
    if (!maximum_reached && !quiet) return {};
    return freeze_and_build_locked();
}

} // namespace erui::host
