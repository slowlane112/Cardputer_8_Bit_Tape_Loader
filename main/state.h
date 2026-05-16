/*
 * SPDX-FileCopyrightText: 2026 slowlane112
 *
 * SPDX-License-Identifier: Apache-2.0
 */
 
#ifndef STATE_H
#define STATE_H

typedef enum {
    STATE_SYSTEM,
    STATE_OPTION,
    STATE_HELP,    
    STATE_FILE_BROWSER,
    STATE_PLAYER_COMMODORE,
    STATE_PLAYER_SPECTRUM,
    STATE_PLAYER_MSX,
    STATE_PLAYER_ACORN,
    STATE_PLAYER_DRAGON,
    STATE_PLAYER_ORIC   
} State;

extern State state;

#endif



