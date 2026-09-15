#ifndef BSP_AD9959_H
#define BSP_AD9959_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"

#include <stdint.h>

#define AD9959_SYSTEM_CLOCK_HZ 500000000UL
#define AD9959_MAX_FREQUENCY_HZ 200000000UL
#define AD9959_MAX_AMPLITUDE 1023U
#define AD9959_FULL_SCALE_PHASE_MDEG 360000UL

typedef enum
{
  AD9959_CHANNEL_0 = 0x10U,
  AD9959_CHANNEL_1 = 0x20U,
  AD9959_CHANNEL_2 = 0x40U,
  AD9959_CHANNEL_3 = 0x80U
} AD9959_Channel;

typedef enum
{
  AD9959_OK = 0,
  AD9959_ERROR_ARGUMENT,
  AD9959_ERROR_HAL
} AD9959_Status;

typedef struct
{
  AD9959_Channel channel;
  uint32_t frequency_hz;
  uint16_t amplitude;
  uint32_t phase_mdeg;
} AD9959_ChannelConfig;

AD9959_Status AD9959_Init(SPI_HandleTypeDef *hspi);
AD9959_Status AD9959_ConfigureChannel(AD9959_Channel channel,
                                     uint32_t frequency_hz,
                                     uint16_t amplitude,
                                     uint32_t phase_mdeg);
AD9959_Status AD9959_SetFrequency(AD9959_Channel channel,
                                 uint32_t frequency_hz);
AD9959_Status AD9959_SetAmplitude(AD9959_Channel channel,
                                 uint16_t amplitude);
AD9959_Status AD9959_SetPhase(AD9959_Channel channel,
                             uint32_t phase_mdeg);
AD9959_Status AD9959_ConfigureChannelsSynchronized(
    const AD9959_ChannelConfig *configs,
    uint32_t config_count);
void AD9959_IOUpdate(void);
uint32_t AD9959_FrequencyToFTW(uint32_t frequency_hz);
uint16_t AD9959_PhaseToPOW(uint32_t phase_mdeg);

#ifdef __cplusplus
}
#endif

#endif /* BSP_AD9959_H */
