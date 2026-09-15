#include "bsp_ad9959.h"

#include "main.h"

#include <stddef.h>

#define AD9959_REG_CSR 0x00U
#define AD9959_REG_FR1 0x01U
#define AD9959_REG_FR2 0x02U
#define AD9959_REG_CFTW0 0x04U
#define AD9959_REG_CPOW0 0x05U
#define AD9959_REG_ACR 0x06U

/* Reference-example-compatible software serial pins. */
#define AD9959_SCLK_GPIO_Port GPIOB
#define AD9959_SCLK_Pin GPIO_PIN_3
#define AD9959_SDIO0_GPIO_Port GPIOB
#define AD9959_SDIO0_Pin GPIO_PIN_5
#define AD9959_SDIO2_GPIO_Port GPIOB
#define AD9959_SDIO2_Pin GPIO_PIN_4

static uint8_t s_ad9959_initialized;

static uint8_t AD9959_IsValidChannel(AD9959_Channel channel)
{
  return ((channel == AD9959_CHANNEL_0) ||
          (channel == AD9959_CHANNEL_1) ||
          (channel == AD9959_CHANNEL_2) ||
          (channel == AD9959_CHANNEL_3));
}

static void AD9959_UpdateDelay(void)
{
  volatile uint32_t delay;

  /* Covers more than one 6.25 MHz SYNC_CLK period before PLL lock. */
  for (delay = 0U; delay < 256U; ++delay)
  {
    __NOP();
  }
}

