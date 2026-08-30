#pragma once

#include <atomic>

namespace erui::native {

enum class DialogModalLoss {
    none,
    dismissed,
    ownership_lost,
};

struct DialogModalReconcileResult {
    void* job{};
    DialogModalLoss loss{DialogModalLoss::none};
};

// Tracks the exact CSPopupMenu blocking task owned by ERNativeUI. The owner
// is published before the job; acquiring a non-null job therefore also makes
// its owner visible. Reconciliation clears a task exactly once.
class DialogModalState {
public:
    void publish(void* owner, void* job) noexcept {
        owner_.store(owner, std::memory_order_relaxed);
        job_.store(job, std::memory_order_release);
    }

    [[nodiscard]] bool active() const noexcept {
        return job_.load(std::memory_order_acquire) != nullptr;
    }

    [[nodiscard]] bool owns(void* owner, void* slot_job) const noexcept {
        void* const submitted = job_.load(std::memory_order_acquire);
        return submitted && owner == owner_.load(std::memory_order_relaxed) &&
            slot_job == submitted;
    }

    // Returns the task and reason when ownership ends. A compare/exchange
    // makes repeated observations and nested hooks idempotent.
    [[nodiscard]] DialogModalReconcileResult reconcile(
        void* owner,
        void* slot_job) noexcept {
        void* submitted = job_.load(std::memory_order_acquire);
        if (!submitted) return {};
        const bool same_owner =
            owner == owner_.load(std::memory_order_relaxed);
        if (same_owner && slot_job == submitted) return {};
        const DialogModalLoss loss = same_owner && !slot_job
            ? DialogModalLoss::dismissed
            : DialogModalLoss::ownership_lost;
        if (!job_.compare_exchange_strong(
                submitted, nullptr, std::memory_order_acq_rel,
                std::memory_order_acquire)) {
            return {};
        }
        owner_.store(nullptr, std::memory_order_release);
        return {submitted, loss};
    }

    void reset() noexcept {
        job_.store(nullptr, std::memory_order_release);
        owner_.store(nullptr, std::memory_order_release);
    }

private:
    std::atomic<void*> owner_{};
    std::atomic<void*> job_{};
};

} // namespace erui::native
