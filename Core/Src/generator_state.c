#include "generator_state.h"

#include <stddef.h>

void Generator_State_Init(generator_state_t *state)
{
  uint8_t channel;

  if (state == NULL)
  {
    return;
  }

  for (channel = 0U; channel < GENERATOR_CHANNEL_COUNT; ++channel)
  {
    state->ch[channel].frequency_hz = 1000000.0;
    state->ch[channel].amplitude_vpp = 1.0F;
    state->ch[channel].offset_v = 0.0F;
    state->ch[channel].phase_deg = (float)channel * 90.0F;
    state->ch[channel].waveform = WAVE_SINE;
    state->ch[channel].output_enable = false;
  }

  state->active_channel = 0U;
  state->touch_locked = false;
  state->freq_step_hz = 1000.0;
  state->amp_step_v = 0.01F;
  state->offset_step_v = 0.01F;
  state->phase_step_deg = 1.0F;
}

channel_state_t *Generator_State_GetActive(generator_state_t *state)
{
  if ((state == NULL) ||
      (state->active_channel >= GENERATOR_CHANNEL_COUNT))
  {
    return NULL;
  }
  return &state->ch[state->active_channel];
}

const channel_state_t *Generator_State_GetActiveConst(
    const generator_state_t *state)
{
  if ((state == NULL) ||
      (state->active_channel >= GENERATOR_CHANNEL_COUNT))
  {
    return NULL;
  }
  return &state->ch[state->active_channel];
}
