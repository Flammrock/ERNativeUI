#include "submenu_navigation.hpp"

#include <algorithm>
#include <new>

namespace erui::detail {
namespace {

PageRoute invalid_route() noexcept {
    return PageRoute{
        .logical_page_index = invalid_route_index,
    };
}

bool route_is_bindable(
    const PageRoute& route,
    std::size_t logical_page_count) noexcept {
    return route.valid() &&
        route.kind != PageRouteKind::root_main &&
        route.logical_page_index < logical_page_count;
}

} // namespace

bool SubmenuNavigation::initialize(
    std::size_t logical_page_count,
    std::size_t binding_capacity) noexcept {
    reset();
    if (logical_page_count == 0 || binding_capacity == 0) {
        return false;
    }

    try {
        // Pre-size rather than merely reserve so bind() performs no allocation
        // from an Elden Ring UI callback.
        bindings_.resize(binding_capacity);
    } catch (const std::bad_alloc&) {
        reset();
        return false;
    } catch (...) {
        reset();
        return false;
    }

    logical_page_count_ = logical_page_count;
    return true;
}

void SubmenuNavigation::reset() noexcept {
    bindings_.clear();
    logical_page_count_ = 0;
    pending_route_ = invalid_route();
    pending_tick_ = 0;
    pending_parent_page_ = nullptr;
}

bool SubmenuNavigation::begin_request(
    PageRoute target,
    std::uint64_t tick,
    void* parent_native_page) noexcept {
    if (!route_is_bindable(target, logical_page_count_)) {
        return false;
    }
    pending_route_ = target;
    pending_tick_ = tick;
    pending_parent_page_ = parent_native_page;
    return true;
}

void SubmenuNavigation::cancel_request() noexcept {
    pending_route_ = invalid_route();
    pending_tick_ = 0;
    pending_parent_page_ = nullptr;
}

void SubmenuNavigation::unbind_native_page(void* native_page) noexcept {
    clear_native_alias(native_page);
}

void SubmenuNavigation::clear_native_alias(void* native_page) noexcept {
    if (!native_page) {
        return;
    }
    for (Binding& binding : bindings_) {
        if (binding.native_page == native_page) {
            binding = {};
        }
    }
}

bool SubmenuNavigation::bind(
    PageRoute route,
    void* native_page,
    void* parent_native_page) noexcept {
    if (!native_page || !route_is_bindable(route, logical_page_count_)) {
        return false;
    }

    clear_native_alias(native_page);

    // A reopened route replaces its stale native pointer. This prevents the
    // fixed binding table from growing indefinitely across menu reopenings.
    for (Binding& binding : bindings_) {
        if (binding.active() && binding.route == route) {
            binding.native_page = native_page;
            binding.parent_native_page = parent_native_page;
            binding.last_injection_tick = 0;
            return true;
        }
    }

    for (Binding& binding : bindings_) {
        if (!binding.active()) {
            binding.route = route;
            binding.native_page = native_page;
            binding.parent_native_page = parent_native_page;
            binding.last_injection_tick = 0;
            return true;
        }
    }
    return false;
}

PageRoute SubmenuNavigation::route_for_native_page(
    void* native_page) const noexcept {
    if (!native_page) {
        return invalid_route();
    }
    for (const Binding& binding : bindings_) {
        if (binding.active() && binding.native_page == native_page) {
            return binding.route;
        }
    }
    return invalid_route();
}

SubmenuResolution SubmenuNavigation::resolve(
    void* native_page,
    std::uint64_t tick,
    std::uint64_t request_timeout_ms) noexcept {
    if (!native_page) {
        return {};
    }

    if (pending_route_.valid()) {
        const bool monotonic = tick >= pending_tick_;
        const bool current = monotonic &&
            tick - pending_tick_ <= request_timeout_ms;
        if (current) {
            const PageRoute route = pending_route_;
            void* const parent = pending_parent_page_;
            const bool bound = bind(route, native_page, parent);
            cancel_request();
            if (!bound) {
                return {};
            }
            return {
                .kind = SubmenuResolutionKind::pending_request,
                .route = route,
            };
        }

        cancel_request();
        const PageRoute bound_after_expiry = route_for_native_page(native_page);
        if (bound_after_expiry.valid()) {
            return {
                .kind = SubmenuResolutionKind::bound_page,
                .route = bound_after_expiry,
            };
        }
        return {
            .kind = SubmenuResolutionKind::expired_request,
            .route = invalid_route(),
        };
    }

    const PageRoute bound = route_for_native_page(native_page);
    if (!bound.valid()) {
        return {};
    }
    return {
        .kind = SubmenuResolutionKind::bound_page,
        .route = bound,
    };
}

bool SubmenuNavigation::should_inject(
    const PageRoute& route,
    void* native_page,
    std::uint64_t tick,
    std::uint64_t cooldown_ms) noexcept {
    if (!route.valid() || !native_page) {
        return false;
    }

    for (Binding& binding : bindings_) {
        if (!binding.active() || binding.native_page != native_page ||
            binding.route != route) {
            continue;
        }

        if (binding.last_injection_tick != 0 &&
            tick >= binding.last_injection_tick &&
            tick - binding.last_injection_tick < cooldown_ms) {
            return false;
        }
        binding.last_injection_tick = tick;
        return true;
    }
    return false;
}

void* SubmenuNavigation::native_page(const PageRoute& route) const noexcept {
    if (!route.valid()) {
        return nullptr;
    }
    for (const Binding& binding : bindings_) {
        if (binding.active() && binding.route == route) {
            return binding.native_page;
        }
    }
    return nullptr;
}

void* SubmenuNavigation::parent_page(void* native_page) const noexcept {
    if (!native_page) return nullptr;
    for (const Binding& binding : bindings_) {
        if (binding.active() && binding.native_page == native_page) {
            return binding.parent_native_page;
        }
    }
    return nullptr;
}

} // namespace erui::detail
