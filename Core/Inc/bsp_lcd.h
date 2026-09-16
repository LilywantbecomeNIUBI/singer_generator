#ifndef BSP_LCD_H
#define BSP_LCD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define BSP_LCD_HOR_RES 320U
#define BSP_LCD_VER_RES 240U

#define BSP_LCD_COLOR_BLACK 0x0000U
#define BSP_LCD_COLOR_WHITE 0xFFFFU
#define BSP_LCD_COLOR_RED   0xF800U
#define BSP_LCD_COLOR_GREEN 0x07E0U
#define BSP_LCD_COLOR_BLUE  0x001FU

typedef enum
{
  BSP_LCD_CONTROLLER_UNKNOWN = 0x0000U,
  BSP_LCD_CONTROLLER_ILI9341 = 0x9341U,
  BSP_LCD_CONTROLLER_ST7789 = 0x7789U
} BSP_LCD_Controller;

uint8_t BSP_LCD_Init(void);
void BSP_LCD_Backlight(uint8_t enabled);
BSP_LCD_Controller BSP_LCD_GetController(void);
const char *BSP_LCD_GetControllerName(void);
void BSP_LCD_SetAddressWindow(uint16_t x1,
                              uint16_t y1,
                              uint16_t x2,
                              uint16_t y2);
void BSP_LCD_WritePixels(const uint16_t *pixels, uint32_t count);
void BSP_LCD_Fill(uint16_t color);

#ifdef __cplusplus
}
#endif

#endif /* BSP_LCD_H */
