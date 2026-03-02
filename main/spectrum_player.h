/*
 * SPDX-FileCopyrightText: 2026 slowlane112
 *
 * SPDX-License-Identifier: Apache-2.0
 */
 
#ifndef SPECTRUM_PLAYER_H
#define SPECTRUM_PLAYER_H

extern volatile uint8_t spectrum_player_data_tracker;
extern volatile bool spectrum_player_load_buffer;
extern volatile size_t spectrum_player_buffer_overlap;
extern volatile size_t spectrum_player_pos;
extern volatile bool spectrum_player_process_active;
extern volatile bool spectrum_player_tape_status; // playing / stopped
extern volatile bool spectrum_player_user_tape_status;
extern volatile uint8_t spectrum_player_system_type;

void spectrum_player_main();

#endif

