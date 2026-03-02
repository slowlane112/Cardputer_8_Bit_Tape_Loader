/*
 * SPDX-FileCopyrightText: 2026 slowlane112
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TAPE_BUFFER_H
#define TAPE_BUFFER_H

#define TAPE_BUFFER_SIZE 40960

extern uint8_t * volatile tape_buffer_1;
extern uint8_t * volatile tape_buffer_2;
extern volatile size_t tape_buffer_1_offset;
extern volatile size_t tape_buffer_1_size;
extern volatile size_t tape_buffer_2_offset;
extern volatile size_t tape_buffer_2_size;

void tape_buffer_load_initial(size_t buffer_overlap, size_t start_pos);
void tape_buffer_load(size_t buffer_overlap);
void tape_buffer_swap(void);

#endif

