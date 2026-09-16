#include "ui_signal_generator.h"

#include "lv_port_input.h"
#include "lvgl.h"
#include "main.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define UI_COLOR_BACKGROUND 0x0D1117U
#define UI_COLOR_PANEL 0x171D24U
#define UI_COLOR_PANEL_SELECTED 0x29323CU
#define UI_COLOR_TEXT 0xF2F5F7U
#define UI_COLOR_TEXT_MUTED 0x98A2ADU
#define UI_COLOR_BORDER 0x3A4652U
#define UI_COLOR_WARNING 0xFF5B5BU
#define UI_TOAST_DURATION_MS 800U
#define UI_PREVIEW_POINT_CAPACITY 33U
#define UI_PREVIEW_WAVE_WIDTH 225
#define UI_PREVIEW_WAVE_CENTER_Y 12

#define UI_FREQ_MIN_HZ 1.0
#define UI_FREQ_MAX_HZ 100000000.0
#define UI_AMPLITUDE_MIN_VPP 0.0F
#define UI_AMPLITUDE_MAX_VPP 2.0F
#define UI_OFFSET_MIN_V -1.0F
#define UI_OFFSET_MAX_V 1.0F

typedef enum
{
  UI_MODE_NAV = 0,
  UI_MODE_EDIT,
  UI_MODE_POPUP,
  UI_MODE_NUM_INPUT,
  UI_MODE_LOCKED
} UI_Mode;

typedef enum
{
  UI_PAGE_MAIN = 0,
  UI_PAGE_EDIT,
  UI_PAGE_WAVEFORM,
  UI_PAGE_CHANNEL_OVERVIEW,
  UI_PAGE_FUNCTION_MENU,
  UI_PAGE_STEP_SELECT
} UI_Page;

typedef enum
{
  UI_PARAM_FREQUENCY = 0,
  UI_PARAM_AMPLITUDE,
  UI_PARAM_OFFSET,
  UI_PARAM_PHASE,
  UI_PARAM_COUNT
} UI_Parameter;

static const uint32_t s_channel_colors[GENERATOR_CHANNEL_COUNT] = {
    0xFFD447U, 0x41D9E8U, 0xF062D7U, 0x62D26FU};
static const char *const s_wave_names[WAVE_COUNT] = {
    "SINE", "SQUARE", "TRI", "PULSE"};
static const char *const s_parameter_names[UI_PARAM_COUNT] = {
    "FREQ", "AMPL", "OFFSET", "PHASE"};
static const double s_frequency_steps[] = {
    1.0, 10.0, 100.0, 1000.0, 10000.0, 100000.0, 1000000.0};
static const float s_amplitude_steps[] = {0.001F, 0.01F, 0.1F};
static const float s_phase_steps[] = {0.1F, 1.0F, 10.0F};

static generator_state_t s_state;
static channel_state_t s_edit_backup;
static UI_Mode s_mode;
static UI_Page s_page;
static UI_Parameter s_parameter;
static uint8_t s_step_selection;
static lv_obj_t *s_toast;
static uint32_t s_toast_until_ms;
static lv_point_precise_t s_preview_points[UI_PREVIEW_POINT_CAPACITY];

static lv_color_t UI_ChannelColor(uint8_t channel)
{
  return lv_color_hex(s_channel_colors[channel % GENERATOR_CHANNEL_COUNT]);
}

