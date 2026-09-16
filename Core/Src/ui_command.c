#include "ui_command.h"

#include "main.h"

#define UI_COMMAND_QUEUE_CAPACITY 24U

static volatile UI_Command s_queue[UI_COMMAND_QUEUE_CAPACITY];
static volatile uint8_t s_head;
static volatile uint8_t s_tail;

static uint8_t UI_CommandQueue_PostUnlocked(UI_Command command)
{
  uint8_t next;

  if (command == UI_CMD_NONE)
  {
    return 0U;
  }

  next = (uint8_t)((s_head + 1U) % UI_COMMAND_QUEUE_CAPACITY);
  if (next == s_tail)
  {
    return 0U;
  }

  s_queue[s_head] = command;
  s_head = next;
  return 1U;
}

void UI_CommandQueue_Init(void)
{
  s_head = 0U;
  s_tail = 0U;
}

uint8_t UI_CommandQueue_Post(UI_Command command)
{
  uint32_t primask = __get_PRIMASK();
  uint8_t result;

  __disable_irq();
  result = UI_CommandQueue_PostUnlocked(command);
  if (primask == 0U)
  {
    __enable_irq();
  }
  return result;
}

uint8_t UI_CommandQueue_PostFromISR(UI_Command command)
{
  return UI_CommandQueue_PostUnlocked(command);
}

uint8_t UI_CommandQueue_Get(UI_Command *command)
{
  uint32_t primask;

  if (command == NULL)
  {
    return 0U;
  }

  primask = __get_PRIMASK();
  __disable_irq();
  if (s_tail == s_head)
  {
    if (primask == 0U)
    {
      __enable_irq();
    }
    return 0U;
  }

  *command = s_queue[s_tail];
  s_tail = (uint8_t)((s_tail + 1U) % UI_COMMAND_QUEUE_CAPACITY);
  if (primask == 0U)
  {
    __enable_irq();
  }
  return 1U;
}
