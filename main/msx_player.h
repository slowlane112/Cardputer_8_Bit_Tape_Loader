/*
 * SPDX-FileCopyrightText: 2026 slowlane112
 *
 * SPDX-License-Identifier: Apache-2.0
 */
 
#ifndef MSX_PLAYER_H
#define MSX_PLAYER_H

extern volatile uint8_t msx_player_data_tracker;
extern volatile bool msx_player_load_buffer;
extern volatile size_t msx_player_buffer_overlap;
extern volatile size_t msx_player_pos;
extern volatile bool msx_player_process_active;
extern volatile bool msx_player_tape_status; // playing / stopped
extern volatile bool msx_player_user_tape_status;
void msx_player_main();

#endif

