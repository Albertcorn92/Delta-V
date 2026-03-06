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
#include <iostream>
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
            std::cerr << "[RGE] WARN: " << name << " tier rejected null component\n";
            return false;
        }
        if (count >= RGE_MAX_PER_TIER) {
            std::cerr << "[RGE] WARN: " << name
                      << " tier full — cannot register " << c->getName() << "\n";
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
                std::cerr << "[RGE] WARN: active slot limit reached\n";
            }
        } else {
            registerNorm(c);
        }
    }

    auto initAll() -> void {
        std::cout << "[RGE] Initializing " << totalCount() << " components...\n";
        initTier(fast_);
        initTier(norm_);
        initTier(slow_);
        for (size_t i = 0; i < active_count_; ++i) active_.at(i)->init();
        std::cout << "[RGE] Tiers ready — FAST=" << fast_.count
                  << " NORM=" << norm_.count
                  << " SLOW=" << slow_.count
                  << " ACTIVE=" << active_count_ << "\n";
    }

    auto startAll() -> void {
        running_.store(true, std::memory_order_release);

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
                        std::cerr << "[RGE] NORM frame drop #" << norm_.frame_drops << "\n";
                    wake = now; // reset — don't try to catch up after a sustained overrun
                } else {
                    std::this_thread::sleep_until(wake);
                }
            }
        });

        std::cout << "[RGE] All rate groups running. Ctrl+C to stop.\n\n";

        if (fast_thread_.joinable()) fast_thread_.join();
        if (norm_thread_.joinable()) norm_thread_.join();
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
            std::cerr << "[RGE] WARN: null component registration rejected\n";
            return true;
        }
        if (isRegistered(c)) {
            std::cerr << "[RGE] WARN: duplicate registration rejected for "
                      << c->getName() << "\n";
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
                    std::cerr << "[RGE] " << t.name
                              << " frame drop #" << t.frame_drops << "\n";
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
        std::cout << "[RGE]  +" << c->getName() << " → " << tier << "\n";
    }
};

} // namespace deltav