static void AD9959_WriteByte(uint8_t value)
{
  uint8_t bit;

  for (bit = 0U; bit < 8U; ++bit)
  {
    HAL_GPIO_WritePin(AD9959_SCLK_GPIO_Port,
                      AD9959_SCLK_Pin,
                      GPIO_PIN_RESET);
    HAL_GPIO_WritePin(AD9959_SDIO0_GPIO_Port,
                      AD9959_SDIO0_Pin,
                      ((value & 0x80U) != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(AD9959_SCLK_GPIO_Port,
                      AD9959_SCLK_Pin,
                      GPIO_PIN_SET);
    value <<= 1U;
  }

  HAL_GPIO_WritePin(AD9959_SCLK_GPIO_Port,
                    AD9959_SCLK_Pin,
                    GPIO_PIN_RESET);
}

static AD9959_Status AD9959_WriteRegister(uint8_t address,
                                          const uint8_t *data,
                                          uint16_t length,
                                          uint8_t apply_update)
{
  uint16_t index;

  if (s_ad9959_initialized == 0U)
  {
    return AD9959_ERROR_HAL;
  }
  if ((data == NULL) || (length == 0U) || (address > 0x1FU))
  {
    return AD9959_ERROR_ARGUMENT;
  }

  HAL_GPIO_WritePin(SPI_FLASH_CS_GPIO_Port, SPI_FLASH_CS_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(AD9959_SCLK_GPIO_Port,
                    AD9959_SCLK_Pin,
                    GPIO_PIN_RESET);
  HAL_GPIO_WritePin(AD9959_CS_GPIO_Port, AD9959_CS_Pin, GPIO_PIN_RESET);

  AD9959_WriteByte(address & 0x1FU);
  for (index = 0U; index < length; ++index)
  {
    AD9959_WriteByte(data[index]);
  }

  /* Match the supplied example: update while CS is still asserted. */
  if (apply_update != 0U)
  {
    AD9959_IOUpdate();
  }

  HAL_GPIO_WritePin(AD9959_CS_GPIO_Port, AD9959_CS_Pin, GPIO_PIN_SET);
  return AD9959_OK;
}

static AD9959_Status AD9959_SelectChannel(AD9959_Channel channel)
{
  uint8_t csr;

  if (AD9959_IsValidChannel(channel) == 0U)
  {
    return AD9959_ERROR_ARGUMENT;
  }

  csr = (uint8_t)channel;
  return AD9959_WriteRegister(AD9959_REG_CSR, &csr, 1U, 1U);
}

static AD9959_Status AD9959_SelectChannelWithoutUpdate(AD9959_Channel channel)
{
  uint8_t csr;

  if (AD9959_IsValidChannel(channel) == 0U)
  {
    return AD9959_ERROR_ARGUMENT;
  }

  /* CSR[7:4] channel-enable bits take effect as soon as the byte is written. */
  csr = (uint8_t)channel;
  return AD9959_WriteRegister(AD9959_REG_CSR, &csr, 1U, 0U);
}

static AD9959_Status AD9959_ValidateChannelConfig(
    const AD9959_ChannelConfig *config)
{
  if ((config == NULL) ||
      (AD9959_IsValidChannel(config->channel) == 0U) ||
      (config->frequency_hz > AD9959_MAX_FREQUENCY_HZ) ||
      (config->amplitude > AD9959_MAX_AMPLITUDE))
  {
    return AD9959_ERROR_ARGUMENT;
  }

  return AD9959_OK;
}

static AD9959_Status AD9959_StageChannelConfig(
    const AD9959_ChannelConfig *config)
{
  AD9959_Status status;
  uint32_t ftw;
  uint16_t pow;
  uint8_t frequency_bytes[4];
  uint8_t phase_bytes[2];
  uint8_t amplitude_bytes[3];

  status = AD9959_ValidateChannelConfig(config);
  if (status != AD9959_OK)
  {
    return status;
  }

  status = AD9959_SelectChannelWithoutUpdate(config->channel);
  if (status != AD9959_OK)
  {
    return status;
  }

  ftw = AD9959_FrequencyToFTW(config->frequency_hz);
  frequency_bytes[0] = (uint8_t)(ftw >> 24U);
  frequency_bytes[1] = (uint8_t)(ftw >> 16U);
  frequency_bytes[2] = (uint8_t)(ftw >> 8U);
  frequency_bytes[3] = (uint8_t)ftw;
  status = AD9959_WriteRegister(AD9959_REG_CFTW0,
                               frequency_bytes,
                               sizeof(frequency_bytes),
                               0U);
  if (status != AD9959_OK)
  {
    return status;
  }

  pow = AD9959_PhaseToPOW(config->phase_mdeg);
  phase_bytes[0] = (uint8_t)(pow >> 8U);
  phase_bytes[1] = (uint8_t)pow;
  status = AD9959_WriteRegister(AD9959_REG_CPOW0,
                               phase_bytes,
                               sizeof(phase_bytes),
                               0U);
  if (status != AD9959_OK)
  {
    return status;
  }

  amplitude_bytes[0] = 0x00U;
  amplitude_bytes[1] =
      (uint8_t)(0x10U | ((config->amplitude >> 8U) & 0x03U));
  amplitude_bytes[2] = (uint8_t)config->amplitude;
  return AD9959_WriteRegister(AD9959_REG_ACR,
                              amplitude_bytes,
                              sizeof(amplitude_bytes),
                              0U);
}

uint32_t AD9959_FrequencyToFTW(uint32_t frequency_hz)
{
  /* The supplied example truncates instead of rounding. */
  return (uint32_t)(((uint64_t)frequency_hz << 32U) /
                    (uint64_t)AD9959_SYSTEM_CLOCK_HZ);
}

uint16_t AD9959_PhaseToPOW(uint32_t phase_mdeg)
{
  phase_mdeg %= AD9959_FULL_SCALE_PHASE_MDEG;
  return (uint16_t)(((uint64_t)phase_mdeg * 16384ULL) /
                    AD9959_FULL_SCALE_PHASE_MDEG) & 0x3FFFU;
}

void AD9959_IOUpdate(void)
{
  HAL_GPIO_WritePin(AD9959_IO_UPDATE_GPIO_Port,
                    AD9959_IO_UPDATE_Pin,
                    GPIO_PIN_RESET);
  AD9959_UpdateDelay();
  HAL_GPIO_WritePin(AD9959_IO_UPDATE_GPIO_Port,
                    AD9959_IO_UPDATE_Pin,
                    GPIO_PIN_SET);
  AD9959_UpdateDelay();
  HAL_GPIO_WritePin(AD9959_IO_UPDATE_GPIO_Port,
                    AD9959_IO_UPDATE_Pin,
                    GPIO_PIN_RESET);
}

AD9959_Status AD9959_Init(SPI_HandleTypeDef *hspi)
{
  static const uint8_t fr1_500mhz[3] = {0xD0U, 0x00U, 0x00U};
  static const uint8_t fr2_data[2] = {0x20U, 0x00U};
  GPIO_InitTypeDef gpio = {0};
  AD9959_Status status;

  if (hspi == NULL)
  {
    return AD9959_ERROR_ARGUMENT;
  }

  /* The known-good example bit-bangs SCLK and SDIO0 as 2 MHz GPIOs. */
  if (HAL_SPI_DeInit(hspi) != HAL_OK)
  {
    return AD9959_ERROR_HAL;
  }

  gpio.Pin = AD9959_SCLK_Pin | AD9959_SDIO0_Pin | AD9959_SDIO2_Pin;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &gpio);

  HAL_GPIO_WritePin(SPI_FLASH_CS_GPIO_Port, SPI_FLASH_CS_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(AD9959_PWR_DWN_GPIO_Port,
                    AD9959_PWR_DWN_Pin,
                    GPIO_PIN_RESET);
  HAL_GPIO_WritePin(AD9959_CS_GPIO_Port, AD9959_CS_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(AD9959_SCLK_GPIO_Port,
                    AD9959_SCLK_Pin,
                    GPIO_PIN_RESET);
  HAL_GPIO_WritePin(AD9959_SDIO0_GPIO_Port,
                    AD9959_SDIO0_Pin,
                    GPIO_PIN_RESET);
  HAL_GPIO_WritePin(AD9959_SDIO2_GPIO_Port,
                    AD9959_SDIO2_Pin,
                    GPIO_PIN_RESET);
  HAL_GPIO_WritePin(AD9959_IO_UPDATE_GPIO_Port,
                    AD9959_IO_UPDATE_Pin,
                    GPIO_PIN_RESET);

  HAL_GPIO_WritePin(AD9959_RESET_GPIO_Port, AD9959_RESET_Pin, GPIO_PIN_RESET);
  HAL_Delay(1U);
  HAL_GPIO_WritePin(AD9959_RESET_GPIO_Port, AD9959_RESET_Pin, GPIO_PIN_SET);
  HAL_Delay(1U);
  HAL_GPIO_WritePin(AD9959_RESET_GPIO_Port, AD9959_RESET_Pin, GPIO_PIN_RESET);
  HAL_Delay(1U);

  s_ad9959_initialized = 1U;

  status = AD9959_WriteRegister(AD9959_REG_FR1,
                               fr1_500mhz,
                               sizeof(fr1_500mhz),
                               1U);
  if (status != AD9959_OK)
  {
    return status;
  }

  return AD9959_WriteRegister(AD9959_REG_FR2,
                              fr2_data,
                              sizeof(fr2_data),
                              1U);
}

AD9959_Status AD9959_SetFrequency(AD9959_Channel channel,
                                 uint32_t frequency_hz)
{
  AD9959_Status status;
  uint32_t ftw;
  uint8_t bytes[4];

  if (frequency_hz > AD9959_MAX_FREQUENCY_HZ)
  {
    return AD9959_ERROR_ARGUMENT;
  }

  status = AD9959_SelectChannel(channel);
  if (status != AD9959_OK)
  {
    return status;
  }

  ftw = AD9959_FrequencyToFTW(frequency_hz);
  bytes[0] = (uint8_t)(ftw >> 24U);
  bytes[1] = (uint8_t)(ftw >> 16U);
  bytes[2] = (uint8_t)(ftw >> 8U);
  bytes[3] = (uint8_t)ftw;
  return AD9959_WriteRegister(AD9959_REG_CFTW0, bytes, sizeof(bytes), 1U);
}

AD9959_Status AD9959_SetAmplitude(AD9959_Channel channel,
                                 uint16_t amplitude)
{
  AD9959_Status status;
  uint8_t bytes[3];

  if (amplitude > AD9959_MAX_AMPLITUDE)
  {
    return AD9959_ERROR_ARGUMENT;
  }

  status = AD9959_SelectChannel(channel);
  if (status != AD9959_OK)
  {
    return status;
  }

  bytes[0] = 0x00U;
  bytes[1] = (uint8_t)(0x10U | ((amplitude >> 8U) & 0x03U));
  bytes[2] = (uint8_t)amplitude;
  return AD9959_WriteRegister(AD9959_REG_ACR, bytes, sizeof(bytes), 1U);
}

AD9959_Status AD9959_SetPhase(AD9959_Channel channel,
                             uint32_t phase_mdeg)
{
  AD9959_Status status;
  uint16_t pow;
  uint8_t bytes[2];

  status = AD9959_SelectChannel(channel);
  if (status != AD9959_OK)
  {
    return status;
  }

  pow = AD9959_PhaseToPOW(phase_mdeg);
  bytes[0] = (uint8_t)(pow >> 8U);
  bytes[1] = (uint8_t)pow;
  return AD9959_WriteRegister(AD9959_REG_CPOW0, bytes, sizeof(bytes), 1U);
}

AD9959_Status AD9959_ConfigureChannel(AD9959_Channel channel,
                                     uint32_t frequency_hz,
                                     uint16_t amplitude,
                                     uint32_t phase_mdeg)
{
  AD9959_Status status;

  status = AD9959_SetFrequency(channel, frequency_hz);
  if (status == AD9959_OK)
  {
    status = AD9959_SetPhase(channel, phase_mdeg);
  }
  if (status == AD9959_OK)
  {
    status = AD9959_SetAmplitude(channel, amplitude);
  }
  return status;
}

AD9959_Status AD9959_ConfigureChannelsSynchronized(
    const AD9959_ChannelConfig *configs,
    uint32_t config_count)
{
  AD9959_Status status;
  uint32_t index;
  uint32_t compare_index;

  if ((configs == NULL) || (config_count == 0U) || (config_count > 4U))
  {
    return AD9959_ERROR_ARGUMENT;
  }

  /* Reject the whole request before touching a channel buffer. */
  for (index = 0U; index < config_count; ++index)
  {
    status = AD9959_ValidateChannelConfig(&configs[index]);
    if (status != AD9959_OK)
    {
      return status;
    }

    for (compare_index = 0U; compare_index < index; ++compare_index)
    {
      if (configs[index].channel == configs[compare_index].channel)
      {
        return AD9959_ERROR_ARGUMENT;
      }
    }
  }

  for (index = 0U; index < config_count; ++index)
  {
    status = AD9959_StageChannelConfig(&configs[index]);
    if (status != AD9959_OK)
    {
      return status;
    }
  }

  /* One rising edge transfers every staged channel register together. */
  HAL_GPIO_WritePin(AD9959_CS_GPIO_Port, AD9959_CS_Pin, GPIO_PIN_RESET);
  AD9959_IOUpdate();
  HAL_GPIO_WritePin(AD9959_CS_GPIO_Port, AD9959_CS_Pin, GPIO_PIN_SET);
  return AD9959_OK;
}
