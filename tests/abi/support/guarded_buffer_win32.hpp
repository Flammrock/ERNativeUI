#pragma once

#if !defined(_WIN32)
#  error GuardedBuffer requires Windows virtual-memory APIs.
#endif

#include <Windows.h>

#include <cstddef>
#include <new>
#include <type_traits>

namespace erui::abi_test {

/*
 * Places one trivially destructible ABI object at the final bytes of a
 * readable page. Any host read beyond sizeof(T) reaches PAGE_NOACCESS and
 * fails the individual test process deterministically.
 */
template <typename T>
class GuardedObject {
public:
    GuardedObject() {
        static_assert(std::is_trivially_destructible<T>::value,
            "ABI guard objects must not require destructor execution");
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        page_size_ = info.dwPageSize;
        if (sizeof(T) > page_size_) throw std::bad_alloc{};
        pages_ = static_cast<std::byte*>(VirtualAlloc(
            nullptr,
            static_cast<std::size_t>(page_size_) * 2u,
            MEM_RESERVE | MEM_COMMIT,
            PAGE_READWRITE));
        if (!pages_) throw std::bad_alloc{};

        DWORD old_protection{};
        if (!VirtualProtect(
                pages_ + page_size_,
                page_size_,
                PAGE_NOACCESS,
                &old_protection)) {
            VirtualFree(pages_, 0, MEM_RELEASE);
            pages_ = nullptr;
            throw std::bad_alloc{};
        }
        object_ = ::new (pages_ + page_size_ - sizeof(T)) T{};
    }

    ~GuardedObject() {
        if (pages_) VirtualFree(pages_, 0, MEM_RELEASE);
    }

    GuardedObject(const GuardedObject&) = delete;
    GuardedObject& operator=(const GuardedObject&) = delete;

    [[nodiscard]] T* get() noexcept { return object_; }
    [[nodiscard]] const T* get() const noexcept { return object_; }
    [[nodiscard]] T& operator*() noexcept { return *object_; }
    [[nodiscard]] T* operator->() noexcept { return object_; }

private:
    std::byte* pages_{};
    DWORD page_size_{};
    T* object_{};
};

} // namespace erui::abi_test
