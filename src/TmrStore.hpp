#pragma once
// =============================================================================
// TmrStore.hpp — Triple Modular Redundancy (TMR) wrapper  v3.0
// =============================================================================
// DO-178C-informed coding style:
// - std::array replaces C-style arrays
// - [[nodiscard]] and auto -> return types
// - Zero-initialized members
// =============================================================================
#include <array>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string_view>
#include <type_traits>

namespace deltav {

template <typename T>
class TmrStore;

constexpr size_t MAX_TMR_ENTRIES = 64;

struct TmrCriticalParamEntry {
    const char* name{nullptr};
    void* store_ptr{nullptr};
    void        (*scrub_func)(void*){nullptr};
};

class TmrRegistry {
public:
    static auto getInstance() -> TmrRegistry& {
        static TmrRegistry instance;
        return instance;
    }

    auto registerParam(const char* name, void* store_ptr, void (*scrub_func)(void*)) -> void {
        std::lock_guard<std::mutex> lock(mtx);
        if (name == nullptr || store_ptr == nullptr || scrub_func == nullptr) {
            return;
        }

        for (size_t i = 0; i < count; ++i) {
            if (entries[i].store_ptr == store_ptr) {
                return;
            }
        }

        if (count < MAX_TMR_ENTRIES) {
            entries[count] = {name, store_ptr, scrub_func};
            ++count;
        }
    }

    auto unregisterParam(void* store_ptr) noexcept -> void {
        std::lock_guard<std::mutex> lock(mtx);
        if (store_ptr == nullptr || count == 0) {
            return;
        }

        for (size_t i = 0; i < count; ++i) {
            if (entries[i].store_ptr == store_ptr) {
                const size_t last = count - 1;
                entries[i] = entries[last];
                entries[last] = {};
                --count;
                return;
            }
        }
    }

    template <typename T>
    auto registerStore(TmrStore<T>* store) -> void {
        if (store != nullptr) {
            store->registerWithFdir("tmr_store");
        }
    }

    auto scrubAll() -> void {
        std::lock_guard<std::mutex> lock(mtx);
        for (size_t i = 0; i < count; ++i) {
            auto& entry = entries[i];
            if (entry.store_ptr != nullptr && entry.scrub_func != nullptr) {
                entry.scrub_func(entry.store_ptr);
            }
        }
    }

    [[nodiscard]] auto registeredCount() const -> size_t { return count; }

private:
    TmrRegistry() = default;

    std::array<TmrCriticalParamEntry, MAX_TMR_ENTRIES> entries{};
    size_t     count{0};
    std::mutex mtx{};
};

template <typename T>
class TmrStore {
public:
    static_assert(std::is_trivially_copyable_v<T>,
        "TmrStore requires trivially copyable data");

    explicit TmrStore(T initial = T{}) {
        copies.fill(initial);
    }

    ~TmrStore() noexcept {
        if (registered_with_registry) {
            TmrRegistry::getInstance().unregisterParam(this);
        }
    }

    TmrStore(const TmrStore&) = delete;
    auto operator=(const TmrStore&) -> TmrStore& = delete;
    TmrStore(TmrStore&&) = delete;
    auto operator=(TmrStore&&) -> TmrStore& = delete;

    auto registerWithFdir(const char* param_name) -> void {
        TmrRegistry::getInstance().registerParam(param_name, this, scrubCallback);
        registered_with_registry = true;
    }

    auto write(const T& val) -> void {
        std::lock_guard<std::mutex> lk(lock);
        copies.fill(val);
    }

    [[nodiscard]] auto read() -> T {
        std::lock_guard<std::mutex> lk(lock);
        T voted = vote();
        copies.fill(voted);
        return voted;
    }

    [[nodiscard]] auto isSane() const -> bool {
        std::lock_guard<std::mutex> lk(lock);
        return isSaneUnlocked();
    }

    auto scrub() -> void {
        std::lock_guard<std::mutex> lk(lock);
        if (!isSaneUnlocked()) {
            T valid = vote();
            copies.fill(valid);
        }
    }

    auto injectFaultForTesting(const T& bad_value) -> void {
        std::lock_guard<std::mutex> lk(lock);
        copies.at(0) = bad_value;
    }

    auto injectDoubleUpset(const T& value1, const T& value2) -> void {
        std::lock_guard<std::mutex> lk(lock);
        copies.at(1) = value1;
        copies.at(2) = value2;
    }

private:
    std::array<T, 3> copies{};
    mutable std::mutex lock{};
    bool registered_with_registry{false};

    /* NOLINTNEXTLINE(bugprone-easily-swappable-parameters) */
    static auto agree(const T& a, const T& b) -> bool {
        return std::memcmp(&a, &b, sizeof(T)) == 0;
    }

    [[nodiscard]] auto isSaneUnlocked() const -> bool {
        return agree(copies.at(0), copies.at(1)) && agree(copies.at(1), copies.at(2));
    }

    auto vote() -> T {
        if (agree(copies.at(0), copies.at(1))) return copies.at(0);
        if (agree(copies.at(1), copies.at(2))) return copies.at(1);
        if (agree(copies.at(0), copies.at(2))) return copies.at(0);
        // Triple failure: fallback to primary copy
        return copies.at(0);
    }

    static auto scrubCallback(void* instance) -> void {
        auto* tmr = static_cast<TmrStore<T>*>(instance);
        tmr->scrub();
    }
};

} // namespace deltav
