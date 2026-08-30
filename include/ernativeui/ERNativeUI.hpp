#pragma once

#include <ernativeui/erui.h>

#if !defined(_WIN32)
#  error ERNativeUI clients currently require Windows.
#endif

#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#  define ERUI_DETAIL_UNDEF_WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#  define NOMINMAX
#  define ERUI_DETAIL_UNDEF_NOMINMAX
#endif
#include <Windows.h>
#ifdef ERUI_DETAIL_UNDEF_NOMINMAX
#  undef NOMINMAX
#  undef ERUI_DETAIL_UNDEF_NOMINMAX
#endif
#ifdef ERUI_DETAIL_UNDEF_WIN32_LEAN_AND_MEAN
#  undef WIN32_LEAN_AND_MEAN
#  undef ERUI_DETAIL_UNDEF_WIN32_LEAN_AND_MEAN
#endif

#include <atomic>
#include <chrono>
#include <cstring>
#include <cstdint>
#include <exception>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <thread>
#include <utility>

namespace erui {

using RowHandle = ERUI_RowHandle;
using ButtonCallback = void (ERUI_CALL*)(void*) noexcept;
using ValueChangedCallback = void (ERUI_CALL*)(void*, std::uint8_t) noexcept;

enum class GameLanguage : std::uint32_t {
    unknown = ERUI_GAME_LANGUAGE_UNKNOWN,
    english = ERUI_GAME_LANGUAGE_ENGLISH,
    german = ERUI_GAME_LANGUAGE_GERMAN,
    french = ERUI_GAME_LANGUAGE_FRENCH,
    italian = ERUI_GAME_LANGUAGE_ITALIAN,
    korean = ERUI_GAME_LANGUAGE_KOREAN,
    spanish = ERUI_GAME_LANGUAGE_SPANISH,
    chinese_simplified = ERUI_GAME_LANGUAGE_CHINESE_SIMPLIFIED,
    chinese_traditional = ERUI_GAME_LANGUAGE_CHINESE_TRADITIONAL,
    russian = ERUI_GAME_LANGUAGE_RUSSIAN,
    thai = ERUI_GAME_LANGUAGE_THAI,
    japanese = ERUI_GAME_LANGUAGE_JAPANESE,
    polish = ERUI_GAME_LANGUAGE_POLISH,
    arabic = ERUI_GAME_LANGUAGE_ARABIC,
    portuguese_brazil = ERUI_GAME_LANGUAGE_PORTUGUESE_BRAZIL,
    spanish_latin_america = ERUI_GAME_LANGUAGE_SPANISH_LATIN_AMERICA,
};

struct LanguageInfo {
    ERUI_Result result{ERUI_NOT_SUPPORTED};
    GameLanguage known{GameLanguage::unknown};
    std::string identifier{};

    [[nodiscard]] bool available() const noexcept { return result == ERUI_OK; }
};

enum class AlertButtons : std::uint32_t {
    ok = ERUI_ALERT_BUTTONS_OK,
    cancel = ERUI_ALERT_BUTTONS_CANCEL,
    yes = ERUI_ALERT_BUTTONS_YES,
    no = ERUI_ALERT_BUTTONS_NO,
    ok_cancel = ERUI_ALERT_BUTTONS_OK_CANCEL,
    yes_no = ERUI_ALERT_BUTTONS_YES_NO,
    dismiss_only = ERUI_ALERT_BUTTONS_DISMISS_ONLY,
};

enum class AlertPlacement : std::uint32_t {
    bottom = ERUI_ALERT_PLACEMENT_BOTTOM,
    center = ERUI_ALERT_PLACEMENT_CENTER,
};

enum class AlertResponse : std::uint32_t {
    none = ERUI_ALERT_RESPONSE_NONE,
    primary = ERUI_ALERT_RESPONSE_PRIMARY,
    secondary = ERUI_ALERT_RESPONSE_SECONDARY,
    dismissed = ERUI_ALERT_RESPONSE_DISMISSED,
};

using AlertCallback = void (ERUI_CALL*)(
    void*, ERUI_Result, AlertResponse) noexcept;
using PageTitleFormatContext = ERUI_PageTitleFormatContext;
using PageTitleFormatter = ERUI_Result (ERUI_CALL*)(
    void*,
    const PageTitleFormatContext*,
    std::uint16_t*,
    std::uint32_t,
    std::uint32_t*) noexcept;
using StaticPageTitleFormatter = ERUI_Result (ERUI_CALL*)(
    const PageTitleFormatContext*,
    std::uint16_t*,
    std::uint32_t,
    std::uint32_t*) noexcept;

enum class ErrorCode {
    none,
    host_not_loaded,
    export_not_found,
    host_not_ready,
    host_failed,
    incompatible_api,
    registration_failed,
    builder_exception,
    operation_failed,
};

class Error {
public:
    Error() = default;
    Error(ErrorCode code, ERUI_Result native_result, std::wstring message)
        : code_(code), native_result_(native_result), message_(std::move(message)) {}