static void UI_SetObjectBox(lv_obj_t *object,
                            lv_color_t background,
                            lv_color_t border,
                            int32_t radius)
{
  lv_obj_set_style_bg_color(object, background, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(object, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_color(object, border, LV_PART_MAIN);
  lv_obj_set_style_border_width(object, 1, LV_PART_MAIN);
  lv_obj_set_style_radius(object, radius, LV_PART_MAIN);
  lv_obj_set_style_pad_all(object, 0, LV_PART_MAIN);
}

static void UI_ClearScreen(void)
{
  lv_obj_t *screen = lv_screen_active();

  s_toast = NULL;
  lv_obj_clean(screen);
  lv_obj_set_style_bg_color(screen,
                            lv_color_hex(UI_COLOR_BACKGROUND),
                            LV_PART_MAIN);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
}

static lv_obj_t *UI_CreateLabel(lv_obj_t *parent,
                                const char *text,
                                lv_color_t color)
{
  lv_obj_t *label = lv_label_create(parent);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
  return label;
}

static void UI_CommandEvent(lv_event_t *event)
{
  UI_Command command = (UI_Command)(uintptr_t)lv_event_get_user_data(event);
  (void)UI_CommandQueue_Post(command);
}

static lv_obj_t *UI_CreateButton(lv_obj_t *parent,
                                 const char *text,
                                 int32_t x,
                                 int32_t y,
                                 int32_t width,
                                 int32_t height,
                                 UI_Command command,
                                 uint8_t enabled)
{
  lv_obj_t *button = lv_button_create(parent);
  lv_obj_t *label;

  lv_obj_set_pos(button, x, y);
  lv_obj_set_size(button, width, height);
  UI_SetObjectBox(button,
                  lv_color_hex(UI_COLOR_PANEL),
                  lv_color_hex(UI_COLOR_BORDER),
                  3);
  label = UI_CreateLabel(button,
                         text,
                         lv_color_hex((enabled != 0U) ? UI_COLOR_TEXT
                                                     : UI_COLOR_TEXT_MUTED));
  lv_obj_center(label);

  if (enabled != 0U)
  {
    lv_obj_add_event_cb(button,
                        UI_CommandEvent,
                        LV_EVENT_CLICKED,
                        (void *)(uintptr_t)command);
  }
  else
  {
    lv_obj_add_state(button, LV_STATE_DISABLED);
  }
  return button;
}

static void UI_FormatFrequency(double frequency_hz,
                               char *buffer,
                               size_t buffer_size)
{
  uint32_t frequency = (uint32_t)(frequency_hz + 0.5);

  if (frequency >= 1000000U)
  {
    uint32_t whole = frequency / 1000000U;
    uint32_t fraction = frequency % 1000000U;
    (void)snprintf(buffer,
                   buffer_size,
                   "%lu.%03lu %03lu MHz",
                   (unsigned long)whole,
                   (unsigned long)(fraction / 1000U),
                   (unsigned long)(fraction % 1000U));
  }
  else if (frequency >= 1000U)
  {
    (void)snprintf(buffer,
                   buffer_size,
                   "%lu.%03lu kHz",
                   (unsigned long)(frequency / 1000U),
                   (unsigned long)(frequency % 1000U));
  }
  else
  {
    (void)snprintf(buffer,
                   buffer_size,
                   "%lu Hz",
                   (unsigned long)frequency);
  }
}

static void UI_FormatAmplitude(float amplitude_vpp,
                               char *buffer,
                               size_t buffer_size)
{
  uint32_t millivolts = (uint32_t)(amplitude_vpp * 1000.0F + 0.5F);
  (void)snprintf(buffer,
                 buffer_size,
                 "%lu.%03lu Vpp",
                 (unsigned long)(millivolts / 1000U),
                 (unsigned long)(millivolts % 1000U));
}

static void UI_FormatOffset(float offset_v,
                            char *buffer,
                            size_t buffer_size)
{
  int32_t millivolts = (int32_t)(offset_v * 1000.0F);
  uint32_t magnitude = (uint32_t)((millivolts < 0) ? -millivolts
                                                    : millivolts);
  (void)snprintf(buffer,
                 buffer_size,
                 "%s%lu.%03lu V",
                 (millivolts < 0) ? "-" : "",
                 (unsigned long)(magnitude / 1000U),
                 (unsigned long)(magnitude % 1000U));
}

static void UI_FormatPhase(float phase_deg,
                           char *buffer,
                           size_t buffer_size)
{
  uint32_t tenths = (uint32_t)(phase_deg * 10.0F + 0.5F);
  (void)snprintf(buffer,
                 buffer_size,
                 "%lu.%01lu deg",
                 (unsigned long)(tenths / 10U),
                 (unsigned long)(tenths % 10U));
}

static void UI_FormatParameter(UI_Parameter parameter,
                               const channel_state_t *channel,
                               char *buffer,
                               size_t buffer_size)
{
  switch (parameter)
  {
    case UI_PARAM_FREQUENCY:
      UI_FormatFrequency(channel->frequency_hz, buffer, buffer_size);
      break;
    case UI_PARAM_AMPLITUDE:
      UI_FormatAmplitude(channel->amplitude_vpp, buffer, buffer_size);
      break;
    case UI_PARAM_OFFSET:
      UI_FormatOffset(channel->offset_v, buffer, buffer_size);
      break;
    case UI_PARAM_PHASE:
    default:
      UI_FormatPhase(channel->phase_deg, buffer, buffer_size);
      break;
  }
}

static void UI_FormatStep(UI_Parameter parameter,
                          char *buffer,
                          size_t buffer_size)
{
  if (parameter == UI_PARAM_FREQUENCY)
  {
    UI_FormatFrequency(s_state.freq_step_hz, buffer, buffer_size);
  }
  else if (parameter == UI_PARAM_AMPLITUDE)
  {
    UI_FormatAmplitude(s_state.amp_step_v, buffer, buffer_size);
  }
  else if (parameter == UI_PARAM_OFFSET)
  {
    UI_FormatOffset(s_state.offset_step_v, buffer, buffer_size);
  }
  else
  {
    UI_FormatPhase(s_state.phase_step_deg, buffer, buffer_size);
  }
}

static lv_obj_t *UI_CreateTopBar(const char *title)
{
  const channel_state_t *channel = Generator_State_GetActiveConst(&s_state);
  lv_obj_t *bar = lv_obj_create(lv_screen_active());
  lv_obj_t *label;
  char text[48];

  lv_obj_set_pos(bar, 0, 0);
  lv_obj_set_size(bar, 320, 28);
  UI_SetObjectBox(bar,
                  lv_color_hex(UI_COLOR_PANEL),
                  lv_color_hex(UI_COLOR_BORDER),
                  0);
  lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

  if (title == NULL)
  {
    (void)snprintf(text,
                   sizeof(text),
                   "CH%u",
                   (unsigned int)(s_state.active_channel + 1U));
    label = UI_CreateLabel(bar, text, UI_ChannelColor(s_state.active_channel));
    lv_obj_set_pos(label, 7, 5);

    label = UI_CreateLabel(bar,
                           s_wave_names[channel->waveform],
                           lv_color_hex(UI_COLOR_TEXT));
    lv_obj_set_pos(label, 55, 5);

    label = UI_CreateLabel(bar,
                           channel->output_enable ? "OUT ON" : "OUT OFF",
                           channel->output_enable
                               ? UI_ChannelColor(s_state.active_channel)
                               : lv_color_hex(UI_COLOR_TEXT_MUTED));
    lv_obj_set_pos(label, 130, 5);

    UI_FormatStep(s_parameter, text, sizeof(text));
    label = UI_CreateLabel(bar, text, lv_color_hex(UI_COLOR_TEXT_MUTED));
    lv_obj_align(label, LV_ALIGN_RIGHT_MID, -6, 0);
  }
  else
  {
    (void)snprintf(text,
                   sizeof(text),
                   "CH%u  %s",
                   (unsigned int)(s_state.active_channel + 1U),
                   title);
    label = UI_CreateLabel(bar, text, UI_ChannelColor(s_state.active_channel));
    lv_obj_set_pos(label, 7, 5);
  }
  return bar;
}

static void UI_ShowToast(const char *text, lv_color_t color)
{
  if (s_toast != NULL)
  {
    lv_obj_delete(s_toast);
  }

  s_toast = lv_label_create(lv_screen_active());
  lv_label_set_text(s_toast, text);
  lv_obj_set_style_text_color(s_toast, lv_color_hex(UI_COLOR_BACKGROUND),
                              LV_PART_MAIN);
  lv_obj_set_style_bg_color(s_toast, color, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(s_toast, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_pad_hor(s_toast, 12, LV_PART_MAIN);
  lv_obj_set_style_pad_ver(s_toast, 7, LV_PART_MAIN);
  lv_obj_set_style_radius(s_toast, 3, LV_PART_MAIN);
  lv_obj_align(s_toast, LV_ALIGN_TOP_MID, 0, 34);
  s_toast_until_ms = HAL_GetTick() + UI_TOAST_DURATION_MS;
}

static uint32_t UI_BuildPreviewPoints(waveform_t waveform)
{
  static const int8_t sine_offsets[16] = {
      0, -5, -9, -12, -12, -12, -9, -5,
      0, 5, 9, 12, 12, 12, 9, 5};
  static const int8_t triangle_offsets[16] = {
      0, -3, -6, -9, -12, -9, -6, -3,
      0, 3, 6, 9, 12, 9, 6, 3};
  uint32_t index;

  if ((waveform == WAVE_SINE) || (waveform == WAVE_TRIANGLE))
  {
    const int8_t *offsets = (waveform == WAVE_SINE)
                                ? sine_offsets
                                : triangle_offsets;

    for (index = 0U; index < UI_PREVIEW_POINT_CAPACITY; ++index)
    {
      s_preview_points[index].x =
          (lv_value_precise_t)((UI_PREVIEW_WAVE_WIDTH * (int32_t)index) / 32);
      s_preview_points[index].y =
          (lv_value_precise_t)(UI_PREVIEW_WAVE_CENTER_Y + offsets[index % 16U]);
    }
    return UI_PREVIEW_POINT_CAPACITY;
  }

  if (waveform == WAVE_SQUARE)
  {
    static const int16_t x[] = {0, 56, 56, 112, 112, 168, 168, 225, 225};
    static const int8_t y[] = {0, 0, 24, 24, 0, 0, 24, 24, 0};

    for (index = 0U; index < (sizeof(x) / sizeof(x[0])); ++index)
    {
      s_preview_points[index].x = (lv_value_precise_t)x[index];
      s_preview_points[index].y = (lv_value_precise_t)y[index];
    }
    return (uint32_t)(sizeof(x) / sizeof(x[0]));
  }

  {
    static const int16_t x[] = {
        0, 14, 14, 42, 42, 112, 126, 126, 154, 154, 225};
    static const int8_t y[] = {
        24, 24, 0, 0, 24, 24, 24, 0, 0, 24, 24};

    for (index = 0U; index < (sizeof(x) / sizeof(x[0])); ++index)
    {
      s_preview_points[index].x = (lv_value_precise_t)x[index];
      s_preview_points[index].y = (lv_value_precise_t)y[index];
    }
    return (uint32_t)(sizeof(x) / sizeof(x[0]));
  }
}

static void UI_CreateWaveformPreview(const channel_state_t *channel)
{
  lv_obj_t *preview = lv_obj_create(lv_screen_active());
  lv_obj_t *label;
  lv_obj_t *line;
  uint32_t point_count = UI_BuildPreviewPoints(channel->waveform);
  lv_color_t wave_color = channel->output_enable
                              ? UI_ChannelColor(s_state.active_channel)
                              : lv_color_hex(UI_COLOR_TEXT_MUTED);

  lv_obj_set_pos(preview, 4, 29);
  lv_obj_set_size(preview, 312, 36);
  UI_SetObjectBox(preview,
                  lv_color_hex(UI_COLOR_PANEL),
                  lv_color_hex(UI_COLOR_BORDER),
                  2);
  lv_obj_clear_flag(preview, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(preview, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(preview,
                      UI_CommandEvent,
                      LV_EVENT_CLICKED,
                      (void *)(uintptr_t)UI_CMD_WAVE_PAGE);

  label = UI_CreateLabel(preview, s_wave_names[channel->waveform], wave_color);
  lv_obj_set_pos(label, 7, 8);

  line = lv_line_create(preview);
  lv_line_set_points(line, s_preview_points, point_count);
  lv_obj_set_pos(line, 76, 5);
  lv_obj_set_style_line_color(line, wave_color, LV_PART_MAIN);
  lv_obj_set_style_line_width(line, 2, LV_PART_MAIN);
  lv_obj_set_style_line_rounded(line, true, LV_PART_MAIN);
  lv_obj_clear_flag(line, LV_OBJ_FLAG_CLICKABLE);
}

static void UI_ShowMain(void)
{
  const channel_state_t *channel = Generator_State_GetActiveConst(&s_state);
  lv_obj_t *content;
  uint8_t index;

  UI_ClearScreen();
  s_page = UI_PAGE_MAIN;
  s_mode = s_state.touch_locked ? UI_MODE_LOCKED : UI_MODE_NAV;
  (void)UI_CreateTopBar(NULL);
  UI_CreateWaveformPreview(channel);

  content = lv_obj_create(lv_screen_active());
  lv_obj_set_pos(content, 0, 66);
  lv_obj_set_size(content, 320, 130);
  UI_SetObjectBox(content,
                  lv_color_hex(UI_COLOR_BACKGROUND),
                  lv_color_hex(UI_COLOR_BACKGROUND),
                  0);
  lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

  for (index = 0U; index < UI_PARAM_COUNT; ++index)
  {
    lv_obj_t *row = lv_obj_create(content);
    lv_obj_t *name;
    lv_obj_t *value;
    char value_text[40];

    lv_obj_set_pos(row, 4, 1 + (int32_t)index * 32);
    lv_obj_set_size(row, 312, 30);
    UI_SetObjectBox(row,
                    lv_color_hex((index == s_parameter)
                                     ? UI_COLOR_PANEL_SELECTED
                                     : UI_COLOR_BACKGROUND),
                    (index == s_parameter)
                        ? UI_ChannelColor(s_state.active_channel)
                        : lv_color_hex(UI_COLOR_BORDER),
                    2);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(row,
                        UI_CommandEvent,
                        LV_EVENT_CLICKED,
                        (void *)(uintptr_t)(UI_CMD_PARAM_FREQ + index));

    name = UI_CreateLabel(row,
                          s_parameter_names[index],
                          (index == s_parameter)
                              ? UI_ChannelColor(s_state.active_channel)
                              : lv_color_hex(UI_COLOR_TEXT_MUTED));
    lv_obj_set_pos(name, 8, 6);

    UI_FormatParameter((UI_Parameter)index,
                       channel,
                       value_text,
                       sizeof(value_text));
    value = UI_CreateLabel(row, value_text, lv_color_hex(UI_COLOR_TEXT));
    lv_obj_set_style_text_font(value, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_align(value, LV_ALIGN_RIGHT_MID, -8, 0);
  }

  (void)UI_CreateButton(lv_screen_active(), "Wave", 2, 198, 77, 40,
                        UI_CMD_WAVE_PAGE, 1U);
  (void)UI_CreateButton(lv_screen_active(), "Sweep", 82, 198, 77, 40,
                        UI_CMD_NONE, 0U);
  (void)UI_CreateButton(lv_screen_active(), "Mod", 162, 198, 77, 40,
                        UI_CMD_NONE, 0U);
  (void)UI_CreateButton(lv_screen_active(), "More", 242, 198, 76, 40,
                        UI_CMD_MENU, 1U);
}

static void UI_ShowEdit(void)
{
  const channel_state_t *channel = Generator_State_GetActiveConst(&s_state);
  lv_obj_t *title;
  lv_obj_t *value;
  lv_obj_t *step;
  char text[48];

  UI_ClearScreen();
  s_page = UI_PAGE_EDIT;
  s_mode = UI_MODE_EDIT;
  (void)UI_CreateTopBar(s_parameter_names[s_parameter]);

  title = UI_CreateLabel(lv_screen_active(),
                         s_parameter_names[s_parameter],
                         lv_color_hex(UI_COLOR_TEXT_MUTED));
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 43);

  UI_FormatParameter(s_parameter, channel, text, sizeof(text));
  value = UI_CreateLabel(lv_screen_active(), text,
                         UI_ChannelColor(s_state.active_channel));
  lv_obj_set_style_text_font(value, &lv_font_montserrat_18, LV_PART_MAIN);
  lv_obj_align(value, LV_ALIGN_TOP_MID, 0, 70);

  UI_FormatStep(s_parameter, text, sizeof(text));
  step = UI_CreateLabel(lv_screen_active(), "", lv_color_hex(UI_COLOR_TEXT));
  lv_label_set_text_fmt(step, "Step  %s", text);
  lv_obj_align(step, LV_ALIGN_TOP_MID, 0, 110);

  (void)UI_CreateButton(lv_screen_active(), "-", 8, 151, 54, 45,
                        UI_CMD_ENCODER_CCW, 1U);
  (void)UI_CreateButton(lv_screen_active(), "Step", 68, 151, 82, 45,
                        UI_CMD_ENCODER_LONG, 1U);
  (void)UI_CreateButton(lv_screen_active(), "+", 156, 151, 54, 45,
                        UI_CMD_ENCODER_CW, 1U);
  (void)UI_CreateButton(lv_screen_active(), "OK", 216, 151, 96, 45,
                        UI_CMD_ENCODER_PRESS, 1U);
  (void)UI_CreateButton(lv_screen_active(), "Cancel / Back", 8, 204, 304, 30,
                        UI_CMD_BACK, 1U);
}

static uint8_t UI_GetStepCount(void)
{
  if (s_parameter == UI_PARAM_FREQUENCY)
  {
    return (uint8_t)(sizeof(s_frequency_steps) /
                     sizeof(s_frequency_steps[0]));
  }
  return 3U;
}

static void UI_ApplyStepSelection(void)
{
  if (s_parameter == UI_PARAM_FREQUENCY)
  {
    s_state.freq_step_hz = s_frequency_steps[s_step_selection];
  }
  else if (s_parameter == UI_PARAM_AMPLITUDE)
  {
    s_state.amp_step_v = s_amplitude_steps[s_step_selection];
  }
  else if (s_parameter == UI_PARAM_OFFSET)
  {
    s_state.offset_step_v = s_amplitude_steps[s_step_selection];
  }
  else
  {
    s_state.phase_step_deg = s_phase_steps[s_step_selection];
  }
}

static void UI_SelectCurrentStep(void)
{
  uint8_t index;
  uint8_t count = UI_GetStepCount();

  s_step_selection = 0U;
  for (index = 0U; index < count; ++index)
  {
    if ((s_parameter == UI_PARAM_FREQUENCY) &&
        (s_state.freq_step_hz == s_frequency_steps[index]))
    {
      s_step_selection = index;
    }
    else if ((s_parameter == UI_PARAM_AMPLITUDE) &&
             (s_state.amp_step_v == s_amplitude_steps[index]))
    {
      s_step_selection = index;
    }
    else if ((s_parameter == UI_PARAM_OFFSET) &&
             (s_state.offset_step_v == s_amplitude_steps[index]))
    {
      s_step_selection = index;
    }
    else if ((s_parameter == UI_PARAM_PHASE) &&
             (s_state.phase_step_deg == s_phase_steps[index]))
    {
      s_step_selection = index;
    }
  }
}

static void UI_ShowStepSelect(void)
{
  uint8_t index;
  uint8_t count = UI_GetStepCount();

  UI_ClearScreen();
  s_page = UI_PAGE_STEP_SELECT;
  s_mode = UI_MODE_POPUP;
  (void)UI_CreateTopBar("STEP SELECT");

  for (index = 0U; index < count; ++index)
  {
    lv_obj_t *button;
    lv_obj_t *label;
    char text[32];
    int32_t column = index % 2U;
    int32_t row = index / 2U;

    if (s_parameter == UI_PARAM_FREQUENCY)
    {
      UI_FormatFrequency(s_frequency_steps[index], text, sizeof(text));
    }
    else if (s_parameter == UI_PARAM_AMPLITUDE)
    {
      UI_FormatAmplitude(s_amplitude_steps[index], text, sizeof(text));
    }
    else if (s_parameter == UI_PARAM_OFFSET)
    {
      UI_FormatOffset(s_amplitude_steps[index], text, sizeof(text));
    }
    else
    {
      UI_FormatPhase(s_phase_steps[index], text, sizeof(text));
    }

    button = lv_button_create(lv_screen_active());
    lv_obj_set_pos(button, 7 + column * 157, 34 + row * 43);
    lv_obj_set_size(button, 149, 38);
    UI_SetObjectBox(button,
                    lv_color_hex((index == s_step_selection)
                                     ? UI_COLOR_PANEL_SELECTED
                                     : UI_COLOR_PANEL),
                    (index == s_step_selection)
                        ? UI_ChannelColor(s_state.active_channel)
                        : lv_color_hex(UI_COLOR_BORDER),
                    3);
    lv_obj_add_event_cb(button,
                        UI_CommandEvent,
                        LV_EVENT_CLICKED,
                        (void *)(uintptr_t)(UI_CMD_NUM_0 + index));
    label = UI_CreateLabel(button, text, lv_color_hex(UI_COLOR_TEXT));
    lv_obj_center(label);
  }

  (void)UI_CreateButton(lv_screen_active(), "Back", 7, 207, 149, 28,
                        UI_CMD_BACK, 1U);
  (void)UI_CreateButton(lv_screen_active(), "Apply", 164, 207, 149, 28,
                        UI_CMD_ENTER, 1U);
}

static void UI_ShowWaveform(void)
{
  uint8_t index;
  const channel_state_t *channel = Generator_State_GetActiveConst(&s_state);

  UI_ClearScreen();
  s_page = UI_PAGE_WAVEFORM;
  s_mode = UI_MODE_NAV;
  (void)UI_CreateTopBar("WAVEFORM");

  for (index = 0U; index < WAVE_COUNT; ++index)
  {
    lv_obj_t *button = UI_CreateButton(
        lv_screen_active(),
        s_wave_names[index],
        14 + (int32_t)(index % 2U) * 153,
        43 + (int32_t)(index / 2U) * 62,
        139,
        50,
        (UI_Command)(UI_CMD_WAVE_SINE + index),
        1U);
    if (channel->waveform == (waveform_t)index)
    {
      lv_obj_set_style_border_color(button,
                                    UI_ChannelColor(s_state.active_channel),
                                    LV_PART_MAIN);
      lv_obj_set_style_border_width(button, 2, LV_PART_MAIN);
    }
  }

  (void)UI_CreateButton(lv_screen_active(), "Back", 14, 181, 292, 44,
                        UI_CMD_BACK, 1U);
}

static void UI_ShowChannelOverview(void)
{
  uint8_t index;

  UI_ClearScreen();
  s_page = UI_PAGE_CHANNEL_OVERVIEW;
  s_mode = UI_MODE_NAV;
  (void)UI_CreateTopBar("CHANNEL OVERVIEW");

  for (index = 0U; index < GENERATOR_CHANNEL_COUNT; ++index)
  {
    const channel_state_t *channel = &s_state.ch[index];
    lv_obj_t *card = lv_button_create(lv_screen_active());
    lv_obj_t *label;
    char frequency[28];
    char phase[20];
    char summary[96];
    int32_t column = index % 2U;
    int32_t row = index / 2U;

    lv_obj_set_pos(card, 5 + column * 158, 33 + row * 88);
    lv_obj_set_size(card, 152, 82);
    UI_SetObjectBox(card,
                    lv_color_hex(UI_COLOR_PANEL),
                    (index == s_state.active_channel)
                        ? UI_ChannelColor(index)
                        : lv_color_hex(UI_COLOR_BORDER),
                    3);
    lv_obj_set_style_pad_all(card, 6, LV_PART_MAIN);
    lv_obj_add_event_cb(card,
                        UI_CommandEvent,
                        LV_EVENT_CLICKED,
                        (void *)(uintptr_t)(UI_CMD_CH_1 + index));

    UI_FormatFrequency(channel->frequency_hz,
                       frequency,
                       sizeof(frequency));
    UI_FormatPhase(channel->phase_deg, phase, sizeof(phase));
    (void)snprintf(summary,
                   sizeof(summary),
                   "CH%u  %s\n%s\n%s\n%s",
                   (unsigned int)(index + 1U),
                   channel->output_enable ? "ON" : "OFF",
                   s_wave_names[channel->waveform],
                   frequency,
                   phase);
    label = UI_CreateLabel(card, summary, lv_color_hex(UI_COLOR_TEXT));
    lv_obj_set_style_text_color(label, UI_ChannelColor(index), LV_PART_MAIN);
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 0, 0);
  }

  (void)UI_CreateButton(lv_screen_active(), "Back", 5, 211, 310, 24,
                        UI_CMD_BACK, 1U);
}

static void UI_ShowFunctionMenu(void)
{
  UI_ClearScreen();
  s_page = UI_PAGE_FUNCTION_MENU;
  s_mode = UI_MODE_NAV;
  (void)UI_CreateTopBar("FUNCTION MENU");

  (void)UI_CreateButton(lv_screen_active(), "Basic", 12, 42, 142, 44,
                        UI_CMD_HOME, 1U);
  (void)UI_CreateButton(lv_screen_active(), "Waveform", 166, 42, 142, 44,
                        UI_CMD_WAVE_PAGE, 1U);
  (void)UI_CreateButton(lv_screen_active(), "Channel", 12, 96, 142, 44,
                        UI_CMD_CH_OVERVIEW, 1U);
  (void)UI_CreateButton(lv_screen_active(), "Sweep", 166, 96, 142, 44,
                        UI_CMD_NONE, 0U);
  (void)UI_CreateButton(lv_screen_active(), "Modulation", 12, 150, 142, 44,
                        UI_CMD_NONE, 0U);
  (void)UI_CreateButton(lv_screen_active(), "Back", 166, 150, 142, 44,
                        UI_CMD_BACK, 1U);
}

static void UI_RebuildCurrentPage(void)
{
  switch (s_page)
  {
    case UI_PAGE_EDIT:
      UI_ShowEdit();
      break;
    case UI_PAGE_WAVEFORM:
      UI_ShowWaveform();
      break;
    case UI_PAGE_CHANNEL_OVERVIEW:
      UI_ShowChannelOverview();
      break;
    case UI_PAGE_FUNCTION_MENU:
      UI_ShowFunctionMenu();
      break;
    case UI_PAGE_STEP_SELECT:
      UI_ShowStepSelect();
      break;
    case UI_PAGE_MAIN:
    default:
      UI_ShowMain();
      break;
  }
}

static void UI_StartEdit(UI_Parameter parameter)
{
  channel_state_t *channel = Generator_State_GetActive(&s_state);

  s_parameter = parameter;
  s_edit_backup = *channel;
  UI_ShowEdit();
}

static void UI_AdjustParameter(int8_t direction)
{
  channel_state_t *channel = Generator_State_GetActive(&s_state);

  if (s_parameter == UI_PARAM_FREQUENCY)
  {
    channel->frequency_hz += (double)direction * s_state.freq_step_hz;
    if (channel->frequency_hz < UI_FREQ_MIN_HZ)
    {
      channel->frequency_hz = UI_FREQ_MIN_HZ;
    }
    else if (channel->frequency_hz > UI_FREQ_MAX_HZ)
    {
      channel->frequency_hz = UI_FREQ_MAX_HZ;
    }
  }
  else if (s_parameter == UI_PARAM_AMPLITUDE)
  {
    channel->amplitude_vpp += (float)direction * s_state.amp_step_v;
    if (channel->amplitude_vpp < UI_AMPLITUDE_MIN_VPP)
    {
      channel->amplitude_vpp = UI_AMPLITUDE_MIN_VPP;
    }
    else if (channel->amplitude_vpp > UI_AMPLITUDE_MAX_VPP)
    {
      channel->amplitude_vpp = UI_AMPLITUDE_MAX_VPP;
    }
  }
  else if (s_parameter == UI_PARAM_OFFSET)
  {
    channel->offset_v += (float)direction * s_state.offset_step_v;
    if (channel->offset_v < UI_OFFSET_MIN_V)
    {
      channel->offset_v = UI_OFFSET_MIN_V;
    }
    else if (channel->offset_v > UI_OFFSET_MAX_V)
    {
      channel->offset_v = UI_OFFSET_MAX_V;
    }
  }
  else
  {
    channel->phase_deg += (float)direction * s_state.phase_step_deg;
    while (channel->phase_deg >= 360.0F)
    {
      channel->phase_deg -= 360.0F;
    }
    while (channel->phase_deg < 0.0F)
    {
      channel->phase_deg += 360.0F;
    }
  }
}

static void UI_HandleBack(void)
{
  channel_state_t *channel = Generator_State_GetActive(&s_state);

  if (s_page == UI_PAGE_MAIN)
  {
    return;
  }
  if (s_page == UI_PAGE_EDIT)
  {
    *channel = s_edit_backup;
    UI_ShowMain();
  }
  else if (s_page == UI_PAGE_STEP_SELECT)
  {
    UI_ShowEdit();
  }
  else
  {
    UI_ShowMain();
  }
}

static void UI_HandleEncoder(UI_Command command)
{
  int8_t direction = (command == UI_CMD_ENCODER_CW) ? 1 : -1;

  if (s_page == UI_PAGE_MAIN)
  {
    if ((command == UI_CMD_ENCODER_CW) ||
        (command == UI_CMD_ENCODER_CCW))
    {
      int32_t next = (int32_t)s_parameter + direction;
      if (next < 0)
      {
        next = UI_PARAM_COUNT - 1;
      }
      else if (next >= UI_PARAM_COUNT)
      {
        next = 0;
      }
      s_parameter = (UI_Parameter)next;
      UI_ShowMain();
    }
    else if (command == UI_CMD_ENCODER_PRESS)
    {
      UI_StartEdit(s_parameter);
    }
    else if (command == UI_CMD_ENCODER_LONG)
    {
      UI_StartEdit(s_parameter);
      UI_SelectCurrentStep();
      UI_ShowStepSelect();
    }
  }
  else if (s_page == UI_PAGE_EDIT)
  {
    if ((command == UI_CMD_ENCODER_CW) ||
        (command == UI_CMD_ENCODER_CCW))
    {
      UI_AdjustParameter(direction);
      UI_ShowEdit();
    }
    else if (command == UI_CMD_ENCODER_PRESS)
    {
      UI_ShowMain();
    }
    else if (command == UI_CMD_ENCODER_LONG)
    {
      UI_SelectCurrentStep();
      UI_ShowStepSelect();
    }
  }
  else if (s_page == UI_PAGE_STEP_SELECT)
  {
    if ((command == UI_CMD_ENCODER_CW) ||
        (command == UI_CMD_ENCODER_CCW))
    {
      int32_t next = (int32_t)s_step_selection + direction;
      uint8_t count = UI_GetStepCount();
      if (next < 0)
      {
        next = count - 1U;
      }
      else if (next >= count)
      {
        next = 0;
      }
      s_step_selection = (uint8_t)next;
      UI_ShowStepSelect();
    }
    else if (command == UI_CMD_ENCODER_PRESS)
    {
      UI_ApplyStepSelection();
      UI_ShowEdit();
    }
  }
}

void UI_SignalGenerator_Init(void)
{
  Generator_State_Init(&s_state);
  s_parameter = UI_PARAM_FREQUENCY;
  s_page = UI_PAGE_MAIN;
  s_mode = UI_MODE_NAV;
  s_toast = NULL;
  UI_ShowMain();
}

void UI_SignalGenerator_Process(void)
{
  if ((s_toast != NULL) &&
      ((int32_t)(HAL_GetTick() - s_toast_until_ms) >= 0))
  {
    lv_obj_delete(s_toast);
    s_toast = NULL;
  }
}

void UI_SignalGenerator_Dispatch(UI_Command command)
{
  channel_state_t *channel = Generator_State_GetActive(&s_state);

  if (command == UI_CMD_BACK)
  {
    UI_HandleBack();
    return;
  }
  if (command == UI_CMD_HOME)
  {
    UI_ShowMain();
    return;
  }
  if (command == UI_CMD_CH_NEXT)
  {
    s_state.active_channel =
        (uint8_t)((s_state.active_channel + 1U) % GENERATOR_CHANNEL_COUNT);
    UI_ShowMain();
    return;
  }
  if (command == UI_CMD_CH_PREV)
  {
    s_state.active_channel = (s_state.active_channel == 0U)
                                 ? (GENERATOR_CHANNEL_COUNT - 1U)
                                 : (s_state.active_channel - 1U);
    UI_ShowMain();
    return;
  }
  if (command == UI_CMD_CH_OVERVIEW)
  {
    UI_ShowChannelOverview();
    return;
  }
  if ((command >= UI_CMD_CH_1) && (command <= UI_CMD_CH_4))
  {
    s_state.active_channel = (uint8_t)(command - UI_CMD_CH_1);
    UI_ShowMain();
    return;
  }
  if (command == UI_CMD_OUTPUT_TOGGLE)
  {
    channel->output_enable = !channel->output_enable;
    UI_RebuildCurrentPage();
    UI_ShowToast(channel->output_enable ? "OUTPUT ON" : "OUTPUT OFF",
                 channel->output_enable
                     ? UI_ChannelColor(s_state.active_channel)
                     : lv_color_hex(UI_COLOR_WARNING));
    return;
  }
  if (command == UI_CMD_OUTPUT_ALL_OFF)
  {
    uint8_t index;
    for (index = 0U; index < GENERATOR_CHANNEL_COUNT; ++index)
    {
      s_state.ch[index].output_enable = false;
    }
    UI_RebuildCurrentPage();
    UI_ShowToast("ALL OUTPUTS OFF", lv_color_hex(UI_COLOR_WARNING));
    return;
  }
  if (command == UI_CMD_WAVE_NEXT)
  {
    channel->waveform = (waveform_t)((channel->waveform + 1U) % WAVE_COUNT);
    UI_RebuildCurrentPage();
    return;
  }
  if (command == UI_CMD_WAVE_PAGE)
  {
    UI_ShowWaveform();
    return;
  }
  if ((command >= UI_CMD_WAVE_SINE) &&
      (command <= UI_CMD_WAVE_PULSE))
  {
    channel->waveform = (waveform_t)(command - UI_CMD_WAVE_SINE);
    UI_ShowWaveform();
    return;
  }
  if (command == UI_CMD_MENU)
  {
    UI_ShowFunctionMenu();
    return;
  }
  if (command == UI_CMD_TOUCH_LOCK)
  {
    s_state.touch_locked = !s_state.touch_locked;
    LV_Port_Input_SetPointerEnabled(s_state.touch_locked ? 0U : 1U);
    UI_ShowMain();
    UI_ShowToast(s_state.touch_locked ? "TOUCH LOCKED" : "TOUCH UNLOCKED",
                 lv_color_hex(s_state.touch_locked ? UI_COLOR_WARNING
                                                   : s_channel_colors[3]));
    return;
  }
  if ((command == UI_CMD_ENCODER_CW) ||
      (command == UI_CMD_ENCODER_CCW) ||
      (command == UI_CMD_ENCODER_PRESS) ||
      (command == UI_CMD_ENCODER_LONG))
  {
    UI_HandleEncoder(command);
    return;
  }
  if ((command >= UI_CMD_PARAM_FREQ) &&
      (command <= UI_CMD_PARAM_PHASE))
  {
    UI_StartEdit((UI_Parameter)(command - UI_CMD_PARAM_FREQ));
    return;
  }
  if ((s_page == UI_PAGE_STEP_SELECT) &&
      (command >= UI_CMD_NUM_0) && (command <= UI_CMD_NUM_9))
  {
    uint8_t index = (uint8_t)(command - UI_CMD_NUM_0);
    if (index < UI_GetStepCount())
    {
      s_step_selection = index;
      UI_ApplyStepSelection();
      UI_ShowEdit();
    }
    return;
  }
  if ((s_page == UI_PAGE_STEP_SELECT) && (command == UI_CMD_ENTER))
  {
    UI_ApplyStepSelection();
    UI_ShowEdit();
  }
}

const generator_state_t *UI_SignalGenerator_GetState(void)
{
  return &s_state;
}
