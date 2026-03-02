/*
 * SPDX-FileCopyrightText: 2026 slowlane112
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FILE_H
#define FILE_H

void file_display_directory_name(const char *directory, char *out, size_t out_size);
void file_display_file_name(const char *filename, sdcard_item_type_t type, char *out, size_t out_size);
const char* file_name_scroll(const char *name);

#endif



