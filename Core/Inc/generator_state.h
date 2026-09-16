#ifndef GENERATOR_STATE_H
#define GENERATOR_STATE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#define GENERATOR_CHANNEL_COUNT 4U

typedef enum
{
  WAVE_SINE = 0,
  WAVE_SQUARE,
  WAVE_TRIANGLE,
  WAVE_PULSE,
  WAVE_COUNT
} waveform_t;

typedef struct
{
  double frequency_hz;
  float amplitude_vpp;
  float offset_v;
  float phase_deg;
  waveform_t waveform;
  bool output_enable;
} channel_state_t;

typedef struct
{
  channel_state_t ch[GENERATOR_CHANNEL_COUNT];
  uint8_t active_channel;
  bool touch_locked;
  double freq_step_hz;
  float amp_step_v;
  float offset_step_v;
  float phase_step_deg;
} generator_state_t;

void Generator_State_Init(generator_state_t *state);
channel_state_t *Generator_State_GetActive(generator_state_t *state);
const channel_state_t *Generator_State_GetActiveConst(
    const generator_state_t *state);

#ifdef __cplusplus
}
#endif

#endif /* GENERATOR_STATE_H */
