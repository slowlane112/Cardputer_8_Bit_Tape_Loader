/*
 * SPDX-FileCopyrightText: 2026 slowlane112
 *
 * SPDX-License-Identifier: Apache-2.0
 */
 
#ifndef ORIC_PLAYER_H
#define ORIC_PLAYER_H

extern volatile uint8_t oric_player_data_tracker;
extern volatile bool oric_player_load_buffer;
extern volatile size_t oric_player_buffer_overlap;
extern volatile size_t oric_player_pos;
extern volatile bool oric_player_process_active;
extern volatile bool oric_player_tape_status; // playing / stopped
extern volatile bool oric_player_user_tape_status;
extern volatile bool oric_use_remote;
void oric_player_main();

#endif

