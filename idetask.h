#include <stdbool.h>
#include "device.h"

#define TASK_NAME "idetask"
#define TASK_PRIORITY 0
#define TASK_STACK_SIZE 65535

void ide_task();
