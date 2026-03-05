#pragma once
#include <atomic>
#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <cstring>

namespace deltav {

// Define a safe, fixed maximum number of parameters for the mission
constexpr size_t MAX_PARAMS = 256;
constexpr const char* PARAM_FILE = "mission_params.db"; // SITL Persistence

class ParamDb {
public:
    // 🚀 UPDATED: Public constructor allows the Test Suite to create instances
    ParamDb() : count(0), expected_crc(0) {}

    static ParamDb& getInstance() {
        static ParamDb instance;
        return instance;
    }

    // 🚀 UPGRADE: Standard CRC-32 Algorithm for memory integrity
    static uint32_t computeCRC32(const uint8_t* data, size_t length) {
        uint32_t crc = 0xFFFFFFFF;
        for (size_t i = 0; i < length; i++) {
            crc ^= data[i];
            for (int j = 0; j < 8; j++) {
                crc = (crc >> 1) ^ (0xEDB88320 & (-(crc & 1)));
            }
        }
        return ~crc;
    }

    // Helper for Test Suite to inject specific values by name (mapped to ID)
    void setValue(const std::string& name, float value) {
        // Simple hash of string to ID for testing purposes
        uint32_t id = 0;
        for(char c : name) id += c;
        setParam(id, value);
    }

    // Helper for Test Suite to get a raw pointer to simulate corruption
    void* getPtr(const std::string& name) {
        uint32_t id = 0;
        for(char c : name) id += c;
        
        size_t current_count = count.load(std::memory_order_acquire);
        for (size_t i = 0; i < current_count; ++i) {
            if (params[i].id == id) {
                return &(params[i].value);
            }
        }
        return nullptr;
    }

    float getParam(uint32_t param_id, float default_val) {
        size_t current_count = count.load(std::memory_order_acquire);
        for (size_t i = 0; i < current_count; ++i) {
            if (params[i].id == param_id) {
                return params[i].value.load(std::memory_order_acquire);
            }
        }
        
        size_t idx = count.fetch_add(1, std::memory_order_acq_rel);
        if (idx < MAX_PARAMS) {
            params[idx].id = param_id;
            params[idx].value.store(default_val, std::memory_order_release);
            updateChecksum();
            return default_val;
        }
        return default_val; 
    }

    void setParam(uint32_t param_id, float value) {
        size_t current_count = count.load(std::memory_order_acquire);
        for (size_t i = 0; i < current_count; ++i) {
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

    bool verifyIntegrity() {
        uint32_t current_crc = calculateCurrentChecksum();
        return (current_crc == expected_crc.load(std::memory_order_acquire));
    }

    void load() {
        std::ifstream file(PARAM_FILE);
        if (!file.is_open()) return;

        uint32_t id;
        float val;
        while (file >> id >> val) {
            setParam(id, val);
        }
        updateChecksum();
    }

    void save() const {
        std::ofstream file(PARAM_FILE, std::ios::trunc);
        if (!file.is_open()) return;

        size_t current_count = count.load(std::memory_order_acquire);
        for (size_t i = 0; i < current_count; ++i) {
            file << params[i].id << " " << params[i].value.load(std::memory_order_acquire) << "\n";
        }
    }

private:
    struct ParamEntry {
        uint32_t id;
        std::atomic<float> value;
        ParamEntry() : id(0), value(0.0f) {}
    };

    std::array<ParamEntry, MAX_PARAMS> params;
    std::atomic<size_t> count;
    std::atomic<uint32_t> expected_crc;

    uint32_t calculateCurrentChecksum() {
        size_t current_count = count.load(std::memory_order_acquire);
        if (current_count == 0) return 0;

        // Create a buffer of the current values to hash
        float buffer[MAX_PARAMS];
        for(size_t i = 0; i < current_count; ++i) {
            buffer[i] = params[i].value.load(std::memory_order_acquire);
        }
        
        return computeCRC32(reinterpret_cast<const uint8_t*>(buffer), current_count * sizeof(float));
    }

    void updateChecksum() {
        expected_crc.store(calculateCurrentChecksum(), std::memory_order_release);
    }
};

} // namespace deltav