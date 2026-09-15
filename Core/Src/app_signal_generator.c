#include "app_signal_generator.h"

#include "bsp_ad9959.h"
#include "spi.h"
#include "usart.h"

#include <stddef.h>
#include <string.h>

#define APP_M1_DEFAULT_FREQUENCY_HZ 1000000UL
#define APP_M1_DEFAULT_AMPLITUDE 1023U
#define APP_M1_DEFAULT_PHASE_MDEG 0UL
#define APP_M2_DEFAULT_CH2_PHASE_MDEG 90000UL

volatile App_M1Status g_app_m1_status = APP_M1_NOT_STARTED;
volatile uint32_t g_app_m1_frequency_hz = 0U;
volatile App_SignalChannelConfig
    g_app_signal_channels[APP_SIGNAL_GENERATOR_CHANNEL_COUNT];

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

uint8_t App_SignalGenerator_ApplyDualChannel(
    const App_SignalChannelConfig *channel_1,
    const App_SignalChannelConfig *channel_2)
{
  AD9959_ChannelConfig dds_configs[APP_SIGNAL_GENERATOR_CHANNEL_COUNT];
  AD9959_Status status;

  if ((App_IsValidChannelConfig(channel_1) == 0U) ||
      (App_IsValidChannelConfig(channel_2) == 0U))
  {
    return 0U;
  }

  dds_configs[0].channel = AD9959_CHANNEL_0;
  dds_configs[0].frequency_hz = channel_1->frequency_hz;
  dds_configs[0].amplitude = channel_1->amplitude;
  dds_configs[0].phase_mdeg = channel_1->phase_mdeg;

  dds_configs[1].channel = AD9959_CHANNEL_1;
  dds_configs[1].frequency_hz = channel_2->frequency_hz;
  dds_configs[1].amplitude = channel_2->amplitude;
  dds_configs[1].phase_mdeg = channel_2->phase_mdeg;

  g_app_m1_status = APP_M2_CONFIGURING;
  status = AD9959_ConfigureChannelsSynchronized(
      dds_configs,
      APP_SIGNAL_GENERATOR_CHANNEL_COUNT);
  if (status != AD9959_OK)
  {
    g_app_m1_status = APP_M1_ERROR;
    return 0U;
  }

  g_app_signal_channels[0] = *channel_1;
  g_app_signal_channels[1] = *channel_2;
  g_app_m1_frequency_hz = channel_1->frequency_hz;
  g_app_m1_status = APP_M2_SYNCHRONIZED_CONFIG_SENT;
  return 1U;
}

uint8_t App_SignalGenerator_Init(void)
{
  static const App_SignalChannelConfig default_channel_1 = {
      APP_M1_DEFAULT_FREQUENCY_HZ,
      APP_M1_DEFAULT_AMPLITUDE,
      APP_M1_DEFAULT_PHASE_MDEG};
  static const App_SignalChannelConfig default_channel_2 = {
      APP_M1_DEFAULT_FREQUENCY_HZ,
      APP_M1_DEFAULT_AMPLITUDE,
      APP_M2_DEFAULT_CH2_PHASE_MDEG};
  AD9959_Status status;

  g_app_m1_status = APP_M1_INITIALIZING;
  g_app_m1_frequency_hz = 0U;

  App_ConsoleWrite("\r\nSignal generator M2 dual-channel start\r\n");
  App_ConsoleWrite("PB3/PB5 use GPIO bit-bang; AD9959 stays in default 2-wire write mode\r\n");
  App_ConsoleWrite("Logical CH1/CH2 map to AD9959 module CH0/CH1\r\n");

  status = AD9959_Init(&hspi1);
  if (status != AD9959_OK)
  {
    g_app_m1_status = APP_M1_ERROR;
    App_ConsoleWrite("ERROR: AD9959 reference-compatible initialization failed\r\n");
    return 0U;
  }

  if (App_SignalGenerator_ApplyDualChannel(&default_channel_1,
                                           &default_channel_2) == 0U)
  {
    g_app_m1_status = APP_M1_ERROR;
    App_ConsoleWrite("ERROR: AD9959 synchronized dual-channel configuration failed\r\n");
    return 0U;
  }

  App_ConsoleWrite("Reference core sequence sent: FR1=D00000 FR2=2000\r\n");
  App_ConsoleWrite("CH1: 1000000 Hz, ASF=1023, phase=0 deg\r\n");
  App_ConsoleWrite("CH2: 1000000 Hz, ASF=1023, phase=90 deg\r\n");
  App_ConsoleWrite("CH1 and CH2 parameters applied by one final IO_UPDATE\r\n");
  App_ConsoleWrite("IMPORTANT: hold SDIO3, SDIO1 and P0-P3 low as in the supplied example\r\n");
  return 1U;
}

void App_SignalGenerator_Process(void)
{
  /* M2 holds the synchronized dual-channel output until a caller applies new parameters. */
}
