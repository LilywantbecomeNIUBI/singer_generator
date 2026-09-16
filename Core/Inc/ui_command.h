#ifndef UI_COMMAND_H
#define UI_COMMAND_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef enum
{
  UI_CMD_NONE = 0,
  UI_CMD_BACK,
  UI_CMD_HOME,
  UI_CMD_CH_NEXT,
  UI_CMD_CH_PREV,
  UI_CMD_CH_OVERVIEW,
  UI_CMD_OUTPUT_TOGGLE,
  UI_CMD_OUTPUT_ALL_OFF,
  UI_CMD_WAVE_NEXT,
  UI_CMD_WAVE_PAGE,
  UI_CMD_MENU,
  UI_CMD_TOUCH_LOCK,
  UI_CMD_PARAM_PREV,
  UI_CMD_PARAM_NEXT,
  UI_CMD_DECIMAL_POINT,
  UI_CMD_ENCODER_CW,
  UI_CMD_ENCODER_CCW,
  UI_CMD_ENCODER_PRESS,
  UI_CMD_ENCODER_LONG,
  UI_CMD_NUM_0,
  UI_CMD_NUM_1,
  UI_CMD_NUM_2,
  UI_CMD_NUM_3,
  UI_CMD_NUM_4,
  UI_CMD_NUM_5,
  UI_CMD_NUM_6,
  UI_CMD_NUM_7,
  UI_CMD_NUM_8,
  UI_CMD_NUM_9,
  UI_CMD_ENTER,
  UI_CMD_DELETE,
  UI_CMD_PARAM_FREQ,
  UI_CMD_PARAM_AMPLITUDE,
  UI_CMD_PARAM_OFFSET,
  UI_CMD_PARAM_PHASE,
  UI_CMD_CH_1,
  UI_CMD_CH_2,
  UI_CMD_CH_3,
  UI_CMD_CH_4,
  UI_CMD_WAVE_SINE,
  UI_CMD_WAVE_SQUARE,
  UI_CMD_WAVE_TRIANGLE,
  UI_CMD_WAVE_PULSE
} UI_Command;

void UI_CommandQueue_Init(void);
uint8_t UI_CommandQueue_Post(UI_Command command);
uint8_t UI_CommandQueue_PostFromISR(UI_Command command);
uint8_t UI_CommandQueue_Get(UI_Command *command);

#ifdef __cplusplus
}
#endif

#endif /* UI_COMMAND_H */
