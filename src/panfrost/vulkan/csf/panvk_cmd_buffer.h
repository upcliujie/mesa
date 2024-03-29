/*
 * Copyright © 2024 Collabora Ltd.
 * SPDX-License-Identifier: MIT
 */

#ifndef PANVK_CMD_BUFFER_H
#define PANVK_CMD_BUFFER_H

#ifndef PAN_ARCH
#error "PAN_ARCH must be defined"
#endif

#include <stdint.h>

#include "vk_command_buffer.h"

#include "genxml/gen_macros.h"

#define MAX_BIND_POINTS 2 /* compute + graphics */
#define MAX_VBS         16

#endif /* PANVK_CMD_BUFFER_H */
