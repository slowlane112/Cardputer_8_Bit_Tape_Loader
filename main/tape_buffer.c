/*
 * SPDX-FileCopyrightText: 2026 slowlane112
 *
 * SPDX-License-Identifier: Apache-2.0
 */
 
#include <stdio.h>
#include "file_browser.h"
#include "tape_buffer.h"
#include "sdcard.h"

static uint8_t tape_buffer_storage_1[TAPE_BUFFER_SIZE]; 
static uint8_t tape_buffer_storage_2[TAPE_BUFFER_SIZE];
uint8_t * volatile tape_buffer_1 = tape_buffer_storage_1;
uint8_t * volatile tape_buffer_2 = tape_buffer_storage_2;
volatile size_t tape_buffer_1_offset;
volatile size_t tape_buffer_1_size;
volatile size_t tape_buffer_2_offset;
volatile size_t tape_buffer_2_size;

void tape_buffer_load_initial(size_t buffer_overlap, size_t start_pos) {
    size_t pos = start_pos;
    tape_buffer_1_size = sd_read_chunk(file_browser_file, file_browser_file_len, pos, (uint8_t *)tape_buffer_1, TAPE_BUFFER_SIZE);
    tape_buffer_1_offset = pos;
}

void tape_buffer_load(size_t buffer_overlap) {
	
    if (tape_buffer_1_size == TAPE_BUFFER_SIZE) {
        size_t pos = tape_buffer_1_offset + tape_buffer_1_size - buffer_overlap;

        tape_buffer_2_size = sd_read_chunk(
            file_browser_file,
            file_browser_file_len,
            pos,
            (uint8_t *)tape_buffer_2, 
            TAPE_BUFFER_SIZE
        );
        tape_buffer_2_offset = pos;
    }
    
}

void tape_buffer_swap(void)
{
	
    // swap pointers to the buffers
    uint8_t *tmp_buf      = tape_buffer_1;
    tape_buffer_1         = tape_buffer_2;
    tape_buffer_2         = tmp_buf;

    // swap sizes
    size_t tmp_size       = tape_buffer_1_size;
    tape_buffer_1_size    = tape_buffer_2_size;
    tape_buffer_2_size    = tmp_size;

    // swap offsets
    size_t tmp_offset      = tape_buffer_1_offset;
    tape_buffer_1_offset   = tape_buffer_2_offset;
    tape_buffer_2_offset   = tmp_offset;
    
}