    ErrorCode code() const noexcept { return code_; }
    ERUI_Result native_result() const noexcept { return native_result_; }
    const std::wstring& message() const noexcept { return message_; }

private:
    ErrorCode code_{ErrorCode::none};
    ERUI_Result native_result_{ERUI_OK};
    std::wstring message_{};
};

struct ProviderOptions {
    std::string provider_id{};
    std::wstring display_name{};
    void* owner_module{};
    std::int32_t root_priority{100};
    std::chrono::milliseconds connect_timeout{5000};
};

struct SliderOptions {
    std::int32_t minimum{0};
    std::int32_t maximum{100};
    std::int32_t step{1};
    std::uint8_t initial_value{0};
    bool enabled{true};
};

struct ChoiceOptions {
    const std::wstring_view* values{};
    std::size_t count{};
    std::uint8_t initial_index{0};
};

struct AlertOptions {
    AlertButtons buttons{AlertButtons::ok};
    AlertPlacement placement{AlertPlacement::bottom};
};

struct PagePresentation {
    std::wstring_view menu_title{};
    std::wstring_view page_title{};
    PageTitleFormatter formatter{};
    void* user_data{};
};

inline ERUI_Result write_page_title(
    std::wstring_view title,
    std::uint16_t* output,
    std::uint32_t output_capacity,
    std::uint32_t* out_length) noexcept {
    if (!out_length) return ERUI_INVALID_ARGUMENT;
    *out_length = 0;
    if (title.empty() || !output ||
        title.size() > static_cast<std::size_t>(output_capacity) ||
        title.size() > (std::numeric_limits<std::uint32_t>::max)()) {
        return ERUI_INVALID_ARGUMENT;
    }
    static_assert(sizeof(wchar_t) == sizeof(std::uint16_t),
        "ERNativeUI requires Windows UTF-16 wchar_t");
    std::memcpy(output, title.data(), title.size() * sizeof(std::uint16_t));
    *out_length = static_cast<std::uint32_t>(title.size());
    return ERUI_OK;
}

namespace detail {

// The raw C callback carries ERUI_AlertResponse as uint32_t. Adapt it inside
// the client module instead of invoking a typed C++ callback through a
// reinterpreted function pointer. The same client-side thunk that allocates
// this state also destroys it after the asynchronous completion.
struct AlertCallbackState {
    AlertCallback callback{};
    void* user_data{};
};

inline void ERUI_CALL alert_callback_thunk(
    void* opaque_state,
    ERUI_Result result,
    ERUI_AlertResponse response) noexcept {
    std::unique_ptr<AlertCallbackState> state(
        static_cast<AlertCallbackState*>(opaque_state));
    if (!state || !state->callback) return;
    state->callback(
        state->user_data,
        result,
        static_cast<AlertResponse>(response));
}

inline bool view_size_fits(std::size_t size) noexcept {
    return size <= (std::numeric_limits<std::uint32_t>::max)();
}

inline ERUI_StringView string_view(std::string_view value) noexcept {
    ERUI_StringView result{};
    result.data = value.data();
    result.length = static_cast<std::uint32_t>(value.size());
    return result;
}

inline ERUI_Utf16View utf16_view(std::wstring_view value) noexcept {
    static_assert(sizeof(wchar_t) == sizeof(std::uint16_t),
        "ERNativeUI requires Windows UTF-16 wchar_t");
    ERUI_Utf16View result{};
    result.data = reinterpret_cast<const std::uint16_t*>(value.data());
    result.length = static_cast<std::uint32_t>(value.size());
    return result;
}

inline const wchar_t* result_name(ERUI_Result result) noexcept {
    switch (result) {
    case ERUI_OK: return L"success";
    case ERUI_INVALID_ARGUMENT: return L"invalid argument";
    case ERUI_HOST_NOT_READY: return L"host not ready";
    case ERUI_HOST_FAILED: return L"host failed";
    case ERUI_UNSUPPORTED_VERSION: return L"unsupported API version";
    case ERUI_DUPLICATE_PROVIDER_ID: return L"duplicate provider ID";
    case ERUI_INVALID_HANDLE: return L"invalid handle";
    case ERUI_ALREADY_COMMITTED: return L"provider already committed";
    case ERUI_REGISTRATION_CLOSED: return L"startup registration is closed";
    case ERUI_OUT_OF_MEMORY: return L"out of memory";
    case ERUI_CALLBACK_REJECTED: return L"provider callback/module validation failed";
    case ERUI_INTERNAL_ERROR: return L"internal host error";
    case ERUI_QUEUE_FULL: return L"alert queue is full";
    case ERUI_NOT_SUPPORTED: return L"feature is unavailable for this game build";
    default: return L"unknown host result";
    }
}

struct ConnectionResult {
    ERUI_Api api{};
    Error error{};
    bool connected{};
};

inline LanguageInfo read_game_language(const ERUI_Api& api) {
    LanguageInfo language{};
    ERUI_GameLanguageInfo native_language{};
    native_language.size = sizeof(native_language);
    language.result = api.get_game_language
        ? api.get_game_language(&native_language)
        : static_cast<ERUI_Result>(ERUI_NOT_SUPPORTED);
    if (language.result != ERUI_OK) return language;
    language.known = static_cast<GameLanguage>(native_language.known_language);
    if (native_language.identifier.length != 0 &&
        !native_language.identifier.data) {
        language.result = ERUI_INTERNAL_ERROR;
        return language;
    }
    language.identifier.assign(
        native_language.identifier.data ? native_language.identifier.data : "",
        native_language.identifier.length);
    return language;
}

inline ConnectionResult connect(std::chrono::milliseconds timeout) noexcept {
    ConnectionResult result{};
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    bool saw_host = false;
    do {
        HMODULE host = GetModuleHandleW(L"ERNativeUI.dll");
        if (host) {
            saw_host = true;
            const FARPROC symbol = GetProcAddress(host, "ERUI_GetApi");
            if (!symbol) {
                result.error = Error(ErrorCode::export_not_found, ERUI_INTERNAL_ERROR,
                    L"ERNativeUI.dll does not export ERUI_GetApi.");
                return result;
            }
            ERUI_GetApiFn get_api{};
            static_assert(sizeof(get_api) == sizeof(symbol),
                "Windows function pointers must use one representation");
            std::memcpy(&get_api, &symbol, sizeof(get_api));
            result.api = {};
            result.api.size = sizeof(result.api);
            const ERUI_Result status = get_api(ERUI_API_VERSION_CURRENT, &result.api);
            if (status == ERUI_OK) {
                const bool complete = result.api.register_provider &&
                    result.api.add_button && result.api.add_toggle &&
                    result.api.add_slider && result.api.add_inline_choice &&
                    result.api.add_popup_choice && result.api.add_submenu &&
                    result.api.set_page_presentation &&
                    result.api.commit_provider && result.api.abort_provider &&
                    result.api.set_row_value && result.api.get_row_value &&
                    result.api.enqueue_alert && result.api.get_game_language &&
                    (result.api.capabilities & ERUI_CAP_PAGE_PRESENTATION) != 0 &&
                    (result.api.capabilities & ERUI_CAP_ALERT) != 0 &&
                    (result.api.capabilities & ERUI_CAP_INLINE_CHOICE) != 0 &&
                    (result.api.capabilities & ERUI_CAP_POPUP_CHOICE) != 0 &&
                    (result.api.capabilities & ERUI_CAP_GAME_LANGUAGE) != 0;
                if (!complete ||
                    result.api.api_version != ERUI_API_VERSION_CURRENT) {
                    result.error = Error(ErrorCode::incompatible_api,
                        ERUI_UNSUPPORTED_VERSION,
                        L"ERNativeUI returned an incomplete or incompatible API table.");
                    return result;
                }
                result.connected = true;
                return result;
            }
            if (status == ERUI_HOST_FAILED) {
                result.error = Error(ErrorCode::host_failed, status,
                    L"ERNativeUI host initialization failed; inspect ERNativeUI.log.");
                return result;
            }
            if (status != ERUI_HOST_NOT_READY) {
                result.error = Error(ErrorCode::incompatible_api, status,
                    std::wstring(L"ERNativeUI API negotiation failed: ") + result_name(status));
                return result;
            }
        }
        if (std::chrono::steady_clock::now() >= deadline) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    } while (true);

    result.error = saw_host
        ? Error(ErrorCode::host_not_ready, ERUI_HOST_NOT_READY,
            L"ERNativeUI.dll did not become ready before the connection timeout.")
        : Error(ErrorCode::host_not_loaded, ERUI_HOST_NOT_READY,
            L"ERNativeUI.dll was not loaded; this mod's configuration menu is disabled.");
    return result;
}

class Draft {
public:
    Draft(ERUI_Api api, ERUI_ProviderHandle provider, ERUI_PageHandle root,
        LanguageInfo language)
        : api_(api), provider_(provider), root_(root),
          language_(std::move(language)) {}
    ~Draft() = default;
    Draft(const Draft&) = delete;
    Draft& operator=(const Draft&) = delete;

