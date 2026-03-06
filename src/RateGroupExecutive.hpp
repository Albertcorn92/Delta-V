#pragma once
// =============================================================================
// RateGroupExecutive.hpp — DELTA-V Multi-Tier Rate Group Scheduler  v4.0
// =============================================================================
// Replaces the single-rate Scheduler with three deterministic execution tiers,
// following common flight-software rate-group scheduling patterns.
// =============================================================================
#include "Component.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>

namespace deltav {

constexpr size_t RGE_MAX_PER_TIER = 32;
constexpr size_t RGE_MAX_ACTIVE   = 16;

struct RateTier {
    const char* name{"?"};
    uint32_t    hz{1};
    std::array<Component*, RGE_MAX_PER_TIER> comps{};
    size_t      count{0};
    uint64_t    frame_drops{0};

    auto add(Component* c) -> bool {
        if (c == nullptr) {
            (void)std::fprintf(stderr, "[RGE] WARN: %s tier rejected null component\n", name);
            return false;
        }
        if (count >= RGE_MAX_PER_TIER) {
            const auto comp_name = c->getName();
            (void)std::fprintf(stderr, "[RGE] WARN: %s tier full - cannot register %.*s\n",
                name, static_cast<int>(comp_name.size()), comp_name.data());
            return false;
        }
        comps.at(count++) = c;
        return true;
    }

    auto tick() noexcept -> void {
        for (size_t i = 0; i < count; ++i) {
            if (!comps.at(i)->isActive()) comps.at(i)->step();
        }
    }
};

class RateGroupExecutive {
public:
    RateGroupExecutive() {
        fast_.name = "FAST"; fast_.hz = 10;
        norm_.name = "NORM"; norm_.hz =  1;
        slow_.name = "SLOW"; slow_.hz =  0; // driven from NORM loop every 10 ticks
    }

    ~RateGroupExecutive() { stopAll(); }

    // DO-178C: Rule of 5 — Not copyable or movable (owns std::threads)
    RateGroupExecutive(const RateGroupExecutive&)            = delete;
    auto operator=(const RateGroupExecutive&) -> RateGroupExecutive& = delete;
    RateGroupExecutive(RateGroupExecutive&&)                 = delete;
    auto operator=(RateGroupExecutive&&) -> RateGroupExecutive&      = delete;

    auto registerFast(Component* c) -> void {
        if (rejectRegistration(c)) {
            return;
        }
        if (fast_.add(c)) {
            printReg("FAST  10Hz", c);
        }
    }

    auto registerNorm(Component* c) -> void {
        if (rejectRegistration(c)) {
            return;
        }
        if (norm_.add(c)) {
            printReg("NORM   1Hz", c);
        }
    }

    auto registerSlow(Component* c) -> void {
        if (rejectRegistration(c)) {
            return;
        }
        if (slow_.add(c)) {
            printReg("SLOW 0.1Hz", c);
        }
    }

    auto registerComponent(Component* c) -> void {
        if (rejectRegistration(c)) {
            return;
        }

        if (c->isActive()) {
            if (active_count_ < RGE_MAX_ACTIVE) {
                active_.at(active_count_++) = c;
                printReg("ACTIVE", c);
            } else {
                (void)std::fprintf(stderr, "[RGE] WARN: active slot limit reached\n");
            }
        } else {
            registerNorm(c);
        }
    }

    auto initAll() -> void {
        std::printf("[RGE] Initializing %zu components...\n", totalCount());
        initTier(fast_);
        initTier(norm_);
        initTier(slow_);
        for (size_t i = 0; i < active_count_; ++i) active_.at(i)->init();
        std::printf("[RGE] Tiers ready - FAST=%zu NORM=%zu SLOW=%zu ACTIVE=%zu\n",
            fast_.count, norm_.count, slow_.count, active_count_);
    }

