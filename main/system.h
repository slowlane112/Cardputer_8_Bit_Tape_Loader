/*
 * SPDX-FileCopyrightText: 2026 slowlane112
 *
 * SPDX-License-Identifier: Apache-2.0
 */
 
#ifndef SYSTEM_H
#define SYSTEM_H

extern int system_selected_index;
extern const char *systems[];
extern const int systems_count;
const char* system_get_name(int index);
void system_main(void);

#endif



