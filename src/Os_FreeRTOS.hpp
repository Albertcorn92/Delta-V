#pragma once
// =============================================================================
// Os_FreeRTOS.hpp — DELTA-V OS Abstraction: FreeRTOS Backend
// =============================================================================
// Drop-in replacement for Os.hpp on embedded targets running FreeRTOS.
//
// USAGE:
//   In your embedded CMakeLists.txt (cross-compile target only), add:
//     target_compile_definitions(flight_software PRIVATE DELTAV_OS_FREERTOS)
//   Then in Os.hpp, the conditional include selects this file automatically.
//
// PORTING CHECKLIST:
//   1. Add FreeRTOS include path to target_include_directories
//   2. Set configUSE_TASK_NOTIFICATIONS 1 in FreeRTOSConfig.h
//   3. Set configUSE_TICKLESS_IDLE 0 (deterministic tick required)
//   4. Set configTICK_RATE_HZ >= highest component rate * 10
//   5. Assign stack sizes per mission power budget (see STACK_SIZE below)
//
// Tested against FreeRTOS kernel v10.5.1 (LTS).
// Maps cleanly to Cortex-M4 / M7 (STM32H7, STM32F4, NXP RT1060).
//
// DO-178C Note: FreeRTOS itself is DO-178C certifiable via WHIS SafeRTOS
// derivative. This file uses only the core task and queue APIs that are
// present in both FreeRTOS and SafeRTOS.
// =============================================================================

// Guard — this file is only compiled when FreeRTOS is the target OS
#ifdef DELTAV_OS_FREERTOS

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include <cstdint>
#include <functional>

namespace deltav {
namespace Os {

// ---------------------------------------------------------------------------
// Stack size for flight tasks — tune per mission power/memory budget.
// 512 words (2KB on Cortex-M) is sufficient for typical sensor/hub components.
// TelemetryBridge needs more stack due to socket buffers — increase to 1024.
// ---------------------------------------------------------------------------
constexpr uint16_t DEFAULT_STACK_WORDS = 512u;
constexpr UBaseType_t DEFAULT_PRIORITY = tskIDLE_PRIORITY + 2u;

// ---------------------------------------------------------------------------
// Os::Mutex — wraps FreeRTOS binary semaphore.
// Binary semaphore chosen over mutex to avoid priority inversion in ISR
// contexts. For components that call from ISR, use xSemaphoreGiveFromISR.
// ---------------------------------------------------------------------------
class Mutex {
public:
    Mutex() {
        handle = xSemaphoreCreateBinary();
        if (handle != nullptr) {
            xSemaphoreGive(handle); // Start in unlocked state
        }
    }

    ~Mutex() {
        if (handle != nullptr) {
            vSemaphoreDelete(handle);
        }
    }

    void lock() {
        if (handle != nullptr) {
            xSemaphoreTake(handle, portMAX_DELAY);
        }
    }

    void unlock() {
        if (handle != nullptr) {
            xSemaphoreGive(handle);
        }
    }

    // RAII guard — identical API to POSIX backend
    class Guard {
    public:
        explicit Guard(Mutex& m) : mtx(m) { mtx.lock(); }
        ~Guard() { mtx.unlock(); }
        Guard(const Guard&) = delete;
        Guard& operator=(const Guard&) = delete;
    private:
        Mutex& mtx;
    };

private:
    SemaphoreHandle_t handle{nullptr};
};

// ---------------------------------------------------------------------------
// Os::Thread — wraps FreeRTOS task with absolute-deadline scheduling.
//
// FreeRTOS equivalent of sleep_until: compute next absolute tick count using
// xTaskGetTickCount() + period_ticks, then call vTaskDelayUntil().
// This provides the same drift-free behaviour as the POSIX sleep_until backend.
//
// NOTE: std::function requires heap. On heap-constrained targets, replace
// with a function pointer + void* user_data pattern. The interface is
// identical from the caller's perspective.
// ---------------------------------------------------------------------------
class Thread {
public:
    Thread() = default;

    ~Thread() { stop(); }

    Thread(const Thread&) = delete;
    Thread& operator=(const Thread&) = delete;

    void start(uint32_t hz, std::function<void()> fn, const char* name = "deltav_task",
               uint16_t stack_words = DEFAULT_STACK_WORDS,
               UBaseType_t priority = DEFAULT_PRIORITY) {
        if (running) return;
        period_ticks = static_cast<TickType_t>(configTICK_RATE_HZ / hz);
        if (period_ticks == 0) period_ticks = 1;
        running = true;
        callback = std::move(fn);

        // FreeRTOS task entry shim — captures 'this' via pvParameters
        xTaskCreate(
            [](void* param) {
                auto* self = static_cast<Thread*>(param);
                TickType_t last_wake = xTaskGetTickCount();
                while (self->running) {
                    self->callback();
                    // vTaskDelayUntil provides absolute-deadline scheduling:
                    // last_wake is incremented by period_ticks each call,
                    // so the absolute wakeup time never drifts.
                    vTaskDelayUntil(&last_wake, self->period_ticks);
                }
                self->task_handle = nullptr;
                vTaskDelete(nullptr);
            },
            name,
            stack_words,
            this,
            priority,
            &task_handle
        );
    }

    void stop() {
        running = false;
        // Give the task one period to notice the flag and self-delete.
        // On a real mission you would add a notification mechanism here.
        if (task_handle != nullptr) {
            vTaskDelay(period_ticks + 1u);
            // If task has not self-deleted, force delete (last resort)
            if (task_handle != nullptr) {
                vTaskDelete(task_handle);
                task_handle = nullptr;
            }
        }
    }

    bool isRunning() const { return running && (task_handle != nullptr); }

private:
    TaskHandle_t          task_handle{nullptr};
    std::function<void()> callback;
    TickType_t            period_ticks{1};
    volatile bool         running{false};
};

// ---------------------------------------------------------------------------
// Os::Queue — wraps FreeRTOS queue for inter-task communication.
// FreeRTOS queues are inherently thread-safe so no additional mutex is needed.
// ---------------------------------------------------------------------------
template<typename T, size_t Capacity>
class Queue {
public:
    Queue() {
        handle = xQueueCreate(static_cast<UBaseType_t>(Capacity), sizeof(T));
    }

    ~Queue() {
        if (handle != nullptr) {
            vQueueDelete(handle);
        }
    }

    bool push(const T& item) {
        if (handle == nullptr) return false;
        return xQueueSend(handle, &item, 0) == pdTRUE;
    }

    bool pop(T& out) {
        if (handle == nullptr) return false;
        return xQueueReceive(handle, &out, 0) == pdTRUE;
    }

    bool isEmpty() const {
        if (handle == nullptr) return true;
        return uxQueueMessagesWaiting(handle) == 0;
    }

    size_t size() const {
        if (handle == nullptr) return 0;
        return static_cast<size_t>(uxQueueMessagesWaiting(handle));
    }

private:
    QueueHandle_t handle{nullptr};
};

} // namespace Os
} // namespace deltav

#endif // DELTAV_OS_FREERTOS
