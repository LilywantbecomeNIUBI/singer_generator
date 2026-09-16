#include "lv_port_input.h"

#include "bsp_key.h"
#include "bsp_touch_hr2046.h"

#include <stddef.h>

static lv_indev_t *s_pointer;
static lv_indev_t *s_keypad;
static lv_group_t *s_keypad_group;
static lv_point_t s_last_touch_point;
static uint32_t s_last_key = LV_KEY_ENTER;

static uint32_t LV_Port_Input_MapKey(BSP_Key key)
{
  switch (key)
  {
    case BSP_KEY_0:
      return LV_KEY_ESC;
    case BSP_KEY_1:
      return LV_KEY_ENTER;
    case BSP_KEY_2:
      return LV_KEY_NEXT;
    case BSP_KEY_UP:
      return LV_KEY_PREV;
    case BSP_KEY_NONE:
    case BSP_KEY_COUNT:
    default:
      return s_last_key;
  }
}

static void LV_Port_Input_ReadPointer(lv_indev_t *indev,
                                      lv_indev_data_t *data)
{
  uint16_t x;
  uint16_t y;

  (void)indev;
  if (BSP_Touch_Read(&x, &y) != 0U)
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

static void LV_Port_Input_ReadKeypad(lv_indev_t *indev,
                                     lv_indev_data_t *data)
{
  BSP_Key key;

  (void)indev;
  key = BSP_Key_GetPressed();
  if (key != BSP_KEY_NONE)
  {
    s_last_key = LV_Port_Input_MapKey(key);
    data->state = LV_INDEV_STATE_PRESSED;
  }
  else
  {
    data->state = LV_INDEV_STATE_RELEASED;
  }
  data->key = s_last_key;
}

void LV_Port_Input_Init(lv_display_t *display)
{
  BSP_Touch_Init();
  BSP_Key_Init();

  s_keypad_group = lv_group_create();
  if (s_keypad_group != NULL)
  {
    lv_group_set_default(s_keypad_group);
  }

  s_pointer = lv_indev_create();
  if (s_pointer != NULL)
  {
    lv_indev_set_type(s_pointer, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(s_pointer, LV_Port_Input_ReadPointer);
    lv_indev_set_display(s_pointer, display);
  }

  s_keypad = lv_indev_create();
  if (s_keypad != NULL)
  {
    lv_indev_set_type(s_keypad, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(s_keypad, LV_Port_Input_ReadKeypad);
    lv_indev_set_display(s_keypad, display);
    if (s_keypad_group != NULL)
    {
      lv_indev_set_group(s_keypad, s_keypad_group);
    }
  }
}

lv_indev_t *LV_Port_Input_GetPointer(void)
{
  return s_pointer;
}

lv_indev_t *LV_Port_Input_GetKeypad(void)
{
  return s_keypad;
}
