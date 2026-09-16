#ifndef BSP_IR_REMOTE_H
#define BSP_IR_REMOTE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* Address used by the ALIENTEK NEC remote supplied with the board. */
#ifndef BSP_IR_REMOTE_ADDRESS
#define BSP_IR_REMOTE_ADDRESS 0x00U
#endif

typedef struct
{
  uint8_t key;
  uint8_t is_repeat;
} BSP_IR_RemoteEvent;

uint8_t BSP_IR_Remote_Init(void);
uint8_t BSP_IR_Remote_TakeEvent(BSP_IR_RemoteEvent *event);

#ifdef __cplusplus
}
#endif

#endif /* BSP_IR_REMOTE_H */
