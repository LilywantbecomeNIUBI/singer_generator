#ifndef APP_HMI_H
#define APP_HMI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

uint8_t App_HMI_Init(void);
void App_HMI_Process(void);
uint8_t App_HMI_IsReady(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_HMI_H */
