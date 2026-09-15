#include "bsp_ad9959.h"

#include "main.h"

#include <stddef.h>

#define AD9959_REG_CSR 0x00U
#define AD9959_REG_FR1 0x01U
#define AD9959_REG_FR2 0x02U
#define AD9959_REG_CFTW0 0x04U
#define AD9959_REG_CPOW0 0x05U
#define AD9959_REG_ACR 0x06U

#define AD9959_SPI_TIMEOUT_MS 10U

static SPI_HandleTypeDef *s_ad9959_spi;

static uint8_t AD9959_IsValidChannel(AD9959_Channel channel)
{
  return ((channel == AD9959_CHANNEL_0) ||
          (channel == AD9959_CHANNEL_1) ||
          (channel == AD9959_CHANNEL_2) ||
          (channel == AD9959_CHANNEL_3));
}

static void AD9959_ShortDelay(void)
{
  __NOP();
  __NOP();
  __NOP();
  __NOP();
}

static AD9959_Status AD9959_WriteRegister(uint8_t address,
                                          const uint8_t *data,
                                          uint16_t length,
                                          uint8_t apply_update)
{
  HAL_StatusTypeDef hal_status;
  uint8_t instruction;

  if (s_ad9959_spi == NULL)
  {
    return AD9959_ERROR_NOT_INITIALIZED;
  }
  if ((data == NULL) || (length == 0U) || (address > 0x1FU))
  {
    return AD9959_ERROR_ARGUMENT;
  }

  instruction = address & 0x1FU;

  /* SPI1 is shared with the Explorer board SPI flash. */
  HAL_GPIO_WritePin(SPI_FLASH_CS_GPIO_Port, SPI_FLASH_CS_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(AD9959_CS_GPIO_Port, AD9959_CS_Pin, GPIO_PIN_RESET);

  hal_status = HAL_SPI_Transmit(s_ad9959_spi,
                               &instruction,
                               1U,
                               AD9959_SPI_TIMEOUT_MS);
  if (hal_status == HAL_OK)
  {
    hal_status = HAL_SPI_Transmit(s_ad9959_spi,
                                 (uint8_t *)data,
                                 length,
                                 AD9959_SPI_TIMEOUT_MS);
  }

  HAL_GPIO_WritePin(AD9959_CS_GPIO_Port, AD9959_CS_Pin, GPIO_PIN_SET);

  if (hal_status != HAL_OK)
  {
    return AD9959_ERROR_SPI;
  }

  if (apply_update != 0U)
  {
    AD9959_IOUpdate();
  }

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
  return AD9959_WriteRegister(AD9959_REG_CSR, &csr, 1U, 0U);
}

uint32_t AD9959_FrequencyToFTW(uint32_t frequency_hz)
{
  uint64_t numerator;

  numerator = ((uint64_t)frequency_hz << 32U) +
              ((uint64_t)AD9959_SYSTEM_CLOCK_HZ / 2ULL);
  return (uint32_t)(numerator / (uint64_t)AD9959_SYSTEM_CLOCK_HZ);
}

uint16_t AD9959_PhaseToPOW(uint32_t phase_mdeg)
{
  uint64_t numerator;

  phase_mdeg %= AD9959_FULL_SCALE_PHASE_MDEG;
  numerator = ((uint64_t)phase_mdeg * 16384ULL) +
              ((uint64_t)AD9959_FULL_SCALE_PHASE_MDEG / 2ULL);
  return (uint16_t)(numerator / AD9959_FULL_SCALE_PHASE_MDEG) & 0x3FFFU;
}

void AD9959_IOUpdate(void)
{
  HAL_GPIO_WritePin(AD9959_IO_UPDATE_GPIO_Port,
                    AD9959_IO_UPDATE_Pin,
                    GPIO_PIN_RESET);
  AD9959_ShortDelay();
  HAL_GPIO_WritePin(AD9959_IO_UPDATE_GPIO_Port,
                    AD9959_IO_UPDATE_Pin,
                    GPIO_PIN_SET);
  AD9959_ShortDelay();
  HAL_GPIO_WritePin(AD9959_IO_UPDATE_GPIO_Port,
                    AD9959_IO_UPDATE_Pin,
                    GPIO_PIN_RESET);
}

AD9959_Status AD9959_Init(SPI_HandleTypeDef *hspi)
{
  static const uint8_t fr1_500mhz[3] = {0xD0U, 0x00U, 0x00U};
  static const uint8_t fr2_default[2] = {0x20U, 0x00U};
  AD9959_Status status;

  if (hspi == NULL)
  {
    return AD9959_ERROR_ARGUMENT;
  }

  s_ad9959_spi = hspi;

  HAL_GPIO_WritePin(SPI_FLASH_CS_GPIO_Port, SPI_FLASH_CS_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(AD9959_CS_GPIO_Port, AD9959_CS_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(AD9959_PWR_DWN_GPIO_Port,
                    AD9959_PWR_DWN_Pin,
                    GPIO_PIN_RESET);
  HAL_GPIO_WritePin(AD9959_IO_UPDATE_GPIO_Port,
                    AD9959_IO_UPDATE_Pin,
                    GPIO_PIN_RESET);

  HAL_GPIO_WritePin(AD9959_RESET_GPIO_Port, AD9959_RESET_Pin, GPIO_PIN_RESET);
  HAL_Delay(1U);
  HAL_GPIO_WritePin(AD9959_RESET_GPIO_Port, AD9959_RESET_Pin, GPIO_PIN_SET);
  HAL_Delay(1U);
  HAL_GPIO_WritePin(AD9959_RESET_GPIO_Port, AD9959_RESET_Pin, GPIO_PIN_RESET);
  HAL_Delay(5U);

  status = AD9959_WriteRegister(AD9959_REG_FR1,
                               fr1_500mhz,
                               sizeof(fr1_500mhz),
                               0U);
  if (status != AD9959_OK)
  {
    return status;
  }

  status = AD9959_WriteRegister(AD9959_REG_FR2,
                               fr2_default,
                               sizeof(fr2_default),
                               0U);
  if (status != AD9959_OK)
  {
    return status;
  }

  AD9959_IOUpdate();
  HAL_Delay(10U);
  return AD9959_OK;
}

AD9959_Status AD9959_SetFrequency(AD9959_Channel channel,
                                 uint32_t frequency_hz,
                                 uint8_t apply_update)
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

  return AD9959_WriteRegister(AD9959_REG_CFTW0,
                              bytes,
                              sizeof(bytes),
                              apply_update);
}

AD9959_Status AD9959_SetAmplitude(AD9959_Channel channel,
                                 uint16_t amplitude,
                                 uint8_t apply_update)
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

  return AD9959_WriteRegister(AD9959_REG_ACR,
                              bytes,
                              sizeof(bytes),
                              apply_update);
}

AD9959_Status AD9959_SetPhase(AD9959_Channel channel,
                             uint32_t phase_mdeg,
                             uint8_t apply_update)
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

  return AD9959_WriteRegister(AD9959_REG_CPOW0,
                              bytes,
                              sizeof(bytes),
                              apply_update);
}

AD9959_Status AD9959_ConfigureChannel(AD9959_Channel channel,
                                     uint32_t frequency_hz,
                                     uint16_t amplitude,
                                     uint32_t phase_mdeg)
{
  AD9959_Status status;

  status = AD9959_SetFrequency(channel, frequency_hz, 0U);
  if (status != AD9959_OK)
  {
    return status;
  }

  status = AD9959_SetPhase(channel, phase_mdeg, 0U);
  if (status != AD9959_OK)
  {
    return status;
  }

  status = AD9959_SetAmplitude(channel, amplitude, 0U);
  if (status != AD9959_OK)
  {
    return status;
  }

  AD9959_IOUpdate();
  return AD9959_OK;
}
