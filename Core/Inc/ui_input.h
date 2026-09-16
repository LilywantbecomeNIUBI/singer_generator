#ifndef UI_INPUT_H
#define UI_INPUT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ui_command.h"

#include <stdint.h>

void UI_Input_Init(void);
void UI_Input_Process(void);
void UI_Input_EXTI_Callback(uint16_t gpio_pin);

/* Hardware adapters for the later TPAD and infrared receiver stages. */
void UI_Input_PostTpad(uint8_t long_press);
void UI_Input_PostIRDigit(uint8_t digit);
void UI_Input_PostIRCommand(UI_Command command);

#ifdef __cplusplus
}
#endif

#endif /* UI_INPUT_H */
