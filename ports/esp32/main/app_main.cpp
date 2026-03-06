#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern auto main() -> int;

namespace {

// ESP-IDF main task stack (3.5 KB default) is too small for full framework init.
constexpr uint32_t DELTAV_TASK_STACK_BYTES = 16 * 1024;
constexpr UBaseType_t DELTAV_TASK_PRIORITY = tskIDLE_PRIORITY + 1;

auto deltav_task(void* /*arg*/) -> void {
    (void)main();
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

} // namespace

extern "C" void app_main(void) {
    BaseType_t rc = xTaskCreatePinnedToCore(
        deltav_task,
        "deltav_task",
        DELTAV_TASK_STACK_BYTES,
        nullptr,
        DELTAV_TASK_PRIORITY,
        nullptr,
        tskNO_AFFINITY);

    if (rc == pdPASS) {
        // app_main task can terminate once the dedicated framework task is running.
        return;
    }

    // Fallback: keep app_main alive if task creation fails.
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