    auto startAll() -> void {
        running_.store(true, std::memory_order_release);

#if defined(ESP_PLATFORM) || defined(ARDUINO_ARCH_ESP32)
        // ESP path: cooperative scheduler avoids heavy host-style threading stack use.
        constexpr uint32_t FAST_PERIOD_MS = 100; // 10 Hz
        uint32_t norm_divider = 0;
        uint32_t slow_divider = 0;

        std::printf("[RGE] Embedded cooperative scheduler running.\n\n");

        while (running_.load(std::memory_order_acquire)) {
            fast_.tick();

            ++norm_divider;
            if (norm_divider >= 10) {
                norm_divider = 0;
                norm_.tick();

                // Active components execute in the cooperative loop on ESP targets.
                for (size_t i = 0; i < active_count_; ++i) {
                    active_.at(i)->step();
                }

                ++slow_divider;
                if (slow_divider >= 10) {
                    slow_divider = 0;
                    slow_.tick();
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(FAST_PERIOD_MS));
        }
        return;
#else
        for (size_t i = 0; i < active_count_; ++i) active_.at(i)->startThread();

        fast_thread_ = std::thread([this] { runTier(fast_); });

        norm_thread_ = std::thread([this] {
            constexpr uint64_t PERIOD_NS = 1'000'000'000ULL;
            auto wake = std::chrono::steady_clock::now();
            uint32_t cycle = 0;

            while (running_.load(std::memory_order_acquire)) {
                norm_.tick();
                if (++cycle % 10 == 0) slow_.tick();

                wake += std::chrono::nanoseconds(PERIOD_NS);
                auto now = std::chrono::steady_clock::now();
                if (now > wake) {
                    ++norm_.frame_drops;
                    if (norm_.frame_drops == 1 || norm_.frame_drops % 100 == 0)
                        (void)std::fprintf(stderr, "[RGE] NORM frame drop #%llu\n",
                            static_cast<unsigned long long>(norm_.frame_drops));
                    wake = now; // reset — don't try to catch up after a sustained overrun
                } else {
                    std::this_thread::sleep_until(wake);
                }
            }
        });

        std::printf("[RGE] All rate groups running. Ctrl+C to stop.\n\n");

        if (fast_thread_.joinable()) fast_thread_.join();
        if (norm_thread_.joinable()) norm_thread_.join();
#endif
    }

    auto stopAll() -> void {
        running_.store(false, std::memory_order_release);
        if (fast_thread_.joinable()) fast_thread_.join();
        if (norm_thread_.joinable()) norm_thread_.join();
        for (size_t i = 0; i < active_count_; ++i) active_.at(i)->joinThread();
    }

    [[nodiscard]] auto getFastDrops()  const noexcept -> uint64_t { return fast_.frame_drops; }
    [[nodiscard]] auto getNormDrops()  const noexcept -> uint64_t { return norm_.frame_drops; }
    [[nodiscard]] auto fastCount()     const noexcept -> size_t { return fast_.count; }
    [[nodiscard]] auto normCount()     const noexcept -> size_t { return norm_.count; }
    [[nodiscard]] auto slowCount()     const noexcept -> size_t { return slow_.count; }
    [[nodiscard]] auto activeCount()   const noexcept -> size_t { return active_count_; }
    [[nodiscard]] auto totalCount()    const noexcept -> size_t {
        return fast_.count + norm_.count + slow_.count + active_count_;
    }

private:
    RateTier fast_{};
    RateTier norm_{};
    RateTier slow_{};

    std::array<Component*, RGE_MAX_ACTIVE> active_{};
    size_t            active_count_{0};
    std::atomic<bool> running_{false};
    std::thread       fast_thread_{};
    std::thread       norm_thread_{};

    [[nodiscard]] auto isRegistered(const Component* c) const -> bool {
        if (c == nullptr) {
            return false;
        }

        for (size_t i = 0; i < fast_.count; ++i) {
            if (fast_.comps.at(i) == c) {
                return true;
            }
        }
        for (size_t i = 0; i < norm_.count; ++i) {
            if (norm_.comps.at(i) == c) {
                return true;
            }
        }
        for (size_t i = 0; i < slow_.count; ++i) {
            if (slow_.comps.at(i) == c) {
                return true;
            }
        }
        for (size_t i = 0; i < active_count_; ++i) {
            if (active_.at(i) == c) {
                return true;
            }
        }
        return false;
    }

    auto rejectRegistration(const Component* c) -> bool {
        if (c == nullptr) {
            (void)std::fprintf(stderr, "[RGE] WARN: null component registration rejected\n");
            return true;
        }
        if (isRegistered(c)) {
            const auto comp_name = c->getName();
            (void)std::fprintf(stderr, "[RGE] WARN: duplicate registration rejected for %.*s\n",
                static_cast<int>(comp_name.size()), comp_name.data());
            return true;
        }
        return false;
    }

    auto runTier(RateTier& t) -> void {
        const uint64_t period_ns = 1'000'000'000ULL / t.hz;
        auto wake = std::chrono::steady_clock::now();

        while (running_.load(std::memory_order_acquire)) {
            t.tick();
            wake += std::chrono::nanoseconds(period_ns);
            auto now = std::chrono::steady_clock::now();
            if (now > wake) {
                ++t.frame_drops;
                if (t.frame_drops == 1 || t.frame_drops % 100 == 0)
                    (void)std::fprintf(stderr, "[RGE] %s frame drop #%llu\n", t.name,
                        static_cast<unsigned long long>(t.frame_drops));
                wake = now; // prevent catch-up spiral
            } else {
                std::this_thread::sleep_until(wake);
            }
        }
    }

    static auto initTier(RateTier& t) -> void {
        for (size_t i = 0; i < t.count; ++i) t.comps.at(i)->init();
    }

    static auto printReg(const char* tier, const Component* c) -> void {
        const auto comp_name = c->getName();
        std::printf("[RGE]  +%.*s -> %s\n",
            static_cast<int>(comp_name.size()), comp_name.data(), tier);
    }
};

} // namespace deltav
