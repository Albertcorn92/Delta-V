#include "HeapGuard.hpp"

#if !defined(DELTAV_OS_FREERTOS)

#include <cstddef>
#include <cstdlib>
#include <new>

void* operator new(std::size_t size) {
    if (deltav::HeapGuard::isArmed()) {
        deltav::HeapGuard::violation("operator new");
    }
    void* p = std::malloc(size);
    if (p == nullptr) {
        throw std::bad_alloc{};
    }
    return p;
}

void* operator new[](std::size_t size) {
    if (deltav::HeapGuard::isArmed()) {
        deltav::HeapGuard::violation("operator new[]");
    }
    void* p = std::malloc(size);
    if (p == nullptr) {
        throw std::bad_alloc{};
    }
    return p;
}

void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

#endif // !DELTAV_OS_FREERTOS
