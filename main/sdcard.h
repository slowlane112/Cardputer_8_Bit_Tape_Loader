/*
 * SPDX-FileCopyrightText: 2026 slowlane112
 *
 * SPDX-License-Identifier: Apache-2.0
 */
 
#ifndef SDCARD_H
#define SDCARD_H

#define SDCARD_CHUNK_SIZE 32

typedef enum {
	SD_RESULT_UNKNOWN = 0,
    SD_OK,
    SD_ERR_NOT_FOUND,
    SD_ERR_TIMEOUT,
    SD_ERR_NO_MEM,
    SD_ERR_FILE_OPEN,
    SD_OK_PARTIAL
} sdcard_result_t;

typedef enum {
    SDCARD_FILE,
    SDCARD_DIR
} sdcard_item_type_t;

typedef struct {
	uint32_t file_index;
    char *name;
    sdcard_item_type_t type;
} sdcard_entry_t;

typedef struct sdcard_chunk {
    sdcard_entry_t entries[SDCARD_CHUNK_SIZE];
    size_t count;
    struct sdcard_chunk *next;
} sdcard_chunk_t;

typedef struct {
    sdcard_entry_t **entries;
    size_t count;
    sdcard_result_t status;
    sdcard_chunk_t *chunks;
} sdcard_list_t;

void sdcard_init(void);
void sdcard_deinit(void);
void sdcard_system_init(void);
void sdcard_list_free(sdcard_list_t *list);
sdcard_list_t sdcard_list_dir(const char *path);
FILE *sdcard_open(const char *path, size_t *out_len);
size_t sdcard_read_chunk(FILE *f, size_t file_len, size_t pos, uint8_t *buf, size_t chunk_size);
void sdcard_get_filename_by_index(const char *path, uint32_t target_index, char *out, size_t out_size);
uint32_t sdcard_get_index_by_filename(const char *path, const char *filename);
#endif