    [[nodiscard]] bool open() const noexcept {
        return open_.load(std::memory_order_acquire);
    }

    void close() noexcept {
        open_.store(false, std::memory_order_release);
    }

    void mark_committed() noexcept {
        close();
    }

    void record(ERUI_Result result, const wchar_t* operation) noexcept {
        if (result == ERUI_OK || failed_ || !open()) return;
        failed_ = true;
        try {
            error_ = Error(ErrorCode::operation_failed, result,
                std::wstring(operation) + L" failed: " + result_name(result));
        } catch (...) {
            error_ = Error(ErrorCode::operation_failed, result, L"Menu operation failed.");
        }
    }

    ERUI_Api api_{};
    ERUI_ProviderHandle provider_{};
    ERUI_PageHandle root_{};
    LanguageInfo language_{};
    bool failed_{};
    Error error_{};

private:
    std::atomic_bool open_{true};
};

class DraftGuard {
public:
    explicit DraftGuard(std::shared_ptr<Draft> draft) noexcept
        : draft_(std::move(draft)) {}
    ~DraftGuard() { if (draft_) draft_->close(); }
    DraftGuard(const DraftGuard&) = delete;
    DraftGuard& operator=(const DraftGuard&) = delete;
private:
    std::shared_ptr<Draft> draft_{};
};

class ProviderAbortGuard {
public:
    ProviderAbortGuard(ERUI_Api api, ERUI_ProviderHandle provider) noexcept
        : api_(api), provider_(provider) {}
    ~ProviderAbortGuard() {
        if (active_ && provider_ != ERUI_INVALID_PROVIDER) {
            api_.abort_provider(provider_);
        }
    }
    ProviderAbortGuard(const ProviderAbortGuard&) = delete;
    ProviderAbortGuard& operator=(const ProviderAbortGuard&) = delete;
    void release() noexcept { active_ = false; }
private:
    ERUI_Api api_{};
    ERUI_ProviderHandle provider_{};
    bool active_{true};
};

} // namespace detail

[[nodiscard]] inline LanguageInfo query_game_language(
    std::chrono::milliseconds timeout = std::chrono::milliseconds{5000}) noexcept {
    try {
        const auto connection = detail::connect(timeout);
        if (!connection.connected) {
            LanguageInfo unavailable{};
            unavailable.result = connection.error.native_result();
            return unavailable;
        }
        return detail::read_game_language(connection.api);
    } catch (...) {
        LanguageInfo unavailable{};
        unavailable.result = ERUI_OUT_OF_MEMORY;
        return unavailable;
    }
}

class Page {
public:
    Page() = default;

