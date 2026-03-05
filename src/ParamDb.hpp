#pragma once
#include <map>
#include <mutex>
#include <cstdint>

namespace deltav {

class ParamDb {
public:
    // Get the global instance (Singleton pattern)
    static ParamDb& getInstance() {
        static ParamDb instance;
        return instance;
    }

    // Components call this to get their settings
    float getParam(uint32_t param_id, float default_val) {
        std::lock_guard<std::mutex> lock(mtx);
        if (params.find(param_id) == params.end()) {
            params[param_id] = default_val;
        }
        return params[param_id];
    }

    // The CommandHub will call this to update values mid-flight
    void setParam(uint32_t param_id, float value) {
        std::lock_guard<std::mutex> lock(mtx);
        params[param_id] = value;
    }

private:
    ParamDb() = default;
    std::map<uint32_t, float> params;
    std::mutex mtx; // Thread safety for when the GDS updates values
};

} // namespace deltav