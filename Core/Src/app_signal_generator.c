#include "app_signal_generator.h"

#include "bsp_ad9959.h"
#include "spi.h"
#include "usart.h"

#include <stddef.h>
#include <string.h>

#define APP_M1_DEFAULT_FREQUENCY_HZ 1000000UL
#define APP_M1_DEFAULT_AMPLITUDE 1023U
#define APP_M7_DEFAULT_SWEEP_START_HZ 1000000UL
#define APP_M7_DEFAULT_SWEEP_STOP_HZ 10000000UL
#define APP_M7_DEFAULT_SWEEP_STEP_HZ 1000000UL
#define APP_M7_DEFAULT_SWEEP_DWELL_MS 500UL

static const AD9959_Channel s_ad9959_channels[
    APP_SIGNAL_GENERATOR_CHANNEL_COUNT] = {
    AD9959_CHANNEL_0,
    AD9959_CHANNEL_1,
    AD9959_CHANNEL_2,
    AD9959_CHANNEL_3};

const uint32_t g_app_m1_test_frequencies_hz[
    APP_SIGNAL_GENERATOR_M1_TEST_POINT_COUNT] = {
    1000UL,
    100000UL,
    1000000UL,
    5000000UL,
    10000000UL,
    20000000UL};

volatile App_M1Status g_app_m1_status = APP_M1_NOT_STARTED;
volatile uint32_t g_app_m1_frequency_hz = 0U;
volatile App_SignalChannelConfig
    g_app_signal_channels[APP_SIGNAL_GENERATOR_CHANNEL_COUNT];
volatile App_SignalPattern g_app_signal_pattern = APP_SIGNAL_PATTERN_CUSTOM;
volatile App_SignalSweepStatus g_app_signal_sweep_status =
    APP_SIGNAL_SWEEP_STOPPED;
volatile uint32_t g_app_signal_sweep_frequency_hz = 0U;

static App_SignalSweepConfig s_sweep_config;
static uint32_t s_sweep_last_step_ms;

static uint8_t App_ApplyChannelsInternal(
    const App_SignalChannelConfig
        channels[APP_SIGNAL_GENERATOR_CHANNEL_COUNT]);

static void App_ConsoleWrite(const char *message)
{
  if (message == NULL)
  {
    return;
  }

  (void)HAL_UART_Transmit(&huart1,
                         (uint8_t *)message,
                         (uint16_t)strlen(message),
                         100U);
}

static uint8_t App_IsValidChannelConfig(
    const App_SignalChannelConfig *config)
{
  return ((config != NULL) &&
          (config->frequency_hz >= APP_SIGNAL_GENERATOR_MIN_FREQUENCY_HZ) &&
          (config->frequency_hz <= APP_SIGNAL_GENERATOR_MAX_FREQUENCY_HZ) &&
          (config->amplitude <= APP_SIGNAL_GENERATOR_MAX_AMPLITUDE) &&
          (config->phase_mdeg <
           APP_SIGNAL_GENERATOR_FULL_SCALE_PHASE_MDEG));
}

static void App_ConsolePrintHelp(void)
{
  App_ConsoleWrite(
      "Commands: 0..5=M1 points, c=CH1, a=in-phase, i=I/Q, q=4-phase, "
      "s=sweep, r=repeat sweep, x=stop, h=help\r\n");
}

static void App_ConsoleApplyPattern(App_SignalPattern pattern)
{
  if (App_SignalGenerator_SetPattern(pattern,
                                     g_app_m1_frequency_hz,
                                     APP_M1_DEFAULT_AMPLITUDE) != 0U)
  {
    App_ConsoleWrite("ACK: signal pattern applied\r\n");
  }
  else
  {
    App_ConsoleWrite("ERROR: signal pattern rejected\r\n");
  }
}

static void App_ConsoleStartSweep(uint8_t repeat)
{
  const App_SignalSweepConfig sweep = {
      APP_M7_DEFAULT_SWEEP_START_HZ,
      APP_M7_DEFAULT_SWEEP_STOP_HZ,
      APP_M7_DEFAULT_SWEEP_STEP_HZ,
      APP_M7_DEFAULT_SWEEP_DWELL_MS,
      repeat};

  if (App_SignalGenerator_StartSweep(&sweep) != 0U)
  {
    App_ConsoleWrite("ACK: 1..10 MHz software sweep started\r\n");
  }
  else
  {
    App_ConsoleWrite("ERROR: software sweep rejected\r\n");
  }
}

