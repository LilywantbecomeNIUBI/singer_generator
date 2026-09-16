#include "bsp_lcd.h"

#include "main.h"

#include <stddef.h>

/*
 * FSMC Bank 4 starts at 0x6C000000. With a 16-bit data bus the MCU address
 * bit A7 drives FSMC_A6, so command and data accesses are separated by 0x80.
 */
#define BSP_LCD_COMMAND_ADDRESS 0x6C00007EUL
#define BSP_LCD_DATA_ADDRESS    0x6C000080UL

#define BSP_LCD_COMMAND (*((volatile uint16_t *)BSP_LCD_COMMAND_ADDRESS))
#define BSP_LCD_DATA    (*((volatile uint16_t *)BSP_LCD_DATA_ADDRESS))

static BSP_LCD_Controller s_controller = BSP_LCD_CONTROLLER_UNKNOWN;

static void BSP_LCD_WriteCommand(uint8_t command)
{
  BSP_LCD_COMMAND = command;
}

static void BSP_LCD_WriteData(uint8_t data)
{
  BSP_LCD_DATA = data;
}

static uint16_t BSP_LCD_ReadData(void)
{
  return BSP_LCD_DATA;
}

static void BSP_LCD_WriteCommandData(uint8_t command,
                                     const uint8_t *data,
                                     uint8_t count)
{
  uint8_t index;

  BSP_LCD_WriteCommand(command);
  for (index = 0U; index < count; ++index)
  {
    BSP_LCD_WriteData(data[index]);
  }
}

static BSP_LCD_Controller BSP_LCD_DetectController(void)
{
  uint16_t id;

  BSP_LCD_WriteCommand(0xD3U);
  (void)BSP_LCD_ReadData();
  (void)BSP_LCD_ReadData();
  id = (uint16_t)((BSP_LCD_ReadData() & 0x00FFU) << 8U);
  id |= (uint16_t)(BSP_LCD_ReadData() & 0x00FFU);
  if (id == (uint16_t)BSP_LCD_CONTROLLER_ILI9341)
  {
    return BSP_LCD_CONTROLLER_ILI9341;
  }

  BSP_LCD_WriteCommand(0x04U);
  (void)BSP_LCD_ReadData();
  (void)BSP_LCD_ReadData();
  id = (uint16_t)((BSP_LCD_ReadData() & 0x00FFU) << 8U);
  id |= (uint16_t)(BSP_LCD_ReadData() & 0x00FFU);
  if ((id == 0x8552U) || (id == (uint16_t)BSP_LCD_CONTROLLER_ST7789))
  {
    return BSP_LCD_CONTROLLER_ST7789;
  }

  return BSP_LCD_CONTROLLER_UNKNOWN;
}

