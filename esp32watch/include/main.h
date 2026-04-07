/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "platform_api.h"
#include "board_features.h"

/* Private includes ----------------------------------------------------------*/

/* Exported types ------------------------------------------------------------*/

/* External variables ---------------------------------------------------------*/
extern PlatformI2cBus platform_i2c_main;
extern PlatformRtcDevice platform_rtc_main;
extern PlatformAdcDevice platform_adc_main;
#if APP_FEATURE_COMPANION_UART
extern PlatformUartPort platform_uart_main;
#endif
#if APP_FEATURE_WATCHDOG
extern PlatformWatchdog platform_watchdog_main;
#endif

/* Exported constants --------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);
void system_fatal_trap(uint8_t trap_id, uint16_t value, uint8_t aux);

/* Private defines -----------------------------------------------------------*/

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
