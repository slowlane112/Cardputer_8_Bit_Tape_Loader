/*
 * SPDX-FileCopyrightText: 2026 slowlane112
 *
 * SPDX-License-Identifier: Apache-2.0
 */
 
#ifndef COMMODORE_PLAYER_H
#define COMMODORE_PLAYER_H

extern volatile uint8_t commodore_player_data_tracker;
extern volatile bool commodore_player_tape_status;
extern volatile bool commodore_player_load_buffer;
extern volatile size_t commodore_player_buffer_overlap;
extern volatile bool commodore_player_process_active;
extern volatile bool commodore_player_user_tape_status;
extern volatile size_t commodore_player_pos;
extern volatile int8_t commodore_player_system;
extern volatile int8_t commodore_player_format;

void commodore_player_main();

#endif
