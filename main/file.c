/*
 * SPDX-FileCopyrightText: 2026 slowlane112
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include "sdcard.h"
#include "esp_timer.h"

#define DIRECTORY_NAME_MAX_LEN	29
#define FILE_NAME_SCROLL_DISPLAY_LEN        29
#define FILE_NAME_SCROLL_INTERVAL_US 		(400 * 1000)
#define FILE_NAME_SCROLL_HOLD_START_US      (3 * 1000 * 1000)
#define FILE_NAME_SCROLL_HOLD_END_US        (3 * 1000 * 1000)
#define FILE_NAME_SCROLL_RESET_GAP_US       (2 * 1000 * 1000)

typedef enum {
    STATE_HOLD_START,
    STATE_SCROLL,
    STATE_HOLD_END
} file_name_scroll_state_t;

void file_display_directory_name(const char *directory, char *out, size_t out_size) {

    if (!out || out_size == 0) return;

    size_t terminal_index = (out_size > DIRECTORY_NAME_MAX_LEN + 1) ? DIRECTORY_NAME_MAX_LEN : (out_size - 1);
    size_t len = strlen(directory);

    if (len <= terminal_index) {
        strncpy(out, directory, terminal_index);
    } else if (terminal_index >= 3) {
        out[0] = '.'; out[1] = '.'; out[2] = '.';
        size_t chars_to_copy = terminal_index - 3;
        memcpy(out + 3, directory + (len - chars_to_copy), chars_to_copy);
    } else {
        memset(out, '.', terminal_index);
    }
    out[terminal_index] = '\0';
}

void file_display_file_name(const char *filename, sdcard_item_type_t type, char *out, size_t out_size)
{
    if (!out || out_size == 0) return;

    size_t max_len = out_size - 1;
    out[max_len] = '\0';

    if (!filename) {
        out[0] = '\0';
        return;
    }

    const char *base = filename;
    size_t base_len = strlen(base);

    if (type == SDCARD_FILE) {
        const char *dot = strrchr(base, '.');
        if (dot) {
            base_len = (size_t)(dot - base);
        }
    }

    if (base_len <= max_len) {
        memcpy(out, base, base_len);
        out[base_len] = '\0';
        return;
    }

    if (max_len >= 5) {
        size_t head = (max_len - 3) / 2;
        size_t tail = max_len - head - 3;

        memcpy(out, base, head);

        out[head] = '.';
        out[head + 1] = '.';
        out[head + 2] = '.';

        memcpy(out + head + 3, base + (base_len - tail), tail);

        out[max_len] = '\0';
        return;
    }

    memset(out, '.', max_len);
    out[max_len] = '\0';
}

const char* file_name_scroll(const char *name)
{
    static char out[FILE_NAME_SCROLL_DISPLAY_LEN+ 1];
    int len = strlen(name);
    
    if (len <= FILE_NAME_SCROLL_DISPLAY_LEN) {
        memcpy(out, name, len);
        out[len] = '\0';
        return out;
    }
    
    static int pos = 0;
    static int last_len = -1;
    static file_name_scroll_state_t state = STATE_HOLD_START;

    static uint64_t last_call_time = 0;
    static uint64_t last_scroll_time = 0;
    static uint64_t hold_until = 0;

    uint64_t now = esp_timer_get_time();
    
    if (last_call_time == 0 ||  
        now - last_call_time >= FILE_NAME_SCROLL_RESET_GAP_US || 
        len != last_len) 
    {
        pos = 0;
        last_len = len;
        state = STATE_HOLD_START;
        hold_until = now + FILE_NAME_SCROLL_HOLD_START_US;
        last_scroll_time = now;
    }

    last_call_time = now;

    int max_pos = len - FILE_NAME_SCROLL_DISPLAY_LEN;

    switch (state) {

    case STATE_HOLD_START:
        if (now >= hold_until) {
            state = STATE_SCROLL;
            last_scroll_time = now;
        }
        break;

    case STATE_SCROLL:
        if (now - last_scroll_time >= FILE_NAME_SCROLL_INTERVAL_US) {
            last_scroll_time = now;

            if (pos < max_pos) {
                pos++;
            } else {
                state = STATE_HOLD_END;
                hold_until = now + FILE_NAME_SCROLL_HOLD_END_US;
            }
        }
        break;

    case STATE_HOLD_END:
        if (now >= hold_until) {
            pos = 0;
            state = STATE_HOLD_START;
            hold_until = now + FILE_NAME_SCROLL_HOLD_START_US;
            last_scroll_time = now;
        }
        break;
    }

    memcpy(out, name + pos, FILE_NAME_SCROLL_DISPLAY_LEN);
    out[FILE_NAME_SCROLL_DISPLAY_LEN] = '\0';

    return out;
}
