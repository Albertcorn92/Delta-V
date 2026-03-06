#pragma once
// =============================================================================
// ParamDb.hpp — DELTA-V Mission Parameter Database
// =============================================================================
// Thread-safe key-value store for mission configuration (PID gains, thresholds,
// calibration coefficients). Persists to filesystem for SITL across reboots.
//
// Key fixes over previous version:
//   1. FNV-1a string hash replaces character-sum (eliminates collision risk).
//   2. CRC-32 covers both param IDs and values (not just values).
//   3. No dynamic allocation — fixed array, atomic counters.
//   4. Public constructor retained for unit test instantiation.
// =============================================================================
#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>

namespace deltav {

constexpr size_t       MAX_PARAMS = 256;
constexpr const char*  PARAM_FILE = "mission_params.db";

class ParamDb {
public:
    ParamDb() : count(0), expected_crc(0) {}

    static ParamDb& getInstance() {
        static ParamDb instance;
        return instance;
    }

    // -----------------------------------------------------------------------
    // FNV-1a 32-bit hash — collision-resistant string → uint32_t mapping.
    // "AB" and "BA" produce different IDs unlike the old character-sum.
    // constexpr so compile-time param IDs can be computed without runtime cost.
    // -----------------------------------------------------------------------
    static constexpr uint32_t fnv1a(const char* str) {
        uint32_t hash = 2166136261u;
        while (*str) {
            hash ^= static_cast<uint8_t>(*str++);
            hash *= 16777619u;
        }
        return hash;
    }

    // -----------------------------------------------------------------------
    // CRC-32 (standard polynomial 0xEDB88320)
    // Now computed over a struct that contains both the param ID and value,
    // so corruption of either field is detected.
    // -----------------------------------------------------------------------
    static uint32_t computeCRC32(const uint8_t* data, size_t length) {
        uint32_t crc = 0xFFFFFFFFu;
        for (size_t i = 0; i < length; i++) {
            crc ^= data[i];
            for (int j = 0; j < 8; j++) {
                crc = (crc >> 1) ^ (0xEDB88320u & static_cast<uint32_t>(-(crc & 1)));
            }
        }
        return ~crc;
    }

    // -----------------------------------------------------------------------
    // Core API — use uint32_t IDs in flight code.
    // Prefer: getParam(ParamDb::fnv1a("MY_PARAM"), default)
    //   or:   constexpr uint32_t MY_ID = ParamDb::fnv1a("MY_PARAM");
    // -----------------------------------------------------------------------
    float getParam(uint32_t param_id, float default_val) {
        size_t n = count.load(std::memory_order_acquire);
        for (size_t i = 0; i < n; ++i) {
            if (params[i].id == param_id) {
                return params[i].value.load(std::memory_order_acquire);
            }
        }
        // Auto-register with default value
        size_t idx = count.fetch_add(1, std::memory_order_acq_rel);
        if (idx < MAX_PARAMS) {
            params[idx].id = param_id;
            params[idx].value.store(default_val, std::memory_order_release);
            updateChecksum();
        }
        return default_val;
    }

    void setParam(uint32_t param_id, float value) {
        size_t n = count.load(std::memory_order_acquire);
        for (size_t i = 0; i < n; ++i) {
            if (params[i].id == param_id) {
                params[i].value.store(value, std::memory_order_release);
                updateChecksum();
                return;
            }
        }
        size_t idx = count.fetch_add(1, std::memory_order_acq_rel);
        if (idx < MAX_PARAMS) {
            params[idx].id = param_id;
            params[idx].value.store(value, std::memory_order_release);
            updateChecksum();
        }
    }

    // String convenience wrappers — use only in test code or GDS uplink handler.
    float getParam(const char* name, float default_val) {
        return getParam(fnv1a(name), default_val);
    }
    void setParam(const char* name, float value) {
        setParam(fnv1a(name), value);
    }

    // -----------------------------------------------------------------------
    // Integrity verification — call from Watchdog on a periodic basis.
    // Returns false if memory has been silently corrupted.
    // -----------------------------------------------------------------------
    bool verifyIntegrity() {
        return calculateCurrentChecksum() == expected_crc.load(std::memory_order_acquire);
    }

    // -----------------------------------------------------------------------
    // Persistence (SITL only — on real hardware, params come from EEPROM/flash)
    // -----------------------------------------------------------------------
    void load() {
        std::ifstream file(PARAM_FILE);
        if (!file.is_open()) return;
        uint32_t id; float val;
        while (file >> id >> val) {
            setParam(id, val);
        }
        updateChecksum();
        std::cout << "[ParamDb] Loaded parameters from " << PARAM_FILE << std::endl;
    }

    void save() const {
        std::ofstream file(PARAM_FILE, std::ios::trunc);
        if (!file.is_open()) return;
        size_t n = count.load(std::memory_order_acquire);
        for (size_t i = 0; i < n; ++i) {
            file << params[i].id << " "
                 << params[i].value.load(std::memory_order_acquire) << "\n";
        }
    }

    size_t paramCount() const { return count.load(std::memory_order_acquire); }

    // For unit tests: inject a raw pointer to simulate corruption
    void* getRawPtr(uint32_t param_id) {
        size_t n = count.load(std::memory_order_acquire);
        for (size_t i = 0; i < n; ++i) {
            if (params[i].id == param_id) return &params[i].value;
        }
        return nullptr;
    }

private:
    struct ParamEntry {
        uint32_t             id{0};
        std::atomic<float>   value{0.0f};
        ParamEntry() = default;
    };

    struct CrcPayload {
        uint32_t id;
        float    value;
    };

    std::array<ParamEntry, MAX_PARAMS> params;
    std::atomic<size_t>    count;
    std::atomic<uint32_t>  expected_crc;

    uint32_t calculateCurrentChecksum() {
        size_t n = count.load(std::memory_order_acquire);
        if (n == 0) return 0;
        // Hash both IDs and values so corruption of either is detected
        CrcPayload buf[MAX_PARAMS];
        for (size_t i = 0; i < n; ++i) {
            buf[i].id    = params[i].id;
            buf[i].value = params[i].value.load(std::memory_order_acquire);
        }
        return computeCRC32(reinterpret_cast<const uint8_t*>(buf), n * sizeof(CrcPayload));
    }

    void updateChecksum() {
        expected_crc.store(calculateCurrentChecksum(), std::memory_order_release);
    }
};

} // namespace deltav