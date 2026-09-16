#ifndef UI_SIGNAL_GENERATOR_H
#define UI_SIGNAL_GENERATOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "generator_state.h"
#include "ui_command.h"

void UI_SignalGenerator_Init(void);
void UI_SignalGenerator_Process(void);
void UI_SignalGenerator_Dispatch(UI_Command command);
const generator_state_t *UI_SignalGenerator_GetState(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_SIGNAL_GENERATOR_H */
