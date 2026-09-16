#include "app_hmi.h"

#include "bsp_lcd.h"
#include "lv_port_display.h"
#include "lv_port_input.h"
#include "lvgl.h"
#include "ui_command.h"
#include "ui_input.h"
#include "ui_signal_adapter.h"
#include "ui_signal_generator.h"
#include "usart.h"

#include <stdio.h>
#include <string.h>

#define APP_HMI_HANDLER_PERIOD_MS 5U

static uint8_t s_hmi_ready;
static uint32_t s_last_handler_ms;

static uint32_t App_HMI_GetTick(void)
{
  return HAL_GetTick();
}

static void App_HMI_ConsoleWrite(const char *message)
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

uint8_t App_HMI_Init(void)
{
  char message[80];

  s_hmi_ready = 0U;
  lv_init();
  lv_tick_set_cb(App_HMI_GetTick);

  if (LV_Port_Display_Init() == 0U)
  {
    App_HMI_ConsoleWrite("HMI ERROR: unsupported or unreadable LCD controller ID\r\n");
    return 0U;
  }

  LV_Port_Input_Init(LV_Port_Display_Get());
  UI_CommandQueue_Init();
  UI_Input_Init();
  UI_SignalGenerator_Init();
  UI_SignalAdapter_Init();
  UI_SignalAdapter_Process();
  s_last_handler_ms = HAL_GetTick();
  s_hmi_ready = 1U;

  (void)snprintf(message,
                 sizeof(message),
                 "HMI ready: LVGL 9.5.0, LCD=%s, 320x240 RGB565\r\n",
                 BSP_LCD_GetControllerName());
  App_HMI_ConsoleWrite(message);
  return 1U;
}

void App_HMI_Process(void)
{
  uint32_t now;
  UI_Command command;

  if (s_hmi_ready == 0U)
  {
    return;
  }

  now = HAL_GetTick();
  if ((now - s_last_handler_ms) < APP_HMI_HANDLER_PERIOD_MS)
  {
    return;
  }

  s_last_handler_ms = now;
  UI_Input_Process();
  while (UI_CommandQueue_Get(&command) != 0U)
  {
    UI_SignalGenerator_Dispatch(command);
    UI_SignalAdapter_Process();
  }
  UI_SignalGenerator_Process();
  UI_SignalAdapter_Process();
  (void)lv_timer_handler();
}

uint8_t App_HMI_IsReady(void)
{
  return s_hmi_ready;
}

void HAL_GPIO_EXTI_Callback(uint16_t gpio_pin)
{
  UI_Input_EXTI_Callback(gpio_pin);
}
