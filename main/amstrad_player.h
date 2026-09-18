/*
 * SPDX-FileCopyrightText: 2026 slowlane112
 *
 * SPDX-License-Identifier: Apache-2.0
 */
 
#ifndef AMSTRAD_PLAYER_H
#define AMSTRAD_PLAYER_H

extern volatile uint8_t amstrad_player_data_tracker;
extern volatile bool amstrad_player_load_buffer;
extern volatile size_t amstrad_player_buffer_overlap;
extern volatile size_t amstrad_player_pos;
extern volatile size_t amstrad_player_stop_pos;
extern volatile bool amstrad_player_process_active;
extern volatile bool amstrad_player_tape_status; // playing / stopped
extern volatile bool amstrad_player_user_tape_status;
extern volatile bool amstrad_use_remote;
void amstrad_player_main();

#endif

