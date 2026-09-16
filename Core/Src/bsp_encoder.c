#include "bsp_encoder.h"

#include "main.h"

#define BSP_ENCODER_EDGE_DEBOUNCE_MS 2U
#define BSP_ENCODER_CW_WHEN_S2_HIGH 1U

static volatile int16_t s_encoder_delta;
static volatile uint32_t s_last_edge_ms;

void BSP_Encoder_Init(void)
{
  s_encoder_delta = 0;
  s_last_edge_ms = HAL_GetTick();
}

void BSP_Encoder_EXTI_Callback(uint16_t gpio_pin)
{
  uint32_t now;
  uint8_t s2_high;

  if (gpio_pin != ENCODER_S1_Pin)
  {
    return;
  }

  now = HAL_GetTick();
  if ((now - s_last_edge_ms) < BSP_ENCODER_EDGE_DEBOUNCE_MS)
  {
    return;
  }
  s_last_edge_ms = now;

  s2_high = (HAL_GPIO_ReadPin(ENCODER_S2_GPIO_Port,
                              ENCODER_S2_Pin) == GPIO_PIN_SET);
#if BSP_ENCODER_CW_WHEN_S2_HIGH
  s_encoder_delta += (s2_high != 0U) ? 1 : -1;
#else
  s_encoder_delta += (s2_high != 0U) ? -1 : 1;
#endif
}

int16_t BSP_Encoder_TakeDelta(void)
{
  uint32_t primask = __get_PRIMASK();
  int16_t delta;

  __disable_irq();
  delta = s_encoder_delta;
  s_encoder_delta = 0;
  if (primask == 0U)
  {
    __enable_irq();
  }
  return delta;
}

uint8_t BSP_Encoder_IsPressed(void)
{
  return (HAL_GPIO_ReadPin(ENCODER_KEY_GPIO_Port,
                           ENCODER_KEY_Pin) == GPIO_PIN_RESET);
}