static void BSP_LCD_InitILI9341(void)
{
  static const uint8_t power_b[] = {0x00U, 0xC1U, 0x30U};
  static const uint8_t power_seq[] = {0x64U, 0x03U, 0x12U, 0x81U};
  static const uint8_t driver_timing_a[] = {0x85U, 0x10U, 0x7AU};
  static const uint8_t power_a[] = {0x39U, 0x2CU, 0x00U, 0x34U, 0x02U};
  static const uint8_t pump_ratio[] = {0x20U};
  static const uint8_t driver_timing_b[] = {0x00U, 0x00U};
  static const uint8_t power_control_1[] = {0x1BU};
  static const uint8_t power_control_2[] = {0x01U};
  static const uint8_t vcom_control_1[] = {0x30U, 0x30U};
  static const uint8_t vcom_control_2[] = {0xB7U};
  static const uint8_t pixel_format[] = {0x55U};
  static const uint8_t frame_rate[] = {0x00U, 0x1AU};
  static const uint8_t display_function[] = {0x0AU, 0xA2U};
  static const uint8_t gamma_disable[] = {0x00U};
  static const uint8_t gamma_curve[] = {0x01U};
  static const uint8_t positive_gamma[] = {
      0x0FU, 0x2AU, 0x28U, 0x08U, 0x0EU, 0x08U, 0x54U, 0xA9U,
      0x43U, 0x0AU, 0x0FU, 0x00U, 0x00U, 0x00U, 0x00U};
  static const uint8_t negative_gamma[] = {
      0x00U, 0x15U, 0x17U, 0x07U, 0x11U, 0x06U, 0x2BU, 0x56U,
      0x3CU, 0x05U, 0x10U, 0x0FU, 0x3FU, 0x3FU, 0x0FU};

  BSP_LCD_WriteCommandData(0xCFU, power_b, sizeof(power_b));
  BSP_LCD_WriteCommandData(0xEDU, power_seq, sizeof(power_seq));
  BSP_LCD_WriteCommandData(0xE8U, driver_timing_a, sizeof(driver_timing_a));
  BSP_LCD_WriteCommandData(0xCBU, power_a, sizeof(power_a));
  BSP_LCD_WriteCommandData(0xF7U, pump_ratio, sizeof(pump_ratio));
  BSP_LCD_WriteCommandData(0xEAU, driver_timing_b, sizeof(driver_timing_b));
  BSP_LCD_WriteCommandData(0xC0U, power_control_1, sizeof(power_control_1));
  BSP_LCD_WriteCommandData(0xC1U, power_control_2, sizeof(power_control_2));
  BSP_LCD_WriteCommandData(0xC5U, vcom_control_1, sizeof(vcom_control_1));
  BSP_LCD_WriteCommandData(0xC7U, vcom_control_2, sizeof(vcom_control_2));
  BSP_LCD_WriteCommandData(0x3AU, pixel_format, sizeof(pixel_format));
  BSP_LCD_WriteCommandData(0xB1U, frame_rate, sizeof(frame_rate));
  BSP_LCD_WriteCommandData(0xB6U, display_function, sizeof(display_function));
  BSP_LCD_WriteCommandData(0xF2U, gamma_disable, sizeof(gamma_disable));
  BSP_LCD_WriteCommandData(0x26U, gamma_curve, sizeof(gamma_curve));
  BSP_LCD_WriteCommandData(0xE0U, positive_gamma, sizeof(positive_gamma));
  BSP_LCD_WriteCommandData(0xE1U, negative_gamma, sizeof(negative_gamma));
  BSP_LCD_WriteCommand(0x11U);
  HAL_Delay(120U);
  BSP_LCD_WriteCommand(0x29U);
}

static void BSP_LCD_InitST7789(void)
{
  static const uint8_t pixel_format[] = {0x05U};
  static const uint8_t porch[] = {0x0CU, 0x0CU, 0x00U, 0x33U, 0x33U};
  static const uint8_t gate[] = {0x35U};
  static const uint8_t vcom[] = {0x32U};
  static const uint8_t lcm[] = {0x0CU};
  static const uint8_t vdv_vrh_enable[] = {0x01U};
  static const uint8_t vrh[] = {0x10U};
  static const uint8_t vdv[] = {0x20U};
  static const uint8_t frame_rate[] = {0x0FU};
  static const uint8_t power[] = {0xA4U, 0xA1U};
  static const uint8_t positive_gamma[] = {
      0xD0U, 0x00U, 0x02U, 0x07U, 0x0AU, 0x28U, 0x32U,
      0x44U, 0x42U, 0x06U, 0x0EU, 0x12U, 0x14U, 0x17U};
  static const uint8_t negative_gamma[] = {
      0xD0U, 0x00U, 0x02U, 0x07U, 0x0AU, 0x28U, 0x31U,
      0x54U, 0x47U, 0x0EU, 0x1CU, 0x17U, 0x1BU, 0x1EU};

  BSP_LCD_WriteCommand(0x11U);
  HAL_Delay(120U);
  BSP_LCD_WriteCommandData(0x3AU, pixel_format, sizeof(pixel_format));
  BSP_LCD_WriteCommandData(0xB2U, porch, sizeof(porch));
  BSP_LCD_WriteCommandData(0xB7U, gate, sizeof(gate));
  BSP_LCD_WriteCommandData(0xBBU, vcom, sizeof(vcom));
  BSP_LCD_WriteCommandData(0xC0U, lcm, sizeof(lcm));
  BSP_LCD_WriteCommandData(0xC2U, vdv_vrh_enable, sizeof(vdv_vrh_enable));
  BSP_LCD_WriteCommandData(0xC3U, vrh, sizeof(vrh));
  BSP_LCD_WriteCommandData(0xC4U, vdv, sizeof(vdv));
  BSP_LCD_WriteCommandData(0xC6U, frame_rate, sizeof(frame_rate));
  BSP_LCD_WriteCommandData(0xD0U, power, sizeof(power));
  BSP_LCD_WriteCommandData(0xE0U, positive_gamma, sizeof(positive_gamma));
  BSP_LCD_WriteCommandData(0xE1U, negative_gamma, sizeof(negative_gamma));
  BSP_LCD_WriteCommand(0x29U);
}

