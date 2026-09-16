#include "ui_input.h"

#include "bsp_encoder.h"
#include "bsp_key.h"
#include "main.h"

#define UI_INPUT_LONG_PRESS_MS 1000U
#define UI_INPUT_ENCODER_DEBOUNCE_MS 25U
#define UI_INPUT_ENCODER_MAX_EVENTS_PER_PROCESS 8

typedef struct
{
  uint8_t was_pressed;
  uint8_t long_sent;
  uint32_t pressed_at_ms;
} UI_ButtonTracker;

static UI_ButtonTracker s_key_trackers[BSP_KEY_COUNT];
static UI_ButtonTracker s_encoder_key;
static uint8_t s_encoder_raw;
static uint8_t s_encoder_stable;
static uint32_t s_encoder_changed_at_ms;

static UI_Command UI_Input_GetShortCommand(BSP_Key key)
{
  switch (key)
  {
    case BSP_KEY_0:
      return UI_CMD_BACK;
    case BSP_KEY_1:
      return UI_CMD_CH_NEXT;
    case BSP_KEY_2:
      return UI_CMD_OUTPUT_TOGGLE;
    case BSP_KEY_UP:
      return UI_CMD_WAVE_NEXT;
    case BSP_KEY_NONE:
    case BSP_KEY_COUNT:
    default:
      return UI_CMD_NONE;
  }
}

static UI_Command UI_Input_GetLongCommand(BSP_Key key)
{
  switch (key)
  {
    case BSP_KEY_0:
      return UI_CMD_HOME;
    case BSP_KEY_1:
      return UI_CMD_CH_OVERVIEW;
    case BSP_KEY_2:
      return UI_CMD_OUTPUT_ALL_OFF;
    case BSP_KEY_UP:
      return UI_CMD_WAVE_PAGE;
    case BSP_KEY_NONE:
    case BSP_KEY_COUNT:
    default:
      return UI_CMD_NONE;
  }
}

static void UI_Input_ProcessTracker(UI_ButtonTracker *tracker,
                                    uint8_t pressed,
                                    UI_Command short_command,
                                    UI_Command long_command,
                                    uint32_t now)
{
  if ((pressed != 0U) && (tracker->was_pressed == 0U))
  {
    tracker->was_pressed = 1U;
    tracker->long_sent = 0U;
    tracker->pressed_at_ms = now;
  }
  else if ((pressed != 0U) && (tracker->long_sent == 0U) &&
           ((now - tracker->pressed_at_ms) >= UI_INPUT_LONG_PRESS_MS))
  {
    (void)UI_CommandQueue_Post(long_command);
    tracker->long_sent = 1U;
  }
  else if ((pressed == 0U) && (tracker->was_pressed != 0U))
  {
    if (tracker->long_sent == 0U)
    {
      (void)UI_CommandQueue_Post(short_command);
    }
    tracker->was_pressed = 0U;
  }
}

void UI_Input_Init(void)
{
  BSP_Key key;
  uint32_t now = HAL_GetTick();

  BSP_Key_Init();
  BSP_Encoder_Init();
  for (key = BSP_KEY_0; key < BSP_KEY_COUNT; ++key)
  {
    s_key_trackers[key].was_pressed = BSP_Key_IsPressed(key);
    s_key_trackers[key].long_sent = 0U;
    s_key_trackers[key].pressed_at_ms = now;
  }

  s_encoder_raw = BSP_Encoder_IsPressed();
  s_encoder_stable = s_encoder_raw;
  s_encoder_changed_at_ms = now;
  s_encoder_key.was_pressed = s_encoder_stable;
  s_encoder_key.long_sent = 0U;
  s_encoder_key.pressed_at_ms = now;
}

void UI_Input_Process(void)
{
  BSP_Key key;
  int16_t delta;
  int16_t event_count;
  uint8_t pressed;
  uint32_t now = HAL_GetTick();

  BSP_Key_Process();
  for (key = BSP_KEY_0; key < BSP_KEY_COUNT; ++key)
  {
    UI_Input_ProcessTracker(&s_key_trackers[key],
                            BSP_Key_IsPressed(key),
                            UI_Input_GetShortCommand(key),
                            UI_Input_GetLongCommand(key),
                            now);
  }

  pressed = BSP_Encoder_IsPressed();
  if (pressed != s_encoder_raw)
  {
    s_encoder_raw = pressed;
    s_encoder_changed_at_ms = now;
  }
  else if (((now - s_encoder_changed_at_ms) >=
            UI_INPUT_ENCODER_DEBOUNCE_MS) &&
           (s_encoder_stable != pressed))
  {
    s_encoder_stable = pressed;
  }
  UI_Input_ProcessTracker(&s_encoder_key,
                          s_encoder_stable,
                          UI_CMD_ENCODER_PRESS,
                          UI_CMD_ENCODER_LONG,
                          now);

  delta = BSP_Encoder_TakeDelta();
  if (delta > UI_INPUT_ENCODER_MAX_EVENTS_PER_PROCESS)
  {
    delta = UI_INPUT_ENCODER_MAX_EVENTS_PER_PROCESS;
  }
  else if (delta < -UI_INPUT_ENCODER_MAX_EVENTS_PER_PROCESS)
  {
    delta = -UI_INPUT_ENCODER_MAX_EVENTS_PER_PROCESS;
  }

  event_count = (delta >= 0) ? delta : (int16_t)-delta;
  while (event_count > 0)
  {
    (void)UI_CommandQueue_Post((delta > 0) ? UI_CMD_ENCODER_CW
                                           : UI_CMD_ENCODER_CCW);
    --event_count;
  }
}

void UI_Input_EXTI_Callback(uint16_t gpio_pin)
{
  BSP_Encoder_EXTI_Callback(gpio_pin);
}

void UI_Input_PostTpad(uint8_t long_press)
{
  (void)UI_CommandQueue_Post((long_press != 0U) ? UI_CMD_TOUCH_LOCK
                                                : UI_CMD_MENU);
}

void UI_Input_PostIRDigit(uint8_t digit)
{
  if (digit <= 9U)
  {
    (void)UI_CommandQueue_Post((UI_Command)(UI_CMD_NUM_0 + digit));
  }
}

void UI_Input_PostIRCommand(UI_Command command)
{
  (void)UI_CommandQueue_Post(command);
}
