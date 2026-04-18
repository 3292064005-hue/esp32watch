#include <Arduino.h>
extern "C" {
#include "main.h"
#include "watch_app.h"
#include "system_init.h"
#include "crash_capsule.h"
#include "companion_transport.h"
}

extern "C" void system_fatal_trap(uint8_t trap_id, uint16_t value, uint8_t aux)
{
    Serial.printf("[FATAL] trap=%u value=%u aux=%u\n", trap_id, value, aux);
    delay(50);
    ESP.restart();
}

extern "C" void Error_Handler(void)
{
    Serial.printf("[FATAL] code=%u policy=%u detail=%lu\n",
                  (unsigned)system_last_fatal_code(),
                  (unsigned)system_last_fatal_policy(),
                  (unsigned long)system_last_fatal_detail());
    delay(50);
    ESP.restart();
}

void setup()
{
    Serial.begin(115200);
    delay(100);

    SystemRuntimeCapabilities caps;
    WatchAppInitReport app_init_report;

    system_bootstrap();
    if (!system_board_peripheral_init()) {
        return;
    }
    if (!system_runtime_capability_probe(&caps)) {
        system_raise_fatal(SYSTEM_FATAL_CODE_CAPABILITY_CONTRACT,
                           SYSTEM_INIT_STAGE_CAPABILITY_PROBE,
                           0U,
                           SYSTEM_FATAL_POLICY_STOP);
        return;
    }
    if (!system_runtime_service_init()) {
        return;
    }
    if (!system_web_service_init()) {
        return;
    }
    if (!watch_app_init_checked(&app_init_report)) {
        system_raise_fatal(SYSTEM_FATAL_CODE_INIT_STAGE_FAILURE,
                           SYSTEM_INIT_STAGE_APP,
                           (uint32_t)app_init_report.failed_stage,
                           SYSTEM_FATAL_POLICY_STOP);
        return;
    }
    system_mark_app_initialized();
#if APP_FEATURE_COMPANION_UART
    if (!companion_transport_init()) {
        system_raise_fatal(SYSTEM_FATAL_CODE_INIT_STAGE_FAILURE,
                           SYSTEM_INIT_STAGE_COMPANION_TRANSPORT,
                           0U,
                           SYSTEM_FATAL_POLICY_STOP);
        return;
    }
    system_mark_companion_transport_initialized();
#endif
    Serial.println("[BOOT] watch app initialized on ESP32-S3");
}

void loop()
{
#if APP_FEATURE_COMPANION_UART
    companion_transport_poll();
#endif
    watch_app_task();
    delay(1);
}
