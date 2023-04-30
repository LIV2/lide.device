// SPDX-License-Identifier: GPL-2.0-only
/* This file is part of lide.device
 * Copyright (C) 2023 Matthew Harlum <matt@harlum.net>
 */
#define TASK_NAME "idetask"
#define TASK_PRIORITY 0
#define TASK_STACK_SIZE 65535

#define CMD_DIE 0x1000

void ide_task();
void diskchange_task();