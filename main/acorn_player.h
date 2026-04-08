/*
 * SPDX-FileCopyrightText: 2026 slowlane112
 *
 * SPDX-License-Identifier: Apache-2.0
 */
 
#ifndef ACORN_PLAYER_H
#define ACORN_PLAYER_H

extern volatile uint8_t acorn_player_data_tracker;
extern volatile bool acorn_player_load_buffer;
extern volatile size_t acorn_player_buffer_overlap;
extern volatile size_t acorn_player_pos;
extern volatile bool acorn_player_process_active;
extern volatile bool acorn_player_tape_status; // playing / stopped
extern volatile bool acorn_player_user_tape_status;
extern volatile bool acorn_use_remote;
void acorn_player_main();

#endif

