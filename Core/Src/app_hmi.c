#include "app_hmi.h"

#include "bsp_key.h"
#include "bsp_lcd.h"
#include "lv_port_display.h"
#include "lv_port_input.h"
#include "lvgl.h"
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

static void App_HMI_CreateBringUpScreen(void)
{
  lv_obj_t *screen = lv_screen_active();
  lv_obj_t *title;
  lv_obj_t *details;

  lv_obj_set_style_bg_color(screen, lv_color_hex(0x101820U), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

  title = lv_label_create(screen);
  lv_label_set_text(title, "LVGL display ready");
  lv_obj_set_style_text_color(title, lv_color_hex(0x57D3FFU), LV_PART_MAIN);
  lv_obj_align(title, LV_ALIGN_CENTER, 0, -18);

  details = lv_label_create(screen);
  lv_label_set_text_fmt(details,
                        "LVGL 9.5.0 | %s | 320 x 240",
                        BSP_LCD_GetControllerName());
  lv_obj_set_style_text_color(details, lv_color_hex(0xE8EEF2U), LV_PART_MAIN);
  lv_obj_align(details, LV_ALIGN_CENTER, 0, 18);
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
  App_HMI_CreateBringUpScreen();
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
  BSP_Key_Process();
  (void)lv_timer_handler();
}

uint8_t App_HMI_IsReady(void)
{
  return s_hmi_ready;
}