    RowHandle add_button(
        std::wstring_view label,
        std::wstring_view help,
        ButtonCallback callback,
        void* user_data = nullptr,
        bool enabled = true) noexcept {
        if (!draft_ || !draft_->open() || draft_->failed_) return ERUI_INVALID_ROW;
        if (!detail::view_size_fits(label.size()) ||
            !detail::view_size_fits(help.size())) {
            draft_->record(ERUI_INVALID_ARGUMENT, L"add_button text");
            return ERUI_INVALID_ROW;
        }
        ERUI_ButtonDesc desc{};
        desc.size = sizeof(desc);
        desc.label = detail::utf16_view(label);
        desc.help = detail::utf16_view(help);
        desc.callback = reinterpret_cast<ERUI_ButtonCallback>(callback);
        desc.user_data = user_data;
        desc.enabled = enabled ? 1u : 0u;
        ERUI_RowHandle row{};
        draft_->record(draft_->api_.add_button(
            draft_->provider_, page_, &desc, &row), L"add_button");
        return row;
    }

    template <void (*Function)() noexcept>
    RowHandle add_button(
        std::wstring_view label,
        std::wstring_view help,
        bool enabled = true) noexcept {
        return add_button(label, help, &button_thunk<Function>, nullptr, enabled);
    }

