#ifndef BSP_KEY_H
#define BSP_KEY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef enum
{
  BSP_KEY_NONE = 0,
  BSP_KEY_0,
  BSP_KEY_1,
  BSP_KEY_2,
  BSP_KEY_UP,
  BSP_KEY_COUNT
} BSP_Key;

void BSP_Key_Init(void);
void BSP_Key_Process(void);
uint8_t BSP_Key_IsPressed(BSP_Key key);
BSP_Key BSP_Key_GetPressed(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_KEY_H */