static void App_ConsoleProcess(void)
{
  uint8_t command;

  if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_RXNE) == RESET)
  {
    return;
  }
  if (HAL_UART_Receive(&huart1, &command, 1U, 0U) != HAL_OK)
  {
    return;
  }

  if ((command >= (uint8_t)'0') && (command <= (uint8_t)'5'))
  {
    if (App_SignalGenerator_ApplyM1TestPoint(
            (uint8_t)(command - (uint8_t)'0')) != 0U)
    {
      App_ConsoleWrite("ACK: M1 frequency point applied\r\n");
    }
    else
    {
      App_ConsoleWrite("ERROR: M1 frequency point rejected\r\n");
    }
    return;
  }

  switch (command)
  {
    case (uint8_t)'c':
      App_ConsoleApplyPattern(APP_SIGNAL_PATTERN_SINGLE_CH1);
      break;

    case (uint8_t)'a':
      App_ConsoleApplyPattern(APP_SIGNAL_PATTERN_IN_PHASE);
      break;

    case (uint8_t)'i':
      App_ConsoleApplyPattern(APP_SIGNAL_PATTERN_IQ);
      break;

    case (uint8_t)'q':
      App_ConsoleApplyPattern(APP_SIGNAL_PATTERN_FOUR_PHASE);
      break;

    case (uint8_t)'s':
      App_ConsoleStartSweep(0U);
      break;

    case (uint8_t)'r':
      App_ConsoleStartSweep(1U);
      break;

    case (uint8_t)'x':
      App_SignalGenerator_StopSweep();
      App_ConsoleWrite("ACK: software sweep stopped\r\n");
      break;

    case (uint8_t)'h':
    case (uint8_t)'?':
      App_ConsolePrintHelp();
      break;

    case (uint8_t)'\r':
    case (uint8_t)'\n':
      break;

    default:
      App_ConsoleWrite("ERROR: unknown command; send h for help\r\n");
      break;
  }
}

static uint8_t App_IsValidFrequency(uint32_t frequency_hz)
{
  return ((frequency_hz >= APP_SIGNAL_GENERATOR_MIN_FREQUENCY_HZ) &&
          (frequency_hz <= APP_SIGNAL_GENERATOR_MAX_FREQUENCY_HZ));
}

static uint8_t App_ApplyCommonFrequency(uint32_t frequency_hz)
{
  App_SignalChannelConfig channels[APP_SIGNAL_GENERATOR_CHANNEL_COUNT];
  uint32_t index;

  if (App_IsValidFrequency(frequency_hz) == 0U)
  {
    return 0U;
  }

  for (index = 0U; index < APP_SIGNAL_GENERATOR_CHANNEL_COUNT; ++index)
  {
    channels[index].frequency_hz = frequency_hz;
    channels[index].amplitude = g_app_signal_channels[index].amplitude;
    channels[index].phase_mdeg = g_app_signal_channels[index].phase_mdeg;
  }

  return App_ApplyChannelsInternal(channels);
}

static uint8_t App_ApplyChannelsInternal(
    const App_SignalChannelConfig
        channels[APP_SIGNAL_GENERATOR_CHANNEL_COUNT])
{
  AD9959_ChannelConfig dds_configs[APP_SIGNAL_GENERATOR_CHANNEL_COUNT];
  AD9959_Status status;
  uint32_t index;

  if (channels == NULL)
  {
    return 0U;
  }

  for (index = 0U; index < APP_SIGNAL_GENERATOR_CHANNEL_COUNT; ++index)
  {
    if (App_IsValidChannelConfig(&channels[index]) == 0U)
    {
      return 0U;
    }

    dds_configs[index].channel = s_ad9959_channels[index];
    dds_configs[index].frequency_hz = channels[index].frequency_hz;
    dds_configs[index].amplitude = channels[index].amplitude;
    dds_configs[index].phase_mdeg = channels[index].phase_mdeg;
  }

  g_app_m1_status = APP_M4_CONFIGURING;
  status = AD9959_ConfigureChannelsSynchronized(
      dds_configs,
      APP_SIGNAL_GENERATOR_CHANNEL_COUNT);
  if (status != AD9959_OK)
  {
    g_app_m1_status = APP_M1_ERROR;
    return 0U;
  }

  for (index = 0U; index < APP_SIGNAL_GENERATOR_CHANNEL_COUNT; ++index)
  {
    g_app_signal_channels[index] = channels[index];
  }
  g_app_m1_frequency_hz = channels[0].frequency_hz;
  g_app_m1_status = APP_M4_SYNCHRONIZED_CONFIG_SENT;
  return 1U;
}