    RowHandle add_toggle(
        std::wstring_view label,
        std::wstring_view help,
        std::uint8_t initial_value,
        ValueChangedCallback callback = nullptr,
        void* user_data = nullptr,
        bool enabled = true) noexcept {
        if (!draft_ || !draft_->open() || draft_->failed_) return ERUI_INVALID_ROW;
        if (!detail::view_size_fits(label.size()) ||
            !detail::view_size_fits(help.size())) {
            draft_->record(ERUI_INVALID_ARGUMENT, L"add_toggle text");
            return ERUI_INVALID_ROW;
        }
        ERUI_ToggleDesc desc{};
        desc.size = sizeof(desc);
        desc.label = detail::utf16_view(label);
        desc.help = detail::utf16_view(help);
        desc.changed_callback = reinterpret_cast<ERUI_ValueChangedCallback>(callback);
        desc.user_data = user_data;
        desc.initial_value = initial_value;
        desc.enabled = enabled ? 1u : 0u;
        ERUI_RowHandle row{};
        draft_->record(draft_->api_.add_toggle(
            draft_->provider_, page_, &desc, &row), L"add_toggle");
        return row;
    }

    RowHandle add_slider(
        std::wstring_view label,
        std::wstring_view help,
        const SliderOptions& options,
        ValueChangedCallback callback = nullptr,
        void* user_data = nullptr) noexcept {
        if (!draft_ || !draft_->open() || draft_->failed_) return ERUI_INVALID_ROW;
        if (!detail::view_size_fits(label.size()) ||
            !detail::view_size_fits(help.size())) {
            draft_->record(ERUI_INVALID_ARGUMENT, L"add_slider text");
            return ERUI_INVALID_ROW;
        }
        ERUI_SliderDesc desc{};
        desc.size = sizeof(desc);
        desc.label = detail::utf16_view(label);
        desc.help = detail::utf16_view(help);
        desc.changed_callback = reinterpret_cast<ERUI_ValueChangedCallback>(callback);
        desc.user_data = user_data;
        desc.minimum = options.minimum;
        desc.maximum = options.maximum;
        desc.step = options.step;
        desc.initial_value = options.initial_value;
        desc.enabled = options.enabled ? 1u : 0u;
        ERUI_RowHandle row{};
        draft_->record(draft_->api_.add_slider(
            draft_->provider_, page_, &desc, &row), L"add_slider");
        return row;
    }

    RowHandle add_inline_choice(
        std::wstring_view label,
        std::wstring_view help,
        const ChoiceOptions& options,
        ValueChangedCallback callback = nullptr,
        void* user_data = nullptr) noexcept {
        return add_choice_impl(
            label, help, options, callback, user_data,
            draft_ ? draft_->api_.add_inline_choice : nullptr,
            L"add_inline_choice");
    }

    RowHandle add_popup_choice(
        std::wstring_view label,
        std::wstring_view help,
        const ChoiceOptions& options,
        ValueChangedCallback callback = nullptr,
        void* user_data = nullptr) noexcept {
        return add_choice_impl(
            label, help, options, callback, user_data,
            draft_ ? draft_->api_.add_popup_choice : nullptr,
            L"add_popup_choice");
    }

