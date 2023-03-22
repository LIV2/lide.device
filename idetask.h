#include "device.h"
#include <stdbool.h>

#define TASK_NAME "idetask"
#define TASK_PRIORITY 0
#define TASK_STACK_SIZE 65535

#define CMD_DIE CMD_NONSTD+1

void ide_task();
