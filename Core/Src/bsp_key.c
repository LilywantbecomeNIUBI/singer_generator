#include "bsp_key.h"

#include "main.h"

#define BSP_KEY_DEBOUNCE_MS 20U

typedef struct
{
  uint8_t raw_pressed;
  uint8_t stable_pressed;
  uint32_t changed_at_ms;
} BSP_KeyState;

static BSP_KeyState s_key_states[BSP_KEY_COUNT];

static uint8_t BSP_Key_ReadRaw(BSP_Key key)
{
  switch (key)
  {
    case BSP_KEY_0:
      return (HAL_GPIO_ReadPin(KEY0_GPIO_Port, KEY0_Pin) == GPIO_PIN_RESET);
    case BSP_KEY_1:
      return (HAL_GPIO_ReadPin(KEY1_GPIO_Port, KEY1_Pin) == GPIO_PIN_RESET);
    case BSP_KEY_2:
      return (HAL_GPIO_ReadPin(KEY2_GPIO_Port, KEY2_Pin) == GPIO_PIN_RESET);
    case BSP_KEY_UP:
      return (HAL_GPIO_ReadPin(KEY_UP_GPIO_Port, KEY_UP_Pin) == GPIO_PIN_SET);
    case BSP_KEY_NONE:
    case BSP_KEY_COUNT:
    default:
      return 0U;
  }
}

void BSP_Key_Init(void)
{
  BSP_Key key;
  uint32_t now = HAL_GetTick();

  for (key = BSP_KEY_0; key < BSP_KEY_COUNT; ++key)
  {
    uint8_t pressed = BSP_Key_ReadRaw(key);
    s_key_states[key].raw_pressed = pressed;
    s_key_states[key].stable_pressed = pressed;
    s_key_states[key].changed_at_ms = now;
  }
}

void BSP_Key_Process(void)
{
  BSP_Key key;
  uint32_t now = HAL_GetTick();

  for (key = BSP_KEY_0; key < BSP_KEY_COUNT; ++key)
  {
    uint8_t pressed = BSP_Key_ReadRaw(key);
    BSP_KeyState *state = &s_key_states[key];

    if (pressed != state->raw_pressed)
    {
      state->raw_pressed = pressed;
      state->changed_at_ms = now;
    }
    else if (((now - state->changed_at_ms) >= BSP_KEY_DEBOUNCE_MS) &&
             (state->stable_pressed != pressed))
    {
      state->stable_pressed = pressed;
    }
  }
}

uint8_t BSP_Key_IsPressed(BSP_Key key)
{
  if ((key <= BSP_KEY_NONE) || (key >= BSP_KEY_COUNT))
  {
    return 0U;
  }
  return s_key_states[key].stable_pressed;
}

BSP_Key BSP_Key_GetPressed(void)
{
  BSP_Key key;

  for (key = BSP_KEY_0; key < BSP_KEY_COUNT; ++key)
  {
    if (BSP_Key_IsPressed(key) != 0U)
    {
      return key;
    }
  }
  return BSP_KEY_NONE;
}