    Page add_submenu(
        std::wstring_view label,
        std::wstring_view help,
        std::wstring_view page_title = {},
        std::wstring_view page_help = {},
        bool enabled = true) noexcept {
        if (!draft_ || !draft_->open() || draft_->failed_) return {};
        if (!detail::view_size_fits(label.size()) ||
            !detail::view_size_fits(help.size()) ||
            !detail::view_size_fits(page_title.size()) ||
            !detail::view_size_fits(page_help.size())) {
            draft_->record(ERUI_INVALID_ARGUMENT, L"add_submenu text");
            return {};
        }
        ERUI_SubmenuDesc desc{};
        desc.size = sizeof(desc);
        desc.label = detail::utf16_view(label);
        desc.help = detail::utf16_view(help);
        desc.page_title = detail::utf16_view(page_title);
        desc.page_help = detail::utf16_view(page_help);
        desc.enabled = enabled ? 1u : 0u;
        ERUI_PageHandle child{};
        ERUI_RowHandle row{};
        draft_->record(draft_->api_.add_submenu(
            draft_->provider_, page_, &desc, &child, &row), L"add_submenu");
        return Page(draft_, child);
    }

    Page& set_presentation(const PagePresentation& presentation) noexcept {
        if (!draft_ || !draft_->open() || draft_->failed_) return *this;
        if (!detail::view_size_fits(presentation.menu_title.size()) ||
            !detail::view_size_fits(presentation.page_title.size())) {
            draft_->record(ERUI_INVALID_ARGUMENT, L"set_page_presentation text");
            return *this;
        }
        ERUI_PagePresentationDesc desc{};
        desc.size = sizeof(desc);
        desc.menu_title = detail::utf16_view(presentation.menu_title);
        desc.page_title = detail::utf16_view(presentation.page_title);
        desc.formatter = reinterpret_cast<ERUI_PageTitleFormatter>(
            presentation.formatter);
        desc.user_data = presentation.user_data;
        draft_->record(draft_->api_.set_page_presentation(
            draft_->provider_, page_, &desc), L"set_page_presentation");
        return *this;
    }

    Page& set_presentation(
        std::wstring_view menu_title,
        std::wstring_view page_title = {}) noexcept {
        PagePresentation presentation{};
        presentation.menu_title = menu_title;
        presentation.page_title = page_title;
        return set_presentation(presentation);
    }

    template <StaticPageTitleFormatter Formatter>
    Page& set_presentation(
        std::wstring_view menu_title = {},
        std::wstring_view page_title = {}) noexcept {
        PagePresentation presentation{};
        presentation.menu_title = menu_title;
        presentation.page_title = page_title;
        presentation.formatter = &page_title_formatter_thunk<Formatter>;
        return set_presentation(presentation);
    }

    bool valid() const noexcept {
        return draft_ && draft_->open() && page_ != ERUI_INVALID_PAGE;
    }

private:
    friend class Menu;
    Page(std::shared_ptr<detail::Draft> draft, ERUI_PageHandle page) noexcept
        : draft_(std::move(draft)), page_(page) {}

    template <void (*Function)() noexcept>
    static void ERUI_CALL button_thunk(void*) noexcept { Function(); }

    using AddChoiceFunction = ERUI_Result (ERUI_CALL*)(
        ERUI_ProviderHandle,
        ERUI_PageHandle,
        const ERUI_ChoiceDesc*,
        ERUI_RowHandle*);

