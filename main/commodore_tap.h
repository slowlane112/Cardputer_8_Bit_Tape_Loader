/*
 * SPDX-FileCopyrightText: 2026 slowlane112
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef COMMODORE_TAP_H
#define COMMODORE_TAP_H

#define TAP_HEADER_SIZE 20

extern volatile bool do_load_buffer;
void commodore_tap_main();
void load_buffer() ;

#endif

