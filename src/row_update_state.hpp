#pragma once

#include <mutex>

namespace erui::detail {

// Serializes publication of the presentation and visibility halves of one
// API update batch. The registry holds this lock from the first fallible
// prepare through the last no-fail publish, so a UI frame can never observe a
// label/help generation without its matching layout generation (or vice
// versa).
using RowUpdatePublicationLock = std::unique_lock<std::mutex>;

[[nodiscard]] RowUpdatePublicationLock
lock_row_update_publication() noexcept;

// Publishes both halves at a boundary already proven safe for a native row
// rebuild. Initial materialization is also such a boundary.
void synchronize_pending_row_updates() noexcept;

// Publishes ordinary presentation-only batches. If a visibility generation
// arrived concurrently, leave the complete batch queued for the native
// visibility lifecycle rather than applying either half early.
[[nodiscard]] bool
synchronize_pending_row_updates_if_visibility_idle() noexcept;

} // namespace erui::detail
