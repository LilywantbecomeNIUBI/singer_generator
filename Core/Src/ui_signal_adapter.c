#include "ui_signal_adapter.h"

#include "app_signal_generator.h"
#include "generator_state.h"
#include "ui_signal_generator.h"

#include <stddef.h>

#define UI_ADAPTER_CHANNEL_COUNT 2U
#define UI_ADAPTER_NOMINAL_FULL_SCALE_VPP 1.0F

static App_SignalChannelConfig s_last_applied[UI_ADAPTER_CHANNEL_COUNT];
static uint8_t s_have_last[UI_ADAPTER_CHANNEL_COUNT];
static uint8_t s_last_apply_ok;

static uint8_t UI_Adapter_FrequencyToHz(double frequency_hz,
                                        uint32_t *frequency_out_hz)
{
  if ((frequency_out_hz == NULL) ||
      (frequency_hz < (double)APP_SIGNAL_GENERATOR_MIN_FREQUENCY_HZ) ||
      (frequency_hz > (double)APP_SIGNAL_GENERATOR_MAX_FREQUENCY_HZ))
  {
    return 0U;
  }

  *frequency_out_hz = (uint32_t)(frequency_hz + 0.5);
  return 1U;
}

static uint16_t UI_Adapter_AmplitudeToASF(float amplitude_vpp,
                                         uint8_t output_enable)
{
  float normalized;
  uint32_t asf;

  if (output_enable == 0U)
  {
    return 0U;
  }

  if (amplitude_vpp <= 0.0F)
  {
    return 0U;
  }

  normalized = amplitude_vpp / UI_ADAPTER_NOMINAL_FULL_SCALE_VPP;
  if (normalized > 1.0F)
  {
    normalized = 1.0F;
  }

  asf = (uint32_t)(normalized * (float)APP_SIGNAL_GENERATOR_MAX_AMPLITUDE + 0.5F);
  if (asf > APP_SIGNAL_GENERATOR_MAX_AMPLITUDE)
  {
    asf = APP_SIGNAL_GENERATOR_MAX_AMPLITUDE;
  }
  return (uint16_t)asf;
}

static uint32_t UI_Adapter_PhaseToMilliDegrees(float phase_deg)
{
  float wrapped = phase_deg;
  uint32_t phase_mdeg;

  while (wrapped >= 360.0F)
  {
    wrapped -= 360.0F;
  }
  while (wrapped < 0.0F)
  {
    wrapped += 360.0F;
  }

  phase_mdeg = (uint32_t)(wrapped * 1000.0F + 0.5F);
  if (phase_mdeg >= APP_SIGNAL_GENERATOR_FULL_SCALE_PHASE_MDEG)
  {
    phase_mdeg = 0U;
  }
  return phase_mdeg;
}

static uint8_t UI_Adapter_BuildChannel(const channel_state_t *ui,
                                       App_SignalChannelConfig *out)
{
  if ((ui == NULL) || (out == NULL))
  {
    return 0U;
  }

  /* First integration milestone only closes the loop for sine-wave CH1/CH2. */
  if (ui->waveform != WAVE_SINE)
  {
    return 0U;
  }

  if (UI_Adapter_FrequencyToHz(ui->frequency_hz,
                               &out->frequency_hz) == 0U)
  {
    return 0U;
  }

  out->amplitude = UI_Adapter_AmplitudeToASF(ui->amplitude_vpp,
                                             ui->output_enable ? 1U : 0U);
  out->phase_mdeg = UI_Adapter_PhaseToMilliDegrees(ui->phase_deg);
  return 1U;
}

static uint8_t UI_Adapter_ChannelEquals(
    const App_SignalChannelConfig *left,
    const App_SignalChannelConfig *right)
{
  return ((left != NULL) &&
          (right != NULL) &&
          (left->frequency_hz == right->frequency_hz) &&
          (left->amplitude == right->amplitude) &&
          (left->phase_mdeg == right->phase_mdeg));
}

void UI_SignalAdapter_Init(void)
{
  const generator_state_t *state = UI_SignalGenerator_GetState();
  uint32_t channel;

  s_last_apply_ok = 1U;
  for (channel = 0U; channel < UI_ADAPTER_CHANNEL_COUNT; ++channel)
  {
    s_have_last[channel] = 0U;
    if ((state == NULL) ||
        (UI_Adapter_BuildChannel(&state->ch[channel],
                                 &s_last_applied[channel]) == 0U))
    {
      s_last_apply_ok = 0U;
      continue;
    }

    /* App_SignalGenerator_Init already established this boot baseline. */
    s_have_last[channel] = 1U;
  }
}

void UI_SignalAdapter_Process(void)
{
  const generator_state_t *state = UI_SignalGenerator_GetState();
  App_SignalChannelConfig next[UI_ADAPTER_CHANNEL_COUNT];
  uint32_t channel;

  if (state == NULL)
  {
    s_last_apply_ok = 0U;
    return;
  }

  s_last_apply_ok = 1U;
  for (channel = 0U; channel < UI_ADAPTER_CHANNEL_COUNT; ++channel)
  {
    if (UI_Adapter_BuildChannel(&state->ch[channel], &next[channel]) == 0U)
    {
      s_last_apply_ok = 0U;
      return;
    }

    if ((s_have_last[channel] != 0U) &&
        (UI_Adapter_ChannelEquals(&next[channel],
                                  &s_last_applied[channel]) != 0U))
    {
      continue;
    }

    if (App_SignalGenerator_ApplyChannel(channel, &next[channel]) == 0U)
    {
      s_last_apply_ok = 0U;
      return;
    }

    s_last_applied[channel] = next[channel];
    s_have_last[channel] = 1U;
  }
}

uint8_t UI_SignalAdapter_GetLastApplyOk(void)
{
  return s_last_apply_ok;
}
