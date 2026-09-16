#ifndef UI_SIGNAL_ADAPTER_H
#define UI_SIGNAL_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

void UI_SignalAdapter_Init(void);
void UI_SignalAdapter_Process(void);
uint8_t UI_SignalAdapter_GetLastApplyOk(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_SIGNAL_ADAPTER_H */
