/*
 * SPDX-FileCopyrightText: 2026 slowlane112
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef DISPLAY_H
#define DISPLAY_H

#define DISPLAY_WIDTH	240
#define DISPLAY_HEIGHT	135

#include <stdbool.h>

extern uint16_t *framebuffer;
extern bool display_transfer_in_progress;
void display_init(void);
void display_draw(void);

#endif
