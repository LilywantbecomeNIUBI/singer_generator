#ifndef APP_SIGNAL_GENERATOR_H
#define APP_SIGNAL_GENERATOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define APP_SIGNAL_GENERATOR_CHANNEL_COUNT 4U
#define APP_SIGNAL_GENERATOR_M1_TEST_POINT_COUNT 6U
#define APP_SIGNAL_GENERATOR_MIN_FREQUENCY_HZ 1UL
#define APP_SIGNAL_GENERATOR_MAX_FREQUENCY_HZ 30000000UL
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
  APP_SIGNAL_PATTERN_CUSTOM = 0,
  APP_SIGNAL_PATTERN_SINGLE_CH1,
  APP_SIGNAL_PATTERN_IN_PHASE,
  APP_SIGNAL_PATTERN_IQ,
  APP_SIGNAL_PATTERN_FOUR_PHASE
} App_SignalPattern;

typedef struct
{
  uint32_t start_frequency_hz;
  uint32_t stop_frequency_hz;
  uint32_t step_frequency_hz;
  uint32_t dwell_time_ms;
  uint8_t repeat;
} App_SignalSweepConfig;

typedef enum
{
  APP_SIGNAL_SWEEP_STOPPED = 0,
  APP_SIGNAL_SWEEP_RUNNING,
  APP_SIGNAL_SWEEP_COMPLETE,
  APP_SIGNAL_SWEEP_ERROR
} App_SignalSweepStatus;

typedef enum
{
  APP_M1_NOT_STARTED = 0,
  APP_M1_INITIALIZING,
  APP_M1_REFERENCE_SEQUENCE_SENT,
  APP_M2_CONFIGURING,
  APP_M2_SYNCHRONIZED_CONFIG_SENT,
  APP_M4_CONFIGURING,
  APP_M4_SYNCHRONIZED_CONFIG_SENT,
  APP_M7_SWEEP_RUNNING,
  APP_M7_SWEEP_COMPLETE,
  APP_M1_ERROR
} App_M1Status;

extern volatile App_M1Status g_app_m1_status;
extern volatile uint32_t g_app_m1_frequency_hz;
extern volatile App_SignalChannelConfig
    g_app_signal_channels[APP_SIGNAL_GENERATOR_CHANNEL_COUNT];
extern volatile App_SignalPattern g_app_signal_pattern;
extern volatile App_SignalSweepStatus g_app_signal_sweep_status;
extern volatile uint32_t g_app_signal_sweep_frequency_hz;
extern const uint32_t
    g_app_m1_test_frequencies_hz[APP_SIGNAL_GENERATOR_M1_TEST_POINT_COUNT];

uint8_t App_SignalGenerator_Init(void);
uint8_t App_SignalGenerator_ApplyChannels(
    const App_SignalChannelConfig
        channels[APP_SIGNAL_GENERATOR_CHANNEL_COUNT]);
uint8_t App_SignalGenerator_ApplyDualChannel(
    const App_SignalChannelConfig *channel_1,
    const App_SignalChannelConfig *channel_2);
uint8_t App_SignalGenerator_ApplyM1TestPoint(uint8_t test_point_index);
uint8_t App_SignalGenerator_SetPattern(App_SignalPattern pattern,
                                       uint32_t frequency_hz,
                                       uint16_t amplitude);
uint8_t App_SignalGenerator_StartSweep(
    const App_SignalSweepConfig *config);
void App_SignalGenerator_StopSweep(void);
void App_SignalGenerator_Process(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_SIGNAL_GENERATOR_H */
