#include "lv_port_display.h"

#include "bsp_lcd.h"

#define LV_PORT_DISPLAY_BUFFER_LINES 20U
#define LV_PORT_DISPLAY_BUFFER_PIXELS \
  ((uint32_t)BSP_LCD_HOR_RES * LV_PORT_DISPLAY_BUFFER_LINES)

static uint16_t s_draw_buffer[LV_PORT_DISPLAY_BUFFER_PIXELS]
    __attribute__((aligned(4)));
static lv_display_t *s_display;

static void LV_Port_Display_Flush(lv_display_t *display,
                                  const lv_area_t *area,
                                  uint8_t *pixel_map)
{
  uint32_t width;
  uint32_t height;
  uint32_t pixel_count;

  if ((area == NULL) || (pixel_map == NULL))
  {
    lv_display_flush_ready(display);
    return;
  }

  width = (uint32_t)(area->x2 - area->x1 + 1);
  height = (uint32_t)(area->y2 - area->y1 + 1);
  pixel_count = width * height;

  BSP_LCD_SetAddressWindow((uint16_t)area->x1,
                           (uint16_t)area->y1,
                           (uint16_t)area->x2,
                           (uint16_t)area->y2);
  BSP_LCD_WritePixels((const uint16_t *)pixel_map, pixel_count);
  lv_display_flush_ready(display);
}

uint8_t LV_Port_Display_Init(void)
{
  if (BSP_LCD_Init() == 0U)
  {
    return 0U;
  }

  s_display = lv_display_create(BSP_LCD_HOR_RES, BSP_LCD_VER_RES);
  if (s_display == NULL)
  {
    BSP_LCD_Backlight(0U);
    return 0U;
  }

  lv_display_set_color_format(s_display, LV_COLOR_FORMAT_RGB565);
  lv_display_set_flush_cb(s_display, LV_Port_Display_Flush);
  lv_display_set_buffers(s_display,
                         s_draw_buffer,
                         NULL,
                         sizeof(s_draw_buffer),
                         LV_DISPLAY_RENDER_MODE_PARTIAL);
  return 1U;
}

lv_display_t *LV_Port_Display_Get(void)
{
  return s_display;
}
