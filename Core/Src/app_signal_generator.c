#include "app_signal_generator.h"

#include "bsp_ad9959.h"
#include "main.h"
#include "spi.h"
#include "usart.h"

#include <stddef.h>
#include <string.h>

#define APP_M1_DEFAULT_FREQUENCY_HZ 1000000UL
#define APP_M1_DEFAULT_AMPLITUDE 1023U
#define APP_M1_DEFAULT_PHASE_MDEG 0UL

volatile App_M1Status g_app_m1_status = APP_M1_NOT_STARTED;
volatile uint32_t g_app_m1_frequency_hz = 0U;

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

uint8_t App_SignalGenerator_Init(void)
{
  AD9959_Status status;

  g_app_m1_status = APP_M1_INITIALIZING;
  g_app_m1_frequency_hz = 0U;

  App_ConsoleWrite("\r\nSignal generator M1 start\r\n");
  App_ConsoleWrite("Logical CH1 maps to AD9959 module CH0\r\n");

  status = AD9959_Init(&hspi1);
  if (status != AD9959_OK)
  {
    g_app_m1_status = APP_M1_ERROR;
    App_ConsoleWrite("ERROR: AD9959 initialization SPI transfer failed\r\n");
    return 0U;
  }

  status = AD9959_ConfigureChannel(AD9959_CHANNEL_0,
                                  APP_M1_DEFAULT_FREQUENCY_HZ,
                                  APP_M1_DEFAULT_AMPLITUDE,
                                  APP_M1_DEFAULT_PHASE_MDEG);
  if (status != AD9959_OK)
  {
    g_app_m1_status = APP_M1_ERROR;
    App_ConsoleWrite("ERROR: AD9959 CH0 configuration SPI transfer failed\r\n");
    return 0U;
  }

  g_app_m1_frequency_hz = APP_M1_DEFAULT_FREQUENCY_HZ;
  g_app_m1_status = APP_M1_SPI_CONFIG_SENT;
  App_ConsoleWrite("AD9959 configuration sent: CH0, 1000000 Hz, phase 0, amplitude 1023\r\n");
  App_ConsoleWrite("Measure CH0 with an oscilloscope; output is not software-verifiable\r\n");
  return 1U;
}

void App_SignalGenerator_Process(void)
{
  /* M1 is a fixed point-frequency output. Runtime control is added later. */
}
