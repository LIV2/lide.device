#include <proto/exec.h>
#include <devices/timer.h>
#include <exec/execbase.h>
#include <exec/devices.h>
#include <exec/io.h>
#include <exec/ports.h>
#include <clib/alib_protos.h>
#include <stdio.h>

#include <clib/debug_protos.h>

const char taskname[] = "ide.task";

static void __attribute__((used)) my_task() {

    while (1) {;;}
}

int main () {
    struct Task *task = NULL;

    task = CreateTask(taskname,0,my_task,8192);
    if (task) {
            printf("created task with address\n");
            while (1);
    }
}