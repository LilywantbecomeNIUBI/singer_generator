#ifndef APP_SIGNAL_GENERATOR_H
#define APP_SIGNAL_GENERATOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef enum
{
  APP_M1_NOT_STARTED = 0,
  APP_M1_INITIALIZING,
  APP_M1_SPI_CONFIG_SENT,
  APP_M1_ERROR
} App_M1Status;

extern volatile App_M1Status g_app_m1_status;
extern volatile uint32_t g_app_m1_frequency_hz;

uint8_t App_SignalGenerator_Init(void);
void App_SignalGenerator_Process(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_SIGNAL_GENERATOR_H */