    RowHandle add_choice_impl(
        std::wstring_view label,
        std::wstring_view help,
        const ChoiceOptions& options,
        ValueChangedCallback callback,
        void* user_data,
        AddChoiceFunction function,
        const wchar_t* operation) noexcept {
        if (!draft_ || !draft_->open() || draft_->failed_) {
            return ERUI_INVALID_ROW;
        }
        if (!function || !detail::view_size_fits(label.size()) ||
            !detail::view_size_fits(help.size()) || !options.values ||
            options.count == 0 || options.count > 32 ||
            options.initial_index >= options.count) {
            draft_->record(ERUI_INVALID_ARGUMENT, operation);
            return ERUI_INVALID_ROW;
        }
        try {
            std::vector<ERUI_Utf16View> native_options;
            native_options.reserve(options.count);
            for (std::size_t index = 0; index < options.count; ++index) {
                if (options.values[index].empty() ||
                    !detail::view_size_fits(options.values[index].size())) {
                    draft_->record(ERUI_INVALID_ARGUMENT, operation);
                    return ERUI_INVALID_ROW;
                }
                native_options.push_back(
                    detail::utf16_view(options.values[index]));
            }
            ERUI_ChoiceDesc description{};
            description.size = sizeof(description);
            description.label = detail::utf16_view(label);
            description.help = detail::utf16_view(help);
            description.changed_callback =
                reinterpret_cast<ERUI_ValueChangedCallback>(callback);
            description.user_data = user_data;
            description.options = native_options.data();
            description.option_count =
                static_cast<std::uint32_t>(native_options.size());
            description.initial_index = options.initial_index;
            ERUI_RowHandle row{};
            draft_->record(function(
                draft_->provider_, page_, &description, &row), operation);
            return row;
        } catch (...) {
            draft_->record(ERUI_OUT_OF_MEMORY, operation);
            return ERUI_INVALID_ROW;
        }
    }

    template <StaticPageTitleFormatter Formatter>
    static ERUI_Result ERUI_CALL page_title_formatter_thunk(
        void*,
        const PageTitleFormatContext* context,
        std::uint16_t* output,
        std::uint32_t output_capacity,
        std::uint32_t* out_length) noexcept {
        return Formatter(context, output, output_capacity, out_length);
    }

    std::shared_ptr<detail::Draft> draft_{};
    ERUI_PageHandle page_{};
};

class Menu {
public:
    Page root() noexcept { return Page(draft_, draft_ ? draft_->root_ : 0); }
    [[nodiscard]] const LanguageInfo& game_language() const noexcept {
        static const LanguageInfo unavailable{};
        return draft_ ? draft_->language_ : unavailable;
    }
private:
    template <typename Builder> friend class BuilderAccess;
    template <typename Builder>
    friend class RegistrationInvoker;
    explicit Menu(std::shared_ptr<detail::Draft> draft) noexcept
        : draft_(std::move(draft)) {}
    std::shared_ptr<detail::Draft> draft_{};
};

class Registration {
public:
    Registration() = default;

