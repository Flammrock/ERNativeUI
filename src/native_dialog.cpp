#include "native_dialog.hpp"

#include "dialog_modal_state.hpp"
#include "native_dialog_presentation.hpp"
#include "color_picker.hpp"
#include "runtime_log.hpp"

#include <Windows.h>
#include <safetyhook.hpp>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace erui::native {
namespace {

constexpr std::uint32_t kDialogTextBase = 0x1F0000u;
constexpr std::size_t kMaximumQueuedAlerts = 64;
constexpr std::size_t kMaximumMessageLength = 4096;
constexpr std::uint64_t kRetirementGraceMs = 10000;
constexpr std::uint64_t kRetiredCollectionPeriodMs = 1000;

using LowerFrontendUpdateFn = void(__fastcall*)(void*, float, std::uint8_t*);
using TitleTopUpdateFn = void(__fastcall*)(void*, float, std::uint8_t*);
using PopupUpdateFn = void(__fastcall*)(void*, float, void*);
using MenuWindowJobPollFn = void*(__fastcall*)(void*, void*, void*);
using DialogInitializeFn = void(__fastcall*)(void*);
using DialogSetFieldsFn = void(__fastcall*)(
    void*, std::uint32_t, std::uint32_t, std::uint32_t);
using DialogSetOptionFn = void(__fastcall*)(void*, std::uint32_t);
using DialogBuildFn = void*(__fastcall*)(
    void*, void*, std::uint32_t, void*);
// Installs an intrusive task into one of CSPopupMenu's native blocking slots,
// returns a temporary reference through RDX, and consumes/nulls the pointer
// supplied through R8.
using BlockingJobInstallFn = void*(__fastcall*)(
    void**, void**, void**);
using ReleaseReferenceFn = int(__fastcall*)(void*);
using DestroyReferencedObjectFn = void(__fastcall*)(void*);

struct AlertRequest {
    std::uint64_t sequence{};
    std::uint32_t message_id{};
    std::wstring message{};
    ERUI_AlertCallback callback{};
    void* user_data{};
    void* native_job{};
    bool owns_native_job{};
    NativeDialogPresentation presentation{};
    std::uint64_t lower_gate_baseline{};
    std::uint64_t title_gate_baseline{};
    std::atomic<ERUI_AlertResponse> response{ERUI_ALERT_RESPONSE_NONE};
};

struct Completion {
    ERUI_AlertCallback callback{};
    void* user_data{};
    ERUI_Result result{ERUI_INTERNAL_ERROR};
    ERUI_AlertResponse response{ERUI_ALERT_RESPONSE_NONE};
};

struct RetiredText {
    std::shared_ptr<AlertRequest> request{};
    std::uint64_t expires_at{};
};

SafetyHookInline g_lower_frontend_update_hook{};
std::atomic<LowerFrontendUpdateFn> g_original_lower_frontend_update{};
SafetyHookInline g_title_top_update_hook{};
std::atomic<TitleTopUpdateFn> g_original_title_top_update{};
SafetyHookInline g_popup_update_hook{};
std::atomic<PopupUpdateFn> g_original_popup_update{};
SafetyHookInline g_menu_window_job_poll_hook{};
std::atomic<MenuWindowJobPollFn> g_original_menu_window_job_poll{};
std::atomic_bool g_available{};
void** g_fe_manager_slot{};
DialogInitializeFn g_initialize{};
DialogSetFieldsFn g_set_fields{};
DialogSetOptionFn g_set_fixed_option{};
DialogSetOptionFn g_set_global_option{};
DialogBuildFn g_build{};
BlockingJobInstallFn g_install_blocking_job{};
ReleaseReferenceFn g_release_reference{};

std::mutex g_mutex{};
std::deque<std::shared_ptr<AlertRequest>> g_pending{};
std::shared_ptr<AlertRequest> g_active{};
std::unordered_map<std::uint32_t, std::shared_ptr<AlertRequest>> g_texts{};
std::vector<RetiredText> g_retired{};
std::vector<Completion> g_completions{};
std::vector<Completion> g_dispatch_buffer{};
std::uint64_t g_next_sequence{1};
std::uint32_t g_next_message_id{kDialogTextBase};
DialogModalState g_modal_state{};
std::atomic_bool g_queue_work{};
std::atomic_flag g_scheduling = ATOMIC_FLAG_INIT;
std::atomic<std::uint64_t> g_next_collection_tick{};
std::atomic<std::uint64_t> g_lower_gate_frames{};
std::atomic<std::uint64_t> g_title_gate_frames{};
thread_local bool g_dispatching_popup_input{};

void release_job(void* job) noexcept {
    if (!job || !g_release_reference) return;
#if defined(_MSC_VER)
    __try {
#endif
        auto* const bytes = static_cast<std::byte*>(job);
        const int previous_count = g_release_reference(bytes + 0x08);
        if (previous_count == 1) {
            auto** const vtable = *reinterpret_cast<void***>(job);
            const auto destroy = reinterpret_cast<DestroyReferencedObjectFn>(
                vtable ? vtable[0] : nullptr);
            if (destroy) destroy(job);
        } else if (previous_count <= 0) {
            erui::detail::logf(
                erui::LogLevel::warning,
                "Native alert job reported an invalid reference count during release: job=%p previous=%d",
                job, previous_count);
        }
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        erui::detail::logf(
            erui::LogLevel::warning,
            "Native alert job release faulted safely: job=%p seh=0x%08lX",
            job, static_cast<unsigned long>(GetExceptionCode()));
    }
#endif
}

void retire_request(
    const std::shared_ptr<AlertRequest>& request,
    ERUI_Result result,
    std::uint64_t now) noexcept {
    if (!request) return;
    if (request->owns_native_job) {
        release_job(request->native_job);
    }
    request->native_job = nullptr;
    request->owns_native_job = false;

    bool callback_queued = !request->callback;
    bool text_retired = false;
    {
        std::lock_guard lock(g_mutex);
        if (g_active == request) g_active.reset();
        if (request->callback) {
            try {
                g_completions.push_back({
                    request->callback,
                    request->user_data,
                    result,
                    result == ERUI_OK
                        ? request->response.load(std::memory_order_acquire)
                        : ERUI_ALERT_RESPONSE_NONE,
                });
                callback_queued = true;
            } catch (...) {
                // Never allow allocation failure to unwind through a native
                // game hook. The callback cannot be dispatched safely here.
            }
        }
        try {
            g_retired.push_back({request, now + kRetirementGraceMs});
            text_retired = true;
        } catch (...) {
            // Keep the g_texts entry indefinitely. A small leak under genuine
            // OOM is safer than invalidating text still referenced by Scaleform.
        }
    }
    if (!callback_queued) {
        erui::detail::logf(
            erui::LogLevel::warning,
            "Native alert completion callback could not be queued after an allocation failure");
    }
    if (!text_retired) {
        erui::detail::logf(
            erui::LogLevel::warning,
            "Native alert text retirement could not be queued; storage retained for process lifetime");
    }
}

void collect_retired(std::uint64_t now) noexcept {
    std::lock_guard lock(g_mutex);
    auto output = g_retired.begin();
    for (auto input = g_retired.begin(); input != g_retired.end(); ++input) {
        if (input->expires_at <= now) {
            g_texts.erase(input->request->message_id);
            continue;
        }
        if (output != input) *output = std::move(*input);
        ++output;
    }
    g_retired.erase(output, g_retired.end());
}

struct PopupAccess {
    void* manager{};
    void* context{};
    void* blocking_job{};
};

PopupAccess popup_access() noexcept {
#if defined(_MSC_VER)
    __try {
#endif
        void* const fe_manager = g_fe_manager_slot ? *g_fe_manager_slot : nullptr;
        void* const popup_manager = fe_manager
            ? *reinterpret_cast<void**>(
                static_cast<std::byte*>(fe_manager) + 0x80)
            : nullptr;
        return popup_manager
            ? PopupAccess{
                popup_manager,
                static_cast<std::byte*>(popup_manager) + 0x10,
                *reinterpret_cast<void**>(
                    static_cast<std::byte*>(popup_manager) + 0x298)}
            : PopupAccess{};
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return {};
    }
#endif
}

enum class ScheduleResult : std::uint8_t {
    waiting_for_popup_manager,
    scheduled,
    failed,
};

ScheduleResult invoke_schedule_job(
    AlertRequest* request,
    unsigned long& exception) noexcept {
    exception = 0;
    // The character color editor uses MenuWindowJob rather than CSPopupMenu's
    // +0x298 slot, so the native slot alone cannot serialize these transports.
    // Keep the FIFO request active and retry after the color editor reaches a
    // terminal state; no alert is rejected or dropped.
    if (color_picker_session_active()) {
        return ScheduleResult::waiting_for_popup_manager;
    }
    const PopupAccess popup = popup_access();
    if (!popup.manager || !popup.context) {
        return ScheduleResult::waiting_for_popup_manager;
    }

    if (popup.blocking_job) {
        // +0x298 is CSPopupMenu's native globally blocking task slot. Never
        // replace the game's own failed-save/error dialog or another alert.
        return ScheduleResult::waiting_for_popup_manager;
    }

    void* submitted_job = nullptr;
    void* consumable_job = nullptr;
    void* installed_reference = nullptr;
    bool submit_started = false;
#if defined(_MSC_VER)
    __try {
#endif
        alignas(16) std::array<std::byte, 0x28> descriptor{};
        alignas(16) std::array<void*, 2> build_result{};
        alignas(16) std::array<void*, 2> install_result{};
        g_initialize(descriptor.data());
        g_set_fields(
            descriptor.data(), request->message_id,
            request->presentation.primary_label,
            request->presentation.secondary_label);
        g_set_fixed_option(descriptor.data(), 2u);
        // This helper ignores RCX and writes the generic-dialog option into
        // Elden Ring's UI singleton. The builder consumes it immediately.
        g_set_global_option(descriptor.data(), 1u);
        g_build(
            build_result.data(), popup.context,
            request->presentation.builder_kind,
            descriptor.data());
        submitted_job = build_result[0];
        if (!submitted_job) {
            return ScheduleResult::failed;
        }

        request->native_job = submitted_job;
        request->owns_native_job = true;

        // 0x7AA2E0 is the intrusive assignment used by Elden Ring's own
        // +0x298 blocking-dialog path. It consumes the input pointer, gives the
        // slot an owning reference, and returns one temporary reference.
        consumable_job = submitted_job;
        submit_started = true;
        g_install_blocking_job(
            reinterpret_cast<void**>(
                static_cast<std::byte*>(popup.manager) + 0x298),
            install_result.data(),
            &consumable_job);
        request->owns_native_job = consumable_job != nullptr;
        installed_reference = install_result[0];
        if (consumable_job != nullptr) {
            release_job(consumable_job);
            request->native_job = nullptr;
            request->owns_native_job = false;
            if (installed_reference) {
                release_job(installed_reference);
            }
            return ScheduleResult::failed;
        }

        // Publish the owner before the job. Readers acquire the job first and
        // can then safely reconcile the exact popup slot even if native code
        // clears it between two sampled frames.
        request->lower_gate_baseline =
            g_lower_gate_frames.load(std::memory_order_relaxed);
        request->title_gate_baseline =
            g_title_gate_frames.load(std::memory_order_relaxed);
        g_modal_state.publish(popup.manager, submitted_job);
        if (installed_reference) {
            release_job(installed_reference);
        }
        return ScheduleResult::scheduled;
#if defined(_MSC_VER)
    } __except (exception = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        // If submit did not consume the builder reference, it is still ours.
        // If it did, CSPopupMenu owns the composed task and releasing the raw
        // pointer here would race the scheduler.
        // A fault inside a consuming native call leaves its exact progress
        // unknowable. Once submission begins, never release the input in the
        // handler: a rare leak is safer than double-releasing a task already
        // owned by CSPopupMenu.
        if (!submit_started && consumable_job) {
            release_job(consumable_job);
        }
        request->native_job = nullptr;
        request->owns_native_job = false;
        if (installed_reference) {
            release_job(installed_reference);
        }
        return ScheduleResult::failed;
    }
#endif
}

std::shared_ptr<AlertRequest> acquire_active() noexcept {
    std::lock_guard lock(g_mutex);
    if (!g_active && !g_pending.empty()) {
        g_active = std::move(g_pending.front());
        g_pending.pop_front();
    }
    return g_active;
}

void collect_retired_periodically() noexcept {
    const std::uint64_t now = GetTickCount64();
    std::uint64_t next = g_next_collection_tick.load(std::memory_order_relaxed);
    if (now < next || !g_next_collection_tick.compare_exchange_strong(
            next, now + kRetiredCollectionPeriodMs,
            std::memory_order_acq_rel, std::memory_order_relaxed)) {
        return;
    }
    collect_retired(now);
}

void schedule_service() noexcept {
    if (!g_available.load(std::memory_order_acquire) ||
        !g_queue_work.load(std::memory_order_acquire) ||
        g_scheduling.test_and_set(std::memory_order_acquire)) {
        return;
    }

    struct SchedulingGuard {
        ~SchedulingGuard() { g_scheduling.clear(std::memory_order_release); }
    } guard{};

    // Consume exactly the wake observed by this pump. Producers that enqueue
    // concurrently after this exchange leave a fresh wake behind instead of
    // having it overwritten by a later store(false).
    if (!g_queue_work.exchange(false, std::memory_order_acq_rel)) return;

    const std::shared_ptr<AlertRequest> request = acquire_active();
    if (!request) return;

    if (request->native_job) {
        // A producer may enqueue another alert while the current dialog is
        // open. That marks the queue as ready, but the active native job must
        // never be submitted a second time. Its completion will wake the FIFO
        // so the next request can be scheduled.
        return;
    }

    if (!g_initialize || !g_set_fields || !g_set_fixed_option ||
        !g_set_global_option || !g_build || !g_install_blocking_job) {
        retire_request(request, ERUI_INTERNAL_ERROR, GetTickCount64());
        g_queue_work.store(true, std::memory_order_release);
        return;
    }

    unsigned long exception{};
    const ScheduleResult scheduled = invoke_schedule_job(request.get(), exception);
    if (scheduled == ScheduleResult::waiting_for_popup_manager) {
        g_queue_work.store(true, std::memory_order_release);
        return;
    }
    if (scheduled == ScheduleResult::failed) {
        erui::detail::logf(
            erui::LogLevel::warning,
            "Native alert scheduling failed safely: sequence=%llu seh=0x%08lX",
            static_cast<unsigned long long>(request->sequence),
            exception);
        retire_request(request, ERUI_INTERNAL_ERROR, GetTickCount64());
        g_queue_work.store(true, std::memory_order_release);
        return;
    }

    erui::detail::logf(
        erui::LogLevel::info,
        "Native alert scheduled in blocking slot: sequence=%llu messageId=0x%X job=%p",
        static_cast<unsigned long long>(request->sequence),
        static_cast<unsigned>(request->message_id),
        request->native_job);
}

std::shared_ptr<AlertRequest> active_request_for_job(void* job) noexcept {
    std::lock_guard lock(g_mutex);
    return g_active && g_active->native_job == job ? g_active : nullptr;
}

bool read_popup_blocking_job(void* popup, void*& slot_job) noexcept {
    slot_job = nullptr;
    if (!popup) return false;
#if defined(_MSC_VER)
    __try {
#endif
        const auto* const bytes = static_cast<const std::byte*>(popup);
        slot_job = *reinterpret_cast<void* const*>(bytes + 0x298);
        return true;
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#endif
}

bool owns_current_popup_job(void* popup) noexcept {
    void* slot_job = nullptr;
    return read_popup_blocking_job(popup, slot_job) &&
        g_modal_state.owns(popup, slot_job);
}

void observe_composite_completion(void* popup) noexcept {
    void* slot_job = nullptr;
    if (!read_popup_blocking_job(popup, slot_job)) return;

    // Reconcile through the instance that is actually updating. Reading a
    // remembered owner after the frontend replaces its popup singleton could
    // otherwise inspect stale storage and leave modal ownership latched.
    const DialogModalReconcileResult completion =
        g_modal_state.reconcile(popup, slot_job);
    if (!completion.job) return;

    const std::shared_ptr<AlertRequest> request =
        active_request_for_job(completion.job);
    if (!request) return;

    const bool dismissed = completion.loss == DialogModalLoss::dismissed;
    const std::uint64_t lower_gate_frames =
        g_lower_gate_frames.load(std::memory_order_relaxed) -
        request->lower_gate_baseline;
    const std::uint64_t title_gate_frames =
        g_title_gate_frames.load(std::memory_order_relaxed) -
        request->title_gate_baseline;
    erui::detail::logf(
        dismissed ? erui::LogLevel::info : erui::LogLevel::warning,
        dismissed
            ? "Native alert dismissed from blocking slot: sequence=%llu lowerGateFrames=%llu titleGateFrames=%llu"
            : "Native alert lost blocking-slot ownership: sequence=%llu lowerGateFrames=%llu titleGateFrames=%llu",
        static_cast<unsigned long long>(request->sequence),
        static_cast<unsigned long long>(lower_gate_frames),
        static_cast<unsigned long long>(title_gate_frames));
    retire_request(
        request,
        dismissed ? ERUI_OK : ERUI_INTERNAL_ERROR,
        GetTickCount64());
    g_queue_work.store(true, std::memory_order_release);
}

void __fastcall lower_frontend_update_detour(
    void* frontend,
    float elapsed_seconds,
    std::uint8_t* input_enabled) noexcept {
    const LowerFrontendUpdateFn original =
        g_original_lower_frontend_update.load(std::memory_order_acquire);
    if (!original) return;

    // The outer frontend frame updates this lower tree before it updates
    // CSPopupMenu. Its ordinary input-enabled byte reaches only one branch;
    // title and character-creation siblings continue consuming X/O and
    // navigation. Pause the complete lower tree while our exact blocking
    // task owns CSPopupMenu. The outer frame still proceeds to the separately
    // invoked popup update below, so the dialog keeps exclusive input focus.
    const PopupAccess popup = popup_access();
    if (popup.manager &&
        g_modal_state.owns(popup.manager, popup.blocking_job)) {
        g_lower_gate_frames.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    original(frontend, elapsed_seconds, input_enabled);
}

void __fastcall title_top_update_detour(
    void* title,
    float elapsed_seconds,
    std::uint8_t* input_enabled) noexcept {
    const TitleTopUpdateFn original =
        g_original_title_top_update.load(std::memory_order_acquire);
    if (!original) return;

    // TitleTopDialog is a child of CSPopupMenu rather than the lower frontend
    // tree. Giving CSPopupMenu a private true byte keeps an owned native alert
    // interactive, but also reaches this underlying title task. Give only
    // TitleTopDialog a private false byte while the exact +0x298 job belongs
    // to ERNativeUI. Its visual/state frame still runs, the surrounding task
    // list performs its normal FD4Time wrapper teardown, and CSPopupMenu
    // continues on to poll the alert—including its dismissal frame—without
    // letting movement or X/O bleed through to the title screen.
    const PopupAccess popup = popup_access();
    if (popup.manager &&
        g_modal_state.owns(popup.manager, popup.blocking_job)) {
        g_title_gate_frames.fetch_add(1, std::memory_order_relaxed);
        std::uint8_t title_input_disabled = 0;
        original(title, elapsed_seconds, &title_input_disabled);
        return;
    }
    original(title, elapsed_seconds, input_enabled);
}

void __fastcall popup_update_detour(
    void* popup, float elapsed_seconds, void* frontend_enabled) noexcept {
    const PopupUpdateFn original =
        g_original_popup_update.load(std::memory_order_acquire);
    if (!original) return;
    const bool owned_before_update = owns_current_popup_job(popup);
    std::uint8_t popup_enabled = 1;
    void* const forwarded_enabled =
        owned_before_update && frontend_enabled
            ? static_cast<void*>(&popup_enabled)
            : frontend_enabled;
    // CSMenuManImp has already passed its original false byte to the lower
    // frontend because +0x298 is occupied. Give only CSPopupMenu a private
    // true value so our blocking dialog accepts OK/Cancel without re-enabling
    // the page below it or the later menu layers.
    struct PopupInputScope {
        bool previous;
        explicit PopupInputScope(bool active) noexcept
            : previous(g_dispatching_popup_input) {
            if (active) g_dispatching_popup_input = true;
        }
        ~PopupInputScope() { g_dispatching_popup_input = previous; }
    } popup_input_scope{owned_before_update};
    original(popup, elapsed_seconds, forwarded_enabled);
    observe_composite_completion(popup);

    // Run after CSPopupMenu finishes its normal frame. Submitting here avoids
    // mutating current_top_menu_job while one of its children is being polled,
    // and provides a once-per-frontend-frame pump even when no dialog job is
    // currently active.
    collect_retired_periodically();
    schedule_service();
}

struct NativePollStatus {
    std::uint32_t kind{};
    bool readable{};
};

NativePollStatus read_native_poll_status(void* status) noexcept {
    if (!status) return {};
#if defined(_MSC_VER)
    __try {
#endif
        const auto* const words = static_cast<const std::uint32_t*>(status);
        return {words[0], true};
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return {};
    }
#endif
}

void* __fastcall menu_window_job_poll_detour(
    void* job, void* scheduler_state, void* result) noexcept {
    const MenuWindowJobPollFn original =
        g_original_menu_window_job_poll.load(std::memory_order_acquire);
    if (!original) return scheduler_state;
    void* const status = original(job, scheduler_state, result);

    const std::shared_ptr<AlertRequest> request = active_request_for_job(job);
    const NativePollStatus native_status = read_native_poll_status(scheduler_state);
    if (request && native_status.readable) {
        const ERUI_AlertResponse response = normalize_native_alert_response(
            request->presentation.button_count, native_status.kind);
        if (response != ERUI_ALERT_RESPONSE_NONE) {
            ERUI_AlertResponse expected = ERUI_ALERT_RESPONSE_NONE;
            request->response.compare_exchange_strong(
                expected, response,
                std::memory_order_release, std::memory_order_relaxed);
        }
    }
    observe_color_picker_job_poll(job, scheduler_state);
    return status;
}

bool resolve_transport(
    const ModuleView& game,
    std::uint8_t*& poll_target,
    std::uint8_t*& lower_frontend_update_target,
    std::uint8_t*& title_top_update_target,
    std::uint8_t*& popup_update_target) {
    // This wrapper is a semantic anchor for the real CSPopupMenu submission
    // route: it resolves CSMenuManImp::popup_menu at +0x80, builds a job using
    // popup+0x10, then submits it through the current_top_menu_job scheduler.
    constexpr std::string_view scheduler_wrapper_pattern =
        "40 56 48 83 EC 40 48 C7 44 24 28 FE FF FF FF 48 89 5C 24 50 "
        "4C 8B C2 48 8B D9 48 8B 05 ?? ?? ?? ?? 48 85 C0 "
        "0F 84 ?? ?? ?? ?? 48 8B B0 80 00 00 00";
    const ScanResult scheduler_scan =
        game.scan_executable(scheduler_wrapper_pattern);
    if (!scheduler_scan.address || scheduler_scan.matches != 1) return false;
    g_fe_manager_slot = reinterpret_cast<void**>(resolve_rip_relative(
        game, scheduler_scan.address + 0x1A, 3, 7));

    // Native failed-save dialogs install their task into CSPopupMenu+0x298
    // through this consuming intrusive-pointer assignment.
    constexpr std::string_view blocking_install_pattern =
        "48 8D 54 24 58 48 8D 8B 98 02 00 00 "
        "E8 ?? ?? ?? ?? 48 8B 5C 24 58";
    const ScanResult blocking_install_scan =
        game.scan_executable(blocking_install_pattern);
    if (!blocking_install_scan.address ||
        blocking_install_scan.matches != 1) return false;
    g_install_blocking_job = reinterpret_cast<BlockingJobInstallFn>(
        resolve_rel32_call(game, blocking_install_scan.address + 0x0C));

    // Anchor CSPopupMenu::update through its callsite in CSMenuManImp's frame
    // update. The preceding lower-menu call makes this sequence distinctive.
    constexpr std::string_view popup_update_call_pattern =
        "48 8B 0D ?? ?? ?? ?? 48 85 C9 74 0E 4C 8D 45 67 "
        "F3 0F 10 4F 08 E8 ?? ?? ?? ?? "
        "48 8B 8B 80 00 00 00 48 85 C9 74 0E 4C 8D 45 67 "
        "F3 0F 10 4F 08 E8 ?? ?? ?? ?? 48 8D 45 AF";
    const ScanResult update_scan =
        game.scan_executable(popup_update_call_pattern);
    if (!update_scan.address || update_scan.matches != 1) return false;
    lower_frontend_update_target = resolve_rel32_call(
        game, update_scan.address + 0x15);
    popup_update_target = resolve_rel32_call(
        game, update_scan.address + 0x2F);

    // This dispatcher has one caller: the outer frontend update anchored
    // above. Validate its concrete entry as a second exact-build guard before
    // allowing a modal early return there.
    constexpr std::string_view lower_frontend_update_pattern =
        "48 8B C4 55 57 41 56 48 8D 68 A1 "
        "48 81 EC B0 00 00 00 "
        "48 C7 45 B7 FE FF FF FF "
        "48 89 58 10 48 89 70 18 0F 29 70 D8";
    if (!lower_frontend_update_target ||
        !game.matches(
            lower_frontend_update_target,
            lower_frontend_update_pattern)) {
        return false;
    }

    // CS::TitleTopDialog::frame is a CSPopupMenu child and therefore needs a
    // narrower modal gate than the lower frontend dispatcher above. RTTI
    // identifies this exact entry as TitleTopDialog's vtable slot 2.
    constexpr std::string_view title_top_update_pattern =
        "48 8B C4 55 56 57 48 81 EC D0 01 00 00 "
        "48 C7 44 24 28 FE FF FF FF 48 89 58 20 0F 29 70 D8 "
        "48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 84 24 B0 01 00 00";
    const ScanResult title_top_scan =
        game.scan_executable(title_top_update_pattern);
    if (!title_top_scan.address || title_top_scan.matches != 1) return false;
    title_top_update_target = title_top_scan.address;

    constexpr std::string_view wrapper_pattern =
        "44 89 4C 24 20 44 89 44 24 18 48 89 54 24 10 48 89 4C 24 08 "
        "53 55 56 57 41 54 41 56 41 57 48 83 EC 60";
    const ScanResult wrapper_scan = game.scan_executable(wrapper_pattern);
    if (!wrapper_scan.address || wrapper_scan.matches != 1) return false;
    g_initialize = reinterpret_cast<DialogInitializeFn>(resolve_rel32_call(
        game, wrapper_scan.address + 0x1A7));
    g_set_fields = reinterpret_cast<DialogSetFieldsFn>(resolve_rel32_call(
        game, wrapper_scan.address + 0x1BE));
    g_set_fixed_option = reinterpret_cast<DialogSetOptionFn>(resolve_rel32_call(
        game, wrapper_scan.address + 0x1D0));
    g_set_global_option = reinterpret_cast<DialogSetOptionFn>(resolve_rel32_call(
        game, wrapper_scan.address + 0x209));
    g_build = reinterpret_cast<DialogBuildFn>(resolve_rel32_call(
        game, wrapper_scan.address + 0x226));

    constexpr std::string_view poll_pattern =
        "4C 89 44 24 18 55 53 56 57 41 54 41 56 41 57 48 8D 6C 24 D9 "
        "48 81 EC A0 00 00 00 48 C7 45 A7 FE FF FF FF 4D 8B F0 4C 8B E2 "
        "48 8B F9 48 83 B9 30 01 00 00 00";
    const ScanResult poll_scan = game.scan_executable(poll_pattern);
    if (!poll_scan.address || poll_scan.matches != 1) return false;
    poll_target = poll_scan.address;
    g_release_reference = reinterpret_cast<ReleaseReferenceFn>(
        resolve_rel32_call(game, poll_target + 0x114));

    return lower_frontend_update_target && title_top_update_target &&
        popup_update_target &&
        g_fe_manager_slot &&
        g_initialize && g_set_fields &&
        g_set_fixed_option && g_set_global_option && g_build &&
        g_install_blocking_job && g_release_reference;
}

} // namespace

bool install_native_dialog_transport(const ModuleView& game) noexcept {
    remove_native_dialog_transport();
    try {
        std::uint8_t* poll_target{};
        std::uint8_t* lower_frontend_update_target{};
        std::uint8_t* title_top_update_target{};
        std::uint8_t* popup_update_target{};
        if (!resolve_transport(
                game,
                poll_target,
                lower_frontend_update_target,
                title_top_update_target,
                popup_update_target)) {
            erui::detail::logf(
                erui::LogLevel::warning,
                "Native alert transport unavailable: one or more semantic signatures were unresolved");
            return false;
        }
        {
            std::lock_guard lock(g_mutex);
            g_retired.reserve(kMaximumQueuedAlerts * 2);
            g_completions.reserve(kMaximumQueuedAlerts);
            g_dispatch_buffer.reserve(kMaximumQueuedAlerts);
        }
        auto update_hook = SafetyHookInline::create(
            popup_update_target,
            reinterpret_cast<void*>(&popup_update_detour),
            SafetyHookInline::StartDisabled);
        if (!update_hook) {
            erui::detail::logf(
                erui::LogLevel::warning,
                "Native alert transport unavailable: CSPopupMenu update hook creation failed (SafetyHook error type %u)",
                static_cast<unsigned>(update_hook.error().type));
            return false;
        }
        auto lower_frontend_update_hook = SafetyHookInline::create(
            lower_frontend_update_target,
            reinterpret_cast<void*>(&lower_frontend_update_detour),
            SafetyHookInline::StartDisabled);
        if (!lower_frontend_update_hook) {
            erui::detail::logf(
                erui::LogLevel::warning,
                "Native alert transport unavailable: lower frontend update hook creation failed (SafetyHook error type %u)",
                static_cast<unsigned>(
                    lower_frontend_update_hook.error().type));
            return false;
        }
        auto title_top_update_hook = SafetyHookInline::create(
            title_top_update_target,
            reinterpret_cast<void*>(&title_top_update_detour),
            SafetyHookInline::StartDisabled);
        if (!title_top_update_hook) {
            erui::detail::logf(
                erui::LogLevel::warning,
                "Native alert transport unavailable: TitleTopDialog update hook creation failed (SafetyHook error type %u)",
                static_cast<unsigned>(
                    title_top_update_hook.error().type));
            return false;
        }
        auto poll_hook = SafetyHookInline::create(
            poll_target,
            reinterpret_cast<void*>(&menu_window_job_poll_detour),
            SafetyHookInline::StartDisabled);
        if (!poll_hook) {
            erui::detail::logf(
                erui::LogLevel::warning,
                "Native alert transport unavailable: MenuWindowJob hook creation failed (SafetyHook error type %u)",
                static_cast<unsigned>(poll_hook.error().type));
            return false;
        }
        g_popup_update_hook = std::move(*update_hook);
        g_original_popup_update.store(
            g_popup_update_hook.original<PopupUpdateFn>(),
            std::memory_order_release);
        if (auto enabled = g_popup_update_hook.enable(); !enabled) {
            erui::detail::logf(
                erui::LogLevel::warning,
                "Native alert transport unavailable: CSPopupMenu update hook enable failed (SafetyHook error type %u)",
                static_cast<unsigned>(enabled.error().type));
            g_popup_update_hook.reset();
            g_original_popup_update.store(nullptr, std::memory_order_release);
            return false;
        }
        g_menu_window_job_poll_hook = std::move(*poll_hook);
        g_original_menu_window_job_poll.store(
            g_menu_window_job_poll_hook.original<MenuWindowJobPollFn>(),
            std::memory_order_release);
        if (auto enabled = g_menu_window_job_poll_hook.enable(); !enabled) {
            erui::detail::logf(
                erui::LogLevel::warning,
                "Native alert transport unavailable: MenuWindowJob hook enable failed (SafetyHook error type %u)",
                static_cast<unsigned>(enabled.error().type));
            g_menu_window_job_poll_hook.reset();
            g_original_menu_window_job_poll.store(
                nullptr, std::memory_order_release);
            g_popup_update_hook.reset();
            g_original_popup_update.store(nullptr, std::memory_order_release);
            return false;
        }
        g_lower_frontend_update_hook =
            std::move(*lower_frontend_update_hook);
        g_original_lower_frontend_update.store(
            g_lower_frontend_update_hook.original<LowerFrontendUpdateFn>(),
            std::memory_order_release);
        if (auto enabled = g_lower_frontend_update_hook.enable(); !enabled) {
            erui::detail::logf(
                erui::LogLevel::warning,
                "Native alert transport unavailable: lower frontend update hook enable failed (SafetyHook error type %u)",
                static_cast<unsigned>(enabled.error().type));
            g_lower_frontend_update_hook.reset();
            g_original_lower_frontend_update.store(
                nullptr, std::memory_order_release);
            g_menu_window_job_poll_hook.reset();
            g_original_menu_window_job_poll.store(
                nullptr, std::memory_order_release);
            g_popup_update_hook.reset();
            g_original_popup_update.store(nullptr, std::memory_order_release);
            return false;
        }
        g_title_top_update_hook = std::move(*title_top_update_hook);
        g_original_title_top_update.store(
            g_title_top_update_hook.original<TitleTopUpdateFn>(),
            std::memory_order_release);
        if (auto enabled = g_title_top_update_hook.enable(); !enabled) {
            erui::detail::logf(
                erui::LogLevel::warning,
                "Native alert transport unavailable: TitleTopDialog update hook enable failed (SafetyHook error type %u)",
                static_cast<unsigned>(enabled.error().type));
            g_title_top_update_hook.reset();
            g_original_title_top_update.store(
                nullptr, std::memory_order_release);
            g_lower_frontend_update_hook.reset();
            g_original_lower_frontend_update.store(
                nullptr, std::memory_order_release);
            g_menu_window_job_poll_hook.reset();
            g_original_menu_window_job_poll.store(
                nullptr, std::memory_order_release);
            g_popup_update_hook.reset();
            g_original_popup_update.store(nullptr, std::memory_order_release);
            return false;
        }
        g_available.store(true, std::memory_order_release);
        erui::detail::logf(
            erui::LogLevel::info,
            "Native alert transport installed: blockingInstall=%p lowerFrontendUpdate=%p titleTopUpdate=%p popupUpdate=%p MenuWindowJob=%p RVA=0x%llX queueCapacity=%zu",
            reinterpret_cast<void*>(g_install_blocking_job),
            lower_frontend_update_target,
            title_top_update_target,
            popup_update_target,
            poll_target,
            static_cast<unsigned long long>(
                reinterpret_cast<std::uintptr_t>(poll_target) -
                reinterpret_cast<std::uintptr_t>(game.base())),
            kMaximumQueuedAlerts);
        return true;
    } catch (...) {
        remove_native_dialog_transport();
        return false;
    }
}

void remove_native_dialog_transport() noexcept {
    g_available.store(false, std::memory_order_release);
    g_title_top_update_hook.reset();
    g_original_title_top_update.store(nullptr, std::memory_order_release);
    g_lower_frontend_update_hook.reset();
    g_original_lower_frontend_update.store(
        nullptr, std::memory_order_release);
    g_popup_update_hook.reset();
    g_original_popup_update.store(nullptr, std::memory_order_release);
    g_menu_window_job_poll_hook.reset();
    g_original_menu_window_job_poll.store(nullptr, std::memory_order_release);
    g_fe_manager_slot = nullptr;
    g_initialize = nullptr;
    g_set_fields = nullptr;
    g_set_fixed_option = nullptr;
    g_set_global_option = nullptr;
    g_build = nullptr;
    g_install_blocking_job = nullptr;
    g_release_reference = nullptr;
    g_modal_state.reset();
    g_lower_gate_frames.store(0, std::memory_order_relaxed);
    g_title_gate_frames.store(0, std::memory_order_relaxed);
    g_queue_work.store(false, std::memory_order_release);
    g_scheduling.clear(std::memory_order_release);
    g_next_collection_tick.store(0, std::memory_order_release);

    std::lock_guard lock(g_mutex);
    g_pending.clear();
    g_active.reset();
    g_texts.clear();
    g_retired.clear();
    g_completions.clear();
    g_dispatch_buffer.clear();
}

bool native_dialog_available() noexcept {
    return g_available.load(std::memory_order_acquire);
}

bool native_dialog_owns_menu_input() noexcept {
    return g_modal_state.active();
}

bool native_dialog_dispatching_popup_input() noexcept {
    return g_dispatching_popup_input;
}

ERUI_Result enqueue_native_alert(
    std::wstring message,
    ERUI_AlertButtons buttons,
    ERUI_AlertPlacement placement,
    ERUI_AlertCallback callback,
    void* user_data) noexcept {
    if (!native_dialog_available()) return ERUI_NOT_SUPPORTED;
    NativeDialogPresentation presentation{};
    if (message.empty() || message.size() > kMaximumMessageLength ||
        !resolve_native_dialog_presentation(buttons, placement, presentation) ||
        (!callback && user_data)) {
        return ERUI_INVALID_ARGUMENT;
    }
    try {
        auto request = std::make_shared<AlertRequest>();
        request->message = std::move(message);
        request->presentation = presentation;
        request->callback = callback;
        request->user_data = user_data;

        std::lock_guard lock(g_mutex);
        const std::size_t outstanding = g_pending.size() + (g_active ? 1u : 0u);
        if (outstanding >= kMaximumQueuedAlerts) return ERUI_QUEUE_FULL;
        if (g_next_message_id == 0 || g_next_message_id > 0x7FFFFFFFu) {
            return ERUI_INTERNAL_ERROR;
        }
        request->sequence = g_next_sequence++;
        request->message_id = g_next_message_id++;
        const auto [text_entry, inserted] =
            g_texts.emplace(request->message_id, request);
        if (!inserted) return ERUI_INTERNAL_ERROR;
        try {
            g_pending.push_back(std::move(request));
        } catch (...) {
            g_texts.erase(text_entry);
            throw;
        }
        g_queue_work.store(true, std::memory_order_release);
        return ERUI_OK;
    } catch (const std::bad_alloc&) {
        return ERUI_OUT_OF_MEMORY;
    } catch (...) {
        return ERUI_INTERNAL_ERROR;
    }
}

const wchar_t* lookup_native_dialog_text(std::uint32_t message_id) noexcept {
    if (message_id < kDialogTextBase) return nullptr;
    std::lock_guard lock(g_mutex);
    const auto found = g_texts.find(message_id);
    return found == g_texts.end() ? nullptr : found->second->message.c_str();
}

bool invoke_callback_cpp(const Completion& completion) noexcept {
    try {
        completion.callback(
            completion.user_data, completion.result, completion.response);
        return true;
    } catch (...) {
        return false;
    }
}

void invoke_callback_safely(const Completion* completion) noexcept {
#if defined(_MSC_VER)
    __try {
#endif
        if (!invoke_callback_cpp(*completion)) {
            erui::detail::logf(
                erui::LogLevel::warning,
                "Native alert completion callback threw a C++ exception; ignored");
        }
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        erui::detail::logf(
            erui::LogLevel::warning,
            "Native alert completion callback faulted safely: seh=0x%08lX",
            static_cast<unsigned long>(GetExceptionCode()));
    }
#endif
}

void dispatch_native_dialog_callbacks() noexcept {
    {
        std::lock_guard lock(g_mutex);
        g_dispatch_buffer.clear();
        g_dispatch_buffer.swap(g_completions);
    }
    // The producer immediately receives the already-reserved empty buffer.
    // Invoke callbacks outside the lock, then retain this buffer for the next
    // exchange so normal completion never allocates inside a native UI hook.
    for (const Completion& completion : g_dispatch_buffer) {
        if (!completion.callback) continue;
        invoke_callback_safely(&completion);
    }
    g_dispatch_buffer.clear();
}

} // namespace erui::native