uint8_t App_SignalGenerator_ApplyChannels(
    const App_SignalChannelConfig
        channels[APP_SIGNAL_GENERATOR_CHANNEL_COUNT])
{
  App_SignalGenerator_StopSweep();
  if (App_ApplyChannelsInternal(channels) == 0U)
  {
    return 0U;
  }

  g_app_signal_pattern = APP_SIGNAL_PATTERN_CUSTOM;
  return 1U;
}

uint8_t App_SignalGenerator_ApplyChannel(
    uint32_t channel_index,
    const App_SignalChannelConfig *config)
{
  AD9959_ChannelConfig dds_config;
  AD9959_Status status;

  if ((channel_index >= APP_SIGNAL_GENERATOR_CHANNEL_COUNT) ||
      (App_IsValidChannelConfig(config) == 0U))
  {
    return 0U;
  }

  dds_config.channel = s_ad9959_channels[channel_index];
  dds_config.frequency_hz = config->frequency_hz;
  dds_config.amplitude = config->amplitude;
  dds_config.phase_mdeg = config->phase_mdeg;

  App_SignalGenerator_StopSweep();
  g_app_m1_status = APP_M4_CONFIGURING;
  status = AD9959_ConfigureChannelsSynchronized(&dds_config, 1U);
  if (status != AD9959_OK)
  {
    g_app_m1_status = APP_M1_ERROR;
    return 0U;
  }

  g_app_signal_channels[channel_index] = *config;
  if (channel_index == 0U)
  {
    g_app_m1_frequency_hz = config->frequency_hz;
  }
  g_app_signal_pattern = APP_SIGNAL_PATTERN_CUSTOM;
  g_app_m1_status = APP_M4_SYNCHRONIZED_CONFIG_SENT;
  return 1U;
}

uint8_t App_SignalGenerator_ApplyDualChannel(
    const App_SignalChannelConfig *channel_1,
    const App_SignalChannelConfig *channel_2)
{
  App_SignalChannelConfig channels[APP_SIGNAL_GENERATOR_CHANNEL_COUNT];
  uint32_t index;

  if ((App_IsValidChannelConfig(channel_1) == 0U) ||
      (App_IsValidChannelConfig(channel_2) == 0U))
  {
    return 0U;
  }

  channels[0] = *channel_1;
  channels[1] = *channel_2;
  for (index = 2U; index < APP_SIGNAL_GENERATOR_CHANNEL_COUNT; ++index)
  {
    channels[index].frequency_hz = channel_1->frequency_hz;
    channels[index].amplitude = 0U;
    channels[index].phase_mdeg = 0U;
  }

  g_app_m1_status = APP_M2_CONFIGURING;
  if (App_SignalGenerator_ApplyChannels(channels) == 0U)
  {
    g_app_m1_status = APP_M1_ERROR;
    return 0U;
  }

  g_app_signal_pattern = APP_SIGNAL_PATTERN_CUSTOM;
  g_app_m1_status = APP_M2_SYNCHRONIZED_CONFIG_SENT;
  return 1U;
}

