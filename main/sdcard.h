/*
 * SPDX-FileCopyrightText: 2026 slowlane112
 *
 * SPDX-License-Identifier: Apache-2.0
 */
 
#ifndef SDCARD_H
#define SDCARD_H

typedef enum {
	SD_RESULT_UNKNOWN = 0,
    SD_OK,
    SD_ERR_NOT_FOUND,
    SD_ERR_TIMEOUT,
    SD_ERR_NO_MEM,
    SD_ERR_FILE_OPEN
} sdcard_result_t;

typedef enum {
    SDCARD_FILE,
    SDCARD_DIR
} sdcard_item_type_t;

typedef struct {
    char *name;
    sdcard_item_type_t type;
} sdcard_entry_t;

typedef struct {
    sdcard_entry_t *entries;
    size_t count;
    sdcard_result_t status;
} sdcard_list_t;


void sdcard_init(void);
void sdcard_deinit(void);
void sdcard_system_init(void);
sdcard_list_t sdcard_list_dir(const char *path);
FILE *sd_open(const char *path, size_t *out_len);
size_t sd_read_chunk(FILE *f, size_t file_len, size_t pos, uint8_t *buf, size_t chunk_size);

#endif
