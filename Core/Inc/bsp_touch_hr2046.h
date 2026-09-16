#ifndef BSP_TOUCH_HR2046_H
#define BSP_TOUCH_HR2046_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct
{
  uint16_t raw_x_min;
  uint16_t raw_x_max;
  uint16_t raw_y_min;
  uint16_t raw_y_max;
  uint8_t swap_xy;
  uint8_t invert_x;
  uint8_t invert_y;
} BSP_TouchCalibration;

void BSP_Touch_Init(void);
void BSP_Touch_SetCalibration(const BSP_TouchCalibration *calibration);
uint8_t BSP_Touch_ReadRaw(uint16_t *raw_x, uint16_t *raw_y);
uint8_t BSP_Touch_Read(uint16_t *x, uint16_t *y);

#ifdef __cplusplus
}
#endif

#endif /* BSP_TOUCH_HR2046_H */
