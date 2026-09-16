#ifndef LV_PORT_DISPLAY_H
#define LV_PORT_DISPLAY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "lvgl.h"

uint8_t LV_Port_Display_Init(void);
lv_display_t *LV_Port_Display_Get(void);

#ifdef __cplusplus
}
#endif

#endif /* LV_PORT_DISPLAY_H */