uint8_t App_SignalGenerator_SetPattern(App_SignalPattern pattern,
                                       uint32_t frequency_hz,
                                       uint16_t amplitude)
{
  static const uint32_t in_phase_mdeg[APP_SIGNAL_GENERATOR_CHANNEL_COUNT] = {
      0UL, 0UL, 0UL, 0UL};
  static const uint32_t iq_phase_mdeg[APP_SIGNAL_GENERATOR_CHANNEL_COUNT] = {
      0UL, 90000UL, 0UL, 0UL};
  static const uint32_t four_phase_mdeg[
      APP_SIGNAL_GENERATOR_CHANNEL_COUNT] = {
      0UL, 90000UL, 180000UL, 270000UL};
  const uint32_t *phase_mdeg;
  App_SignalChannelConfig channels[APP_SIGNAL_GENERATOR_CHANNEL_COUNT];
  uint32_t index;

  if ((App_IsValidFrequency(frequency_hz) == 0U) ||
      (amplitude > APP_SIGNAL_GENERATOR_MAX_AMPLITUDE))
  {
    return 0U;
  }

  switch (pattern)
  {
    case APP_SIGNAL_PATTERN_CUSTOM:
      return 0U;

    case APP_SIGNAL_PATTERN_SINGLE_CH1:
    case APP_SIGNAL_PATTERN_IN_PHASE:
      phase_mdeg = in_phase_mdeg;
      break;

    case APP_SIGNAL_PATTERN_IQ:
      phase_mdeg = iq_phase_mdeg;
      break;

    case APP_SIGNAL_PATTERN_FOUR_PHASE:
      phase_mdeg = four_phase_mdeg;
      break;

    default:
      return 0U;
  }

  for (index = 0U; index < APP_SIGNAL_GENERATOR_CHANNEL_COUNT; ++index)
  {
    channels[index].frequency_hz = frequency_hz;
    channels[index].amplitude = amplitude;
    channels[index].phase_mdeg = phase_mdeg[index];
  }

  if (pattern == APP_SIGNAL_PATTERN_SINGLE_CH1)
  {
    channels[1].amplitude = 0U;
    channels[2].amplitude = 0U;
    channels[3].amplitude = 0U;
  }
  else if (pattern == APP_SIGNAL_PATTERN_IQ)
  {
    channels[2].amplitude = 0U;
    channels[3].amplitude = 0U;
  }

  App_SignalGenerator_StopSweep();
  if (App_SignalGenerator_ApplyChannels(channels) == 0U)
  {
    return 0U;
  }

  g_app_signal_pattern = pattern;
  return 1U;
}

uint8_t App_SignalGenerator_ApplyM1TestPoint(uint8_t test_point_index)
{
  if (test_point_index >= APP_SIGNAL_GENERATOR_M1_TEST_POINT_COUNT)
  {
    return 0U;
  }

  return App_SignalGenerator_SetPattern(
      APP_SIGNAL_PATTERN_SINGLE_CH1,
      g_app_m1_test_frequencies_hz[test_point_index],
      APP_M1_DEFAULT_AMPLITUDE);
}

uint8_t App_SignalGenerator_StartSweep(
    const App_SignalSweepConfig *config)
{
  if ((config == NULL) ||
      (App_IsValidFrequency(config->start_frequency_hz) == 0U) ||
      (App_IsValidFrequency(config->stop_frequency_hz) == 0U) ||
      (config->step_frequency_hz == 0U) ||
      (config->dwell_time_ms == 0U))
  {
    g_app_signal_sweep_status = APP_SIGNAL_SWEEP_ERROR;
    return 0U;
  }

  s_sweep_config = *config;
  s_sweep_config.repeat = (config->repeat != 0U) ? 1U : 0U;

  if (App_ApplyCommonFrequency(config->start_frequency_hz) == 0U)
  {
    g_app_signal_sweep_status = APP_SIGNAL_SWEEP_ERROR;
    return 0U;
  }

  g_app_signal_sweep_frequency_hz = config->start_frequency_hz;
  s_sweep_last_step_ms = HAL_GetTick();
  if (config->start_frequency_hz == config->stop_frequency_hz)
  {
    g_app_signal_sweep_status = APP_SIGNAL_SWEEP_COMPLETE;
    g_app_m1_status = APP_M7_SWEEP_COMPLETE;
  }
  else
  {
    g_app_signal_sweep_status = APP_SIGNAL_SWEEP_RUNNING;
    g_app_m1_status = APP_M7_SWEEP_RUNNING;
  }
  return 1U;
}

void App_SignalGenerator_StopSweep(void)
{
  g_app_signal_sweep_status = APP_SIGNAL_SWEEP_STOPPED;
}

