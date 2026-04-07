#include "reset_reason.h"
#include <Arduino.h>
#include <esp32-hal.h>

static ResetReason g_reset_reason = RESET_REASON_UNKNOWN;

void reset_reason_init(void)
{
    esp_reset_reason_t rr = esp_reset_reason();
    switch (rr) {
        case ESP_RST_POWERON: g_reset_reason = RESET_REASON_POWER_ON; break;
        case ESP_RST_EXT: g_reset_reason = RESET_REASON_PIN; break;
        case ESP_RST_SW: g_reset_reason = RESET_REASON_SOFTWARE; break;
        case ESP_RST_INT_WDT:
        case ESP_RST_TASK_WDT:
        case ESP_RST_WDT: g_reset_reason = RESET_REASON_IWDG; break;
        case ESP_RST_DEEPSLEEP: g_reset_reason = RESET_REASON_LOW_POWER; break;
        default: g_reset_reason = RESET_REASON_UNKNOWN; break;
    }
}

ResetReason reset_reason_get(void)
{
    return g_reset_reason;
}

const char *reset_reason_name(ResetReason reason)
{
    switch (reason) {
        case RESET_REASON_POWER_ON: return "POWER_ON";
        case RESET_REASON_PIN: return "PIN";
        case RESET_REASON_SOFTWARE: return "SOFTWARE";
        case RESET_REASON_IWDG: return "IWDG";
        case RESET_REASON_WWDG: return "WWDG";
        case RESET_REASON_LOW_POWER: return "LOW_POWER";
        default: return "UNKNOWN";
    }
}