static void BSP_LCD_SetLandscapeRotation(void)
{
  static const uint8_t landscape_madctl[] = {0xA8U};

  BSP_LCD_WriteCommandData(0x36U,
                           landscape_madctl,
                           sizeof(landscape_madctl));
}

uint8_t BSP_LCD_Init(void)
{
  BSP_LCD_Backlight(0U);
  HAL_Delay(50U);

  s_controller = BSP_LCD_DetectController();
  if (s_controller == BSP_LCD_CONTROLLER_ILI9341)
  {
    BSP_LCD_InitILI9341();
  }
  else if (s_controller == BSP_LCD_CONTROLLER_ST7789)
  {
    BSP_LCD_InitST7789();
  }
  else
  {
    return 0U;
  }

  BSP_LCD_SetLandscapeRotation();
  BSP_LCD_Fill(BSP_LCD_COLOR_BLACK);
  BSP_LCD_Backlight(1U);
  return 1U;
}

void BSP_LCD_Backlight(uint8_t enabled)
{
  HAL_GPIO_WritePin(LCD_BL_GPIO_Port,
                    LCD_BL_Pin,
                    (enabled != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

BSP_LCD_Controller BSP_LCD_GetController(void)
{
  return s_controller;
}

const char *BSP_LCD_GetControllerName(void)
{
  if (s_controller == BSP_LCD_CONTROLLER_ILI9341)
  {
    return "ILI9341";
  }
  if (s_controller == BSP_LCD_CONTROLLER_ST7789)
  {
    return "ST7789";
  }
  return "UNKNOWN";
}

void BSP_LCD_SetAddressWindow(uint16_t x1,
                              uint16_t y1,
                              uint16_t x2,
                              uint16_t y2)
{
  uint8_t coordinates[4];

  if ((x1 > x2) || (y1 > y2) ||
      (x2 >= BSP_LCD_HOR_RES) || (y2 >= BSP_LCD_VER_RES))
  {
    return;
  }

  coordinates[0] = (uint8_t)(x1 >> 8U);
  coordinates[1] = (uint8_t)x1;
  coordinates[2] = (uint8_t)(x2 >> 8U);
  coordinates[3] = (uint8_t)x2;
  BSP_LCD_WriteCommandData(0x2AU, coordinates, sizeof(coordinates));

  coordinates[0] = (uint8_t)(y1 >> 8U);
  coordinates[1] = (uint8_t)y1;
  coordinates[2] = (uint8_t)(y2 >> 8U);
  coordinates[3] = (uint8_t)y2;
  BSP_LCD_WriteCommandData(0x2BU, coordinates, sizeof(coordinates));

  BSP_LCD_WriteCommand(0x2CU);
}

void BSP_LCD_WritePixels(const uint16_t *pixels, uint32_t count)
{
  if (pixels == NULL)
  {
    return;
  }

  while (count > 0U)
  {
    BSP_LCD_DATA = *pixels;
    ++pixels;
    --count;
  }
}

void BSP_LCD_Fill(uint16_t color)
{
  uint32_t count = (uint32_t)BSP_LCD_HOR_RES * BSP_LCD_VER_RES;

  BSP_LCD_SetAddressWindow(0U,
                           0U,
                           BSP_LCD_HOR_RES - 1U,
                           BSP_LCD_VER_RES - 1U);
  while (count > 0U)
  {
    BSP_LCD_DATA = color;
    --count;
  }
}