uint8_t App_SignalGenerator_Init(void)
{
  AD9959_Status status;

  g_app_m1_status = APP_M1_INITIALIZING;
  g_app_m1_frequency_hz = 0U;

  App_ConsoleWrite("\r\nSignal generator V0.4 signal-core start\r\n");
  App_ConsoleWrite("PB3/PB5 use GPIO bit-bang; AD9959 stays in default 2-wire write mode\r\n");
  App_ConsoleWrite("Logical CH1..CH4 map to AD9959 module CH0..CH3\r\n");

  status = AD9959_Init(&hspi1);
  if (status != AD9959_OK)
  {
    g_app_m1_status = APP_M1_ERROR;
    App_ConsoleWrite("ERROR: AD9959 reference-compatible initialization failed\r\n");
    return 0U;
  }

  /* V0.4 freezes boot at M1: CH1 only, 1 MHz, before VCA810 is connected. */
  if (App_SignalGenerator_SetPattern(APP_SIGNAL_PATTERN_SINGLE_CH1,
                                     APP_M1_DEFAULT_FREQUENCY_HZ,
                                     APP_M1_DEFAULT_AMPLITUDE) == 0U)
  {
    g_app_m1_status = APP_M1_ERROR;
    App_ConsoleWrite("ERROR: AD9959 M1 single-channel configuration failed\r\n");
    return 0U;
  }
  g_app_m1_status = APP_M1_REFERENCE_SEQUENCE_SENT;

  App_ConsoleWrite("Reference core sequence sent: FR1=D00000 FR2=2000\r\n");
  App_ConsoleWrite("M1 boot baseline: CH1=1000000 Hz, ASF=1023, phase=0 deg\r\n");
  App_ConsoleWrite("CH2..CH4 muted; M1 test points and M4/M7 APIs are ready\r\n");
  App_ConsoleWrite("IMPORTANT: hold SDIO3, SDIO1 and P0-P3 low as in the supplied example\r\n");
  App_ConsolePrintHelp();
  return 1U;
}

void App_SignalGenerator_Process(void)
{
  uint32_t elapsed_ms;
  uint32_t next_frequency_hz;
  uint32_t remaining_hz;

  App_ConsoleProcess();

  if (g_app_signal_sweep_status != APP_SIGNAL_SWEEP_RUNNING)
  {
    return;
  }

  elapsed_ms = HAL_GetTick() - s_sweep_last_step_ms;
  if (elapsed_ms < s_sweep_config.dwell_time_ms)
  {
    return;
  }

  if ((s_sweep_config.repeat != 0U) &&
      (g_app_signal_sweep_frequency_hz ==
       s_sweep_config.stop_frequency_hz))
  {
    next_frequency_hz = s_sweep_config.start_frequency_hz;
  }
  else if (s_sweep_config.start_frequency_hz <
           s_sweep_config.stop_frequency_hz)
  {
    remaining_hz = s_sweep_config.stop_frequency_hz -
                   g_app_signal_sweep_frequency_hz;
    next_frequency_hz =
        (s_sweep_config.step_frequency_hz >= remaining_hz)
            ? s_sweep_config.stop_frequency_hz
            : (g_app_signal_sweep_frequency_hz +
               s_sweep_config.step_frequency_hz);
  }
  else
  {
    remaining_hz = g_app_signal_sweep_frequency_hz -
                   s_sweep_config.stop_frequency_hz;
    next_frequency_hz =
        (s_sweep_config.step_frequency_hz >= remaining_hz)
            ? s_sweep_config.stop_frequency_hz
            : (g_app_signal_sweep_frequency_hz -
               s_sweep_config.step_frequency_hz);
  }

  if (App_ApplyCommonFrequency(next_frequency_hz) == 0U)
  {
    g_app_signal_sweep_status = APP_SIGNAL_SWEEP_ERROR;
    g_app_m1_status = APP_M1_ERROR;
    return;
  }

  g_app_signal_sweep_frequency_hz = next_frequency_hz;
  s_sweep_last_step_ms = HAL_GetTick();

  if (next_frequency_hz == s_sweep_config.stop_frequency_hz)
  {
    if (s_sweep_config.repeat == 0U)
    {
      g_app_signal_sweep_status = APP_SIGNAL_SWEEP_COMPLETE;
      g_app_m1_status = APP_M7_SWEEP_COMPLETE;
    }
    else
    {
      g_app_m1_status = APP_M7_SWEEP_RUNNING;
    }
  }
  else
  {
    g_app_m1_status = APP_M7_SWEEP_RUNNING;
  }
}
