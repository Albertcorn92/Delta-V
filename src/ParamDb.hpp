#pragma once
#include <atomic>
#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>

namespace deltav {

// Define a safe, fixed maximum number of parameters for the mission
constexpr size_t MAX_PARAMS = 256;
constexpr const char* PARAM_FILE = "mission_params.db"; // SITL Persistence

class ParamDb {
public:
    static ParamDb& getInstance() {
        static ParamDb instance;
        return instance;
    }

    // Lock-free read operation using atomic memory acquire
    float getParam(uint32_t param_id, float default_val) {
        size_t current_count = count.load(std::memory_order_acquire);
        
        // Linear search through contiguous memory is highly cache-efficient
        for (size_t i = 0; i < current_count; ++i) {
            if (params[i].id == param_id) {
                return params[i].value.load(std::memory_order_acquire);
            }
        }
        
        // If not found, safely add it without locks
        size_t idx = count.fetch_add(1, std::memory_order_acq_rel);
        if (idx < MAX_PARAMS) {
            params[idx].id = param_id;
            params[idx].value.store(default_val, std::memory_order_release);
            return default_val;
        }
        return default_val; // Failsafe if we exceed MAX_PARAMS
    }

    // Lock-free write operation
    void setParam(uint32_t param_id, float value) {
        size_t current_count = count.load(std::memory_order_acquire);
        
        for (size_t i = 0; i < current_count; ++i) {
            if (params[i].id == param_id) {
                params[i].value.store(value, std::memory_order_release);
                return;
            }
        }
        
        // If updating a non-existent parameter, safely add it
        size_t idx = count.fetch_add(1, std::memory_order_acq_rel);
        if (idx < MAX_PARAMS) {
            params[idx].id = param_id;
            params[idx].value.store(value, std::memory_order_release);
        }
    }

    // SITL Phase 5: State Persistence Load
    void load() {
        std::ifstream file(PARAM_FILE);
        if (!file.is_open()) {
            std::cout << "[ParamDb] No existing parameter file found. Starting fresh.\n";
            return;
        }

        uint32_t id;
        float val;
        int loaded_count = 0;
        
        // Safely insert file contents into the lock-free array
        while (file >> id >> val) {
            setParam(id, val);
            loaded_count++;
        }
        std::cout << "[ParamDb] Successfully loaded " << loaded_count << " parameters from NVS.\n";
    }

    // SITL Phase 5: State Persistence Save
    void save() const {
        std::ofstream file(PARAM_FILE, std::ios::trunc);
        if (!file.is_open()) {
            std::cerr << "[ParamDb] ERROR: Failed to open parameter file for saving!\n";
            return;
        }

        size_t current_count = count.load(std::memory_order_acquire);
        for (size_t i = 0; i < current_count; ++i) {
            file << params[i].id << " " << params[i].value.load(std::memory_order_acquire) << "\n";
        }
        std::cout << "[ParamDb] Mission parameters safely written to NVS.\n";
    }

private:
    ParamDb() : count(0) {}
    
    struct ParamEntry {
        uint32_t id;
        std::atomic<float> value;
        
        // Atomics cannot be trivially copied, so we initialize them manually
        ParamEntry() : id(0), value(0.0f) {}
    };

    // Statically allocated memory block
    std::array<ParamEntry, MAX_PARAMS> params;
    std::atomic<size_t> count;
};

} // namespace deltav