#include "bsp_touch_hr2046.h"

#include "bsp_lcd.h"
#include "main.h"

#include <stddef.h>

#define BSP_TOUCH_COMMAND_X 0xD0U
#define BSP_TOUCH_COMMAND_Y 0x90U
#define BSP_TOUCH_SAMPLE_COUNT 5U

/*
 * The LCD uses ST7789 MADCTL 0xA8 in landscape mode (MY | MV | BGR).
 * Therefore logical X comes from the raw Y channel in reverse order, while
 * logical Y comes from the raw X channel in its normal order.
 */
static BSP_TouchCalibration s_calibration = {
    200U,
    3900U,
    200U,
    3900U,
    1U,
    1U,
    0U};

static void BSP_Touch_ClockDelay(void)
{
  uint32_t delay;

  for (delay = 0U; delay < 16U; ++delay)
  {
    __NOP();
  }
}

static void BSP_Touch_WriteByte(uint8_t value)
{
  uint8_t bit;

  for (bit = 0U; bit < 8U; ++bit)
  {
    HAL_GPIO_WritePin(T_CLK_GPIO_Port, T_CLK_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(T_MOSI_GPIO_Port,
                      T_MOSI_Pin,
                      ((value & 0x80U) != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    BSP_Touch_ClockDelay();
    HAL_GPIO_WritePin(T_CLK_GPIO_Port, T_CLK_Pin, GPIO_PIN_SET);
    BSP_Touch_ClockDelay();
    value <<= 1U;
  }
  HAL_GPIO_WritePin(T_CLK_GPIO_Port, T_CLK_Pin, GPIO_PIN_RESET);
}

static uint16_t BSP_Touch_ReadChannel(uint8_t command)
{
  uint16_t value = 0U;
  uint8_t bit;

  HAL_GPIO_WritePin(T_CS_GPIO_Port, T_CS_Pin, GPIO_PIN_RESET);
  BSP_Touch_WriteByte(command);

  for (bit = 0U; bit < 16U; ++bit)
  {
    value <<= 1U;
    HAL_GPIO_WritePin(T_CLK_GPIO_Port, T_CLK_Pin, GPIO_PIN_SET);
    BSP_Touch_ClockDelay();
    if (HAL_GPIO_ReadPin(T_MISO_GPIO_Port, T_MISO_Pin) == GPIO_PIN_SET)
    {
      value |= 1U;
    }
    HAL_GPIO_WritePin(T_CLK_GPIO_Port, T_CLK_Pin, GPIO_PIN_RESET);
    BSP_Touch_ClockDelay();
  }

  HAL_GPIO_WritePin(T_CS_GPIO_Port, T_CS_Pin, GPIO_PIN_SET);
  return (uint16_t)(value >> 3U);
}

static void BSP_Touch_Sort(uint16_t *values)
{
  uint8_t outer;
  uint8_t inner;

  for (outer = 0U; outer < (BSP_TOUCH_SAMPLE_COUNT - 1U); ++outer)
  {
    for (inner = (uint8_t)(outer + 1U);
         inner < BSP_TOUCH_SAMPLE_COUNT;
         ++inner)
    {
      if (values[inner] < values[outer])
      {
        uint16_t temporary = values[outer];
        values[outer] = values[inner];
        values[inner] = temporary;
      }
    }
  }
}

static uint16_t BSP_Touch_Scale(uint16_t raw,
                                uint16_t raw_min,
                                uint16_t raw_max,
                                uint16_t resolution)
{
  uint32_t scaled;

  if ((raw_max <= raw_min) || (resolution == 0U))
  {
    return 0U;
  }
  if (raw <= raw_min)
  {
    return 0U;
  }
  if (raw >= raw_max)
  {
    return (uint16_t)(resolution - 1U);
  }

  scaled = ((uint32_t)(raw - raw_min) * (resolution - 1U)) /
           (uint32_t)(raw_max - raw_min);
  return (uint16_t)scaled;
}

void BSP_Touch_Init(void)
{
  HAL_GPIO_WritePin(T_CS_GPIO_Port, T_CS_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(T_CLK_GPIO_Port, T_CLK_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(T_MOSI_GPIO_Port, T_MOSI_Pin, GPIO_PIN_RESET);
}

void BSP_Touch_SetCalibration(const BSP_TouchCalibration *calibration)
{
  if ((calibration == NULL) ||
      (calibration->raw_x_max <= calibration->raw_x_min) ||
      (calibration->raw_y_max <= calibration->raw_y_min))
  {
    return;
  }

  s_calibration = *calibration;
}

uint8_t BSP_Touch_ReadRaw(uint16_t *raw_x, uint16_t *raw_y)
{
  uint16_t x_samples[BSP_TOUCH_SAMPLE_COUNT];
  uint16_t y_samples[BSP_TOUCH_SAMPLE_COUNT];
  uint8_t index;

  if ((raw_x == NULL) || (raw_y == NULL) ||
      (HAL_GPIO_ReadPin(T_PEN_GPIO_Port, T_PEN_Pin) != GPIO_PIN_RESET))
  {
    return 0U;
  }

  for (index = 0U; index < BSP_TOUCH_SAMPLE_COUNT; ++index)
  {
    x_samples[index] = BSP_Touch_ReadChannel(BSP_TOUCH_COMMAND_X);
    y_samples[index] = BSP_Touch_ReadChannel(BSP_TOUCH_COMMAND_Y);
  }

  if (HAL_GPIO_ReadPin(T_PEN_GPIO_Port, T_PEN_Pin) != GPIO_PIN_RESET)
  {
    return 0U;
  }

  BSP_Touch_Sort(x_samples);
  BSP_Touch_Sort(y_samples);
  *raw_x = x_samples[BSP_TOUCH_SAMPLE_COUNT / 2U];
  *raw_y = y_samples[BSP_TOUCH_SAMPLE_COUNT / 2U];
  return 1U;
}

uint8_t BSP_Touch_Read(uint16_t *x, uint16_t *y)
{
  uint16_t raw_x;
  uint16_t raw_y;
  uint16_t screen_x;
  uint16_t screen_y;

  if ((x == NULL) || (y == NULL) ||
      (BSP_Touch_ReadRaw(&raw_x, &raw_y) == 0U))
  {
    return 0U;
  }

  if (s_calibration.swap_xy != 0U)
  {
    screen_x = BSP_Touch_Scale(raw_y,
                               s_calibration.raw_y_min,
                               s_calibration.raw_y_max,
                               BSP_LCD_HOR_RES);
    screen_y = BSP_Touch_Scale(raw_x,
                               s_calibration.raw_x_min,
                               s_calibration.raw_x_max,
                               BSP_LCD_VER_RES);
  }
  else
  {
    screen_x = BSP_Touch_Scale(raw_x,
                               s_calibration.raw_x_min,
                               s_calibration.raw_x_max,
                               BSP_LCD_HOR_RES);
    screen_y = BSP_Touch_Scale(raw_y,
                               s_calibration.raw_y_min,
                               s_calibration.raw_y_max,
                               BSP_LCD_VER_RES);
  }

  if (s_calibration.invert_x != 0U)
  {
    screen_x = (uint16_t)(BSP_LCD_HOR_RES - 1U - screen_x);
  }
  if (s_calibration.invert_y != 0U)
  {
    screen_y = (uint16_t)(BSP_LCD_VER_RES - 1U - screen_y);
  }

  *x = screen_x;
  *y = screen_y;
  return 1U;
}
