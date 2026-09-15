#ifndef APP_SIGNAL_GENERATOR_H
#define APP_SIGNAL_GENERATOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define APP_SIGNAL_GENERATOR_CHANNEL_COUNT 2U
#define APP_SIGNAL_GENERATOR_MIN_FREQUENCY_HZ 1UL
#define APP_SIGNAL_GENERATOR_MAX_FREQUENCY_HZ 1000000UL
#define APP_SIGNAL_GENERATOR_MAX_AMPLITUDE 1023U
#define APP_SIGNAL_GENERATOR_FULL_SCALE_PHASE_MDEG 360000UL

typedef struct
{
  uint32_t frequency_hz;
  uint16_t amplitude;
  uint32_t phase_mdeg;
} App_SignalChannelConfig;

typedef enum
{
  APP_M1_NOT_STARTED = 0,
  APP_M1_INITIALIZING,
  APP_M1_REFERENCE_SEQUENCE_SENT,
  APP_M2_CONFIGURING,
  APP_M2_SYNCHRONIZED_CONFIG_SENT,
  APP_M1_ERROR
} App_M1Status;

extern volatile App_M1Status g_app_m1_status;
extern volatile uint32_t g_app_m1_frequency_hz;
extern volatile App_SignalChannelConfig
    g_app_signal_channels[APP_SIGNAL_GENERATOR_CHANNEL_COUNT];

uint8_t App_SignalGenerator_Init(void);
uint8_t App_SignalGenerator_ApplyDualChannel(
    const App_SignalChannelConfig *channel_1,
    const App_SignalChannelConfig *channel_2);
void App_SignalGenerator_Process(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_SIGNAL_GENERATOR_H */