    ERUI_Result set_value(RowHandle row, std::uint8_t value) const noexcept {
        return valid() ? api_.set_row_value(provider_, row, value)
            : static_cast<ERUI_Result>(ERUI_INVALID_HANDLE);
    }
    ERUI_Result get_value(RowHandle row, std::uint8_t& value) const noexcept {
        return valid() ? api_.get_row_value(provider_, row, &value)
            : static_cast<ERUI_Result>(ERUI_INVALID_HANDLE);
    }
    ERUI_Result alert(
        std::wstring_view message,
        AlertOptions options,
        AlertCallback callback = nullptr,
        void* user_data = nullptr) const noexcept {
        if (!valid()) return ERUI_INVALID_HANDLE;
        if (message.empty() || !detail::view_size_fits(message.size()) ||
            (!callback && user_data) ||
            static_cast<std::uint32_t>(options.buttons) >
                ERUI_ALERT_BUTTONS_DISMISS_ONLY ||
            static_cast<std::uint32_t>(options.placement) >
                ERUI_ALERT_PLACEMENT_CENTER) {
            return ERUI_INVALID_ARGUMENT;
        }
        std::unique_ptr<detail::AlertCallbackState> callback_state{};
        if (callback) {
            try {
                callback_state =
                    std::make_unique<detail::AlertCallbackState>();
                callback_state->callback = callback;
                callback_state->user_data = user_data;
            } catch (...) {
                return ERUI_OUT_OF_MEMORY;
            }
        }
        ERUI_AlertDesc description{};
        description.size = sizeof(description);
        description.message = detail::utf16_view(message);
        description.callback = callback
            ? &detail::alert_callback_thunk
            : nullptr;
        description.user_data = callback_state.get();
        description.buttons = static_cast<ERUI_AlertButtons>(options.buttons);
        description.placement = static_cast<ERUI_AlertPlacement>(
            options.placement);
        const ERUI_Result result = api_.enqueue_alert(provider_, &description);
        if (result == ERUI_OK) callback_state.release();
        return result;
    }
    ERUI_Result alert(
        std::wstring_view message,
        AlertCallback callback = nullptr,
        void* user_data = nullptr) const noexcept {
        return alert(message, AlertOptions{}, callback, user_data);
    }
    bool valid() const noexcept { return provider_ != ERUI_INVALID_PROVIDER; }
    ERUI_ProviderHandle provider_handle() const noexcept { return provider_; }
    [[nodiscard]] const LanguageInfo& game_language() const noexcept {
        return language_;
    }

private:
    template <typename Builder>
    friend class RegistrationInvoker;
    Registration(ERUI_Api api, ERUI_ProviderHandle provider,
        LanguageInfo language)
        : api_(api), provider_(provider), language_(std::move(language)) {}
    ERUI_Api api_{};
    ERUI_ProviderHandle provider_{};
    LanguageInfo language_{};
};

class RegistrationResult {
public:
    explicit operator bool() const noexcept { return success_; }
    bool success() const noexcept { return success_; }
    const Error& error() const noexcept { return error_; }
    Registration& value() noexcept { return registration_; }
    const Registration& value() const noexcept { return registration_; }

private:
    template <typename Builder>
    friend class RegistrationInvoker;
    bool success_{};
    Error error_{};
    Registration registration_{};
};

template <typename Builder>
class RegistrationInvoker {
public:
    static RegistrationResult invoke(
        const ProviderOptions& options,
        Builder&& builder) noexcept {
        RegistrationResult output{};
        try {
            if (!detail::view_size_fits(options.provider_id.size()) ||
                !detail::view_size_fits(options.display_name.size())) {
                output.error_ = Error(ErrorCode::registration_failed,
                    ERUI_INVALID_ARGUMENT,
                    L"Provider ID or display name is too large.");
                return output;
            }
            const auto connection = detail::connect(options.connect_timeout);
            if (!connection.connected) {
                output.error_ = connection.error;
                return output;
            }
            LanguageInfo language = detail::read_game_language(connection.api);
            ERUI_ProviderDesc desc{};
            desc.size = sizeof(desc);
            desc.api_version = connection.api.api_version;
            desc.root_priority = options.root_priority;
            desc.owner_module = options.owner_module;
            desc.provider_id = detail::string_view(options.provider_id);
            desc.display_name = detail::utf16_view(options.display_name);
            ERUI_ProviderHandle provider{};
            ERUI_PageHandle root{};
            const ERUI_Result started = connection.api.register_provider(
                &desc, &provider, &root);
            if (started != ERUI_OK) {
                output.error_ = Error(ErrorCode::registration_failed, started,
                    std::wstring(L"Provider registration failed: ") +
                    detail::result_name(started));
                return output;
            }

            detail::ProviderAbortGuard provider_guard(connection.api, provider);
            auto draft = std::make_shared<detail::Draft>(
                connection.api, provider, root, language);
            detail::DraftGuard draft_guard(draft);
            Menu menu(draft);
            try {
                builder(menu);
            } catch (...) {
                output.error_ = Error(ErrorCode::builder_exception,
                    ERUI_INTERNAL_ERROR, L"The menu builder threw an exception.");
                return output;
            }
            if (draft->failed_) {
                output.error_ = draft->error_;
                return output;
            }
            const ERUI_Result committed = connection.api.commit_provider(provider);
            if (committed != ERUI_OK) {
                output.error_ = Error(ErrorCode::registration_failed, committed,
                    std::wstring(L"Provider commit failed: ") +
                    detail::result_name(committed));
                return output;
            }
            draft->mark_committed();
            provider_guard.release();
            output.registration_ = Registration(
                connection.api, provider, std::move(language));
            output.success_ = true;
            return output;
        } catch (...) {
            output.error_ = Error(ErrorCode::operation_failed,
                ERUI_INTERNAL_ERROR, L"Client wrapper ran out of memory or failed unexpectedly.");
            return output;
        }
    }
};

template <typename Builder>
RegistrationResult register_menu(
    const ProviderOptions& options,
    Builder&& builder) noexcept {
    return RegistrationInvoker<Builder>::invoke(options, std::forward<Builder>(builder));
}

} // namespace erui
