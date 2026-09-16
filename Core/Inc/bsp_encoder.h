#ifndef BSP_ENCODER_H
#define BSP_ENCODER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

void BSP_Encoder_Init(void);
void BSP_Encoder_EXTI_Callback(uint16_t gpio_pin);
int16_t BSP_Encoder_TakeDelta(void);
uint8_t BSP_Encoder_IsPressed(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_ENCODER_H */
