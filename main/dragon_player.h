/*
 * SPDX-FileCopyrightText: 2026 slowlane112
 *
 * SPDX-License-Identifier: Apache-2.0
 */
 
#ifndef DRAGON_PLAYER_H
#define DRAGON_PLAYER_H

extern volatile uint8_t dragon_player_data_tracker;
extern volatile bool dragon_player_load_buffer;
extern volatile size_t dragon_player_buffer_overlap;
extern volatile size_t dragon_player_pos;
extern volatile bool dragon_player_process_active;
extern volatile bool dragon_player_tape_status; // playing / stopped
extern volatile bool dragon_player_user_tape_status;
extern volatile bool dragon_use_remote;
void dragon_player_main();

#endif

