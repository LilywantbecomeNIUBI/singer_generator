#include "lv_port_input.h"

#include "bsp_touch_hr2046.h"

#include <stddef.h>

static lv_indev_t *s_pointer;
static lv_point_t s_last_touch_point;
static uint8_t s_pointer_enabled = 1U;

static void LV_Port_Input_ReadPointer(lv_indev_t *indev,
                                      lv_indev_data_t *data)
{
  uint16_t x;
  uint16_t y;

  (void)indev;
  if ((s_pointer_enabled != 0U) && (BSP_Touch_Read(&x, &y) != 0U))
  {
    s_last_touch_point.x = (int32_t)x;
    s_last_touch_point.y = (int32_t)y;
    data->state = LV_INDEV_STATE_PRESSED;
  }
  else
  {
    data->state = LV_INDEV_STATE_RELEASED;
  }
  data->point = s_last_touch_point;
}

void LV_Port_Input_Init(lv_display_t *display)
{
  BSP_Touch_Init();
  s_pointer_enabled = 1U;

  s_pointer = lv_indev_create();
  if (s_pointer != NULL)
  {
    lv_indev_set_type(s_pointer, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(s_pointer, LV_Port_Input_ReadPointer);
    lv_indev_set_display(s_pointer, display);
  }

}

lv_indev_t *LV_Port_Input_GetPointer(void)
{
  return s_pointer;
}

lv_indev_t *LV_Port_Input_GetKeypad(void)
{
  return NULL;
}

void LV_Port_Input_SetPointerEnabled(uint8_t enabled)
{
  s_pointer_enabled = (enabled != 0U) ? 1U : 0U;
}
