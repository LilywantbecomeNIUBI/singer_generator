#ifndef LV_PORT_INPUT_H
#define LV_PORT_INPUT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

void LV_Port_Input_Init(lv_display_t *display);
lv_indev_t *LV_Port_Input_GetPointer(void);
lv_indev_t *LV_Port_Input_GetKeypad(void);
void LV_Port_Input_SetPointerEnabled(uint8_t enabled);

#ifdef __cplusplus
}
#endif

#endif /* LV_PORT_INPUT_H */
