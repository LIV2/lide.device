#include <stdbool.h>
#include "device.h"

#define TASK_NAME "ide.task"

void  __attribute__((used)) ide_task();

struct TaskData {
    struct DeviceBase *dev;
    bool   failure;
};

