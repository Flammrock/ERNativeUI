#pragma once

#include "pagination.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace erui::detail {

enum class SubmenuResolutionKind : std::uint8_t {
    none,
    pending_request,
    bound_page,
    expired_request,
};

struct SubmenuResolution {
    SubmenuResolutionKind kind{SubmenuResolutionKind::none};
    PageRoute route{};
};

// Tracks the relationship between physical pagination routes and the native
// page objects created by Elden Ring's openSubPage path. Each physical slice
// gets its own native page and therefore participates in Elden Ring's normal
// Back stack. Storage is reserved before hooks are enabled.
class SubmenuNavigation {
public:
    [[nodiscard]] bool initialize(
        std::size_t logical_page_count,
        std::size_t binding_capacity) noexcept;
    void reset() noexcept;

    [[nodiscard]] bool begin_request(
        PageRoute target,
        std::uint64_t tick,
        void* parent_native_page = nullptr) noexcept;
    void cancel_request() noexcept;
    void unbind_native_page(void* native_page) noexcept;

    [[nodiscard]] SubmenuResolution resolve(
        void* native_page,
        std::uint64_t tick,
        std::uint64_t request_timeout_ms) noexcept;

    [[nodiscard]] bool should_inject(
        const PageRoute& route,
        void* native_page,
        std::uint64_t tick,
        std::uint64_t cooldown_ms) noexcept;

    [[nodiscard]] std::size_t page_count() const noexcept {
        return logical_page_count_;
    }

    [[nodiscard]] void* native_page(const PageRoute& route) const noexcept;
    [[nodiscard]] void* parent_page(void* native_page) const noexcept;
    [[nodiscard]] PageRoute pending_route() const noexcept {
        return pending_route_;
    }
    [[nodiscard]] PageRoute route_for_native_page(
        void* native_page) const noexcept;

private:
    struct Binding {
        PageRoute route{
            .logical_page_index = invalid_route_index,
        };
        void* native_page{};
        void* parent_native_page{};
        std::uint64_t last_injection_tick{};

        [[nodiscard]] bool active() const noexcept {
            return route.valid() && native_page != nullptr;
        }
    };

    [[nodiscard]] bool bind(
        PageRoute route,
        void* native_page,
        void* parent_native_page) noexcept;
    void clear_native_alias(void* native_page) noexcept;

    std::vector<Binding> bindings_{};
    std::size_t logical_page_count_{};

    PageRoute pending_route_{
        .logical_page_index = invalid_route_index,
    };
    std::uint64_t pending_tick_{};
    void* pending_parent_page_{};
};

} // namespace erui::detail
