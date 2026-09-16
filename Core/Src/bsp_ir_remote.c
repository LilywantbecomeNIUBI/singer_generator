#include "bsp_ir_remote.h"

#include "main.h"
#include "tim.h"

#include <stddef.h>

#define BSP_IR_TIM_CHANNEL TIM_CHANNEL_1
#define BSP_IR_INPUT_PORT GPIOA
#define BSP_IR_INPUT_PIN GPIO_PIN_8

#define BSP_IR_ZERO_MIN_US 300U
#define BSP_IR_ZERO_MAX_US 800U
#define BSP_IR_ONE_MIN_US 1400U
#define BSP_IR_ONE_MAX_US 1900U
#define BSP_IR_REPEAT_MIN_US 2000U
#define BSP_IR_REPEAT_MAX_US 3000U
#define BSP_IR_LEADER_MIN_US 4200U
#define BSP_IR_LEADER_MAX_US 4700U

static volatile uint32_t s_frame_data;
static volatile uint8_t s_frame_bits;
static volatile uint8_t s_leader_seen;
static volatile uint8_t s_last_key;
static volatile uint8_t s_last_key_valid;
static volatile uint8_t s_event_pending;
static volatile uint8_t s_event_key;
static volatile uint8_t s_event_repeat;
static uint32_t s_timer_tick_hz;

static uint32_t BSP_IR_GetTimerClockHz(void)
{
  RCC_ClkInitTypeDef clocks;
  uint32_t flash_latency;
  uint32_t timer_clock_hz;

  HAL_RCC_GetClockConfig(&clocks, &flash_latency);
  timer_clock_hz = HAL_RCC_GetPCLK2Freq();
  if (clocks.APB2CLKDivider != RCC_HCLK_DIV1)
  {
    timer_clock_hz *= 2U;
  }
  return timer_clock_hz;
}

static uint32_t BSP_IR_TicksToUs(uint32_t ticks)
{
  if (s_timer_tick_hz == 0U)
  {
    return 0U;
  }
  return (uint32_t)(((uint64_t)ticks * 1000000ULL +
                     (uint64_t)(s_timer_tick_hz / 2U)) /
                    (uint64_t)s_timer_tick_hz);
}

static uint8_t BSP_IR_InRange(uint32_t value,
                              uint32_t minimum,
                              uint32_t maximum)
{
  return ((value > minimum) && (value < maximum)) ? 1U : 0U;
}

static void BSP_IR_ResetFrame(void)
{
  s_frame_data = 0U;
  s_frame_bits = 0U;
  s_leader_seen = 0U;
}

static void BSP_IR_PostEventFromISR(uint8_t key, uint8_t is_repeat)
{
  if (s_event_pending == 0U)
  {
    s_event_key = key;
    s_event_repeat = is_repeat;
    s_event_pending = 1U;
  }
}

static void BSP_IR_FinishFrame(void)
{
  uint8_t address = (uint8_t)s_frame_data;
  uint8_t address_inverse = (uint8_t)(s_frame_data >> 8);
  uint8_t command = (uint8_t)(s_frame_data >> 16);
  uint8_t command_inverse = (uint8_t)(s_frame_data >> 24);

  if (((uint8_t)(address ^ address_inverse) == 0xFFU) &&
      (address == BSP_IR_REMOTE_ADDRESS) &&
      ((uint8_t)(command ^ command_inverse) == 0xFFU))
  {
    s_last_key = command;
    s_last_key_valid = 1U;
    BSP_IR_PostEventFromISR(command, 0U);
  }
  BSP_IR_ResetFrame();
}

uint8_t BSP_IR_Remote_Init(void)
{
  s_last_key = 0U;
  s_last_key_valid = 0U;
  s_event_pending = 0U;
  s_event_key = 0U;
  s_event_repeat = 0U;
  BSP_IR_ResetFrame();

  s_timer_tick_hz =
      BSP_IR_GetTimerClockHz() / (uint32_t)(htim1.Init.Prescaler + 1U);
  __HAL_TIM_SET_COUNTER(&htim1, 0U);
  __HAL_TIM_SET_CAPTUREPOLARITY(&htim1,
                                BSP_IR_TIM_CHANNEL,
                                TIM_INPUTCHANNELPOLARITY_RISING);
  return (HAL_TIM_IC_Start_IT(&htim1, BSP_IR_TIM_CHANNEL) == HAL_OK) ? 1U
                                                                    : 0U;
}

uint8_t BSP_IR_Remote_TakeEvent(BSP_IR_RemoteEvent *event)
{
  uint32_t primask;

  if (event == NULL)
  {
    return 0U;
  }

  primask = __get_PRIMASK();
  __disable_irq();
  if (s_event_pending == 0U)
  {
    if (primask == 0U)
    {
      __enable_irq();
    }
    return 0U;
  }

  event->key = s_event_key;
  event->is_repeat = s_event_repeat;
  s_event_pending = 0U;
  if (primask == 0U)
  {
    __enable_irq();
  }
  return 1U;
}

void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
  uint32_t pulse_us;

  if ((htim != &htim1) || (htim->Channel != HAL_TIM_ACTIVE_CHANNEL_1))
  {
    return;
  }

  if (HAL_GPIO_ReadPin(BSP_IR_INPUT_PORT, BSP_IR_INPUT_PIN) == GPIO_PIN_SET)
  {
    __HAL_TIM_SET_CAPTUREPOLARITY(htim,
                                  BSP_IR_TIM_CHANNEL,
                                  TIM_INPUTCHANNELPOLARITY_FALLING);
    __HAL_TIM_SET_COUNTER(htim, 0U);
    return;
  }

  pulse_us = BSP_IR_TicksToUs(
      HAL_TIM_ReadCapturedValue(htim, BSP_IR_TIM_CHANNEL));
  __HAL_TIM_SET_CAPTUREPOLARITY(htim,
                                BSP_IR_TIM_CHANNEL,
                                TIM_INPUTCHANNELPOLARITY_RISING);

  if (BSP_IR_InRange(pulse_us,
                     BSP_IR_LEADER_MIN_US,
                     BSP_IR_LEADER_MAX_US) != 0U)
  {
    s_frame_data = 0U;
    s_frame_bits = 0U;
    s_leader_seen = 1U;
    return;
  }

  if ((s_last_key_valid != 0U) &&
      (BSP_IR_InRange(pulse_us,
                      BSP_IR_REPEAT_MIN_US,
                      BSP_IR_REPEAT_MAX_US) != 0U))
  {
    BSP_IR_PostEventFromISR(s_last_key, 1U);
    BSP_IR_ResetFrame();
    return;
  }

  if (s_leader_seen == 0U)
  {
    return;
  }

  s_frame_data >>= 1;
  if (BSP_IR_InRange(pulse_us, BSP_IR_ONE_MIN_US, BSP_IR_ONE_MAX_US) != 0U)
  {
    s_frame_data |= 0x80000000UL;
  }
  else if (BSP_IR_InRange(pulse_us,
                          BSP_IR_ZERO_MIN_US,
                          BSP_IR_ZERO_MAX_US) == 0U)
  {
    BSP_IR_ResetFrame();
    return;
  }

  ++s_frame_bits;
  if (s_frame_bits >= 32U)
  {
    BSP_IR_FinishFrame();
  }
}
