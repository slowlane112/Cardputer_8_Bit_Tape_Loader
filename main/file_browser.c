/*
 * SPDX-FileCopyrightText: 2026 slowlane112
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <ctype.h>
#include "freertos/FreeRTOS.h"
#include "system.h"
#include "keyboard.h"
#include "sdcard.h"
#include "commodore_player.h"
#include "spectrum_player.h"
#include "msx_player.h"
#include "display.h"
#include "graphic.h"
#include "file.h"

FILE *file_browser_file = NULL;
size_t file_browser_file_len = 0;
char *file_browser_file_name;
static sdcard_list_t items = {0};
static int selected_item = 0;
static char current_dir[2048] = "/sdcard";
static char previous_item[256];
static char selected_file_path[2200]; 
static bool file_browser_process = false;
static bool update_display = false;
static int exit_mode = 0;
static sdcard_result_t sdcard_status = 0;

static int get_selected_item() {
	
	for (size_t i = 0; i < items.count; i++) {
		if (strcmp(previous_item, items.entries[i].name) == 0) {
			 return i;
		 }
	}
	
	return 0;
}

static void display_screen(void) {
	
	for (int y = HEADER_HEIGHT; y < DISPLAY_HEIGHT; y++) {
		
		for (int x = 0; x < DISPLAY_WIDTH; x++) {
		
			framebuffer[(y * DISPLAY_WIDTH) + x] = BG_COLOR;
		}
	}
	
	if (sdcard_status == SD_OK) {
	
		int items_per_page = 6;
		int current_page = (int)(selected_item / (float)items_per_page);
		
		int item_start = current_page * items_per_page;
		int item_end = item_start + items_per_page;
		
		if (item_end > items.count) {
			item_end = items.count;
		}
		
		int text_y_start = 2;
		int text_x_start = 4;
		
		char directory_name[40];
		file_display_directory_name(current_dir, directory_name, sizeof(directory_name));
		

		draw_header((const char *)directory_name);
		
		text_y_start = text_y_start + 20;
		
		for (size_t i = item_start; i < item_end; i++) {
			
			char item_name[40];
			char temp_file[39];
			
			file_display_file_name(items.entries[i].name, items.entries[i].type, temp_file, sizeof(temp_file));
			snprintf(item_name, sizeof(item_name), "%s%s", ((i == selected_item) ? ">" : " "), temp_file);

			graphic_display_text(item_name, text_y_start, text_x_start, ((items.entries[i].type == SDCARD_DIR) ? FOLDER_COLOR : LABEL_COLOR), BG_COLOR);
			
		
			text_y_start = text_y_start + 19;
		}
	
	}
	else {
		graphic_display_text("error", 40, 4, LABEL_COLOR, BG_COLOR);
	}
	
	display_draw();
	
}

static void get_files(const char *directory)
{
	
	items = sdcard_list_dir(directory);
	sdcard_status = items.status;
	
	if (sdcard_status == SD_OK) {

		selected_item = 0;
		if (previous_item[0] != '\0') {
			selected_item = get_selected_item();
			previous_item[0] = '\0';
		}
	
	}
	
	update_display = true;

}

static void pop_dir(void) {
    size_t len = strlen(current_dir);

    int slash_count = 0;
    for (size_t i = 0; i < len; i++) {
        if (current_dir[i] == '/')
            slash_count++;
    }

    if (slash_count <= 1)
        return;

    if (len > 0 && current_dir[len - 1] == '/') {
        current_dir[len - 1] = '\0';
        len--;
    }

    while (len > 0 && current_dir[len - 1] != '/') {
        len--;
    }

    current_dir[len - 1] = '\0';
}

static void append_dir(const char *name) {
    strncat(current_dir, "/", sizeof(current_dir) - strlen(current_dir) - 1);
    strncat(current_dir, name, sizeof(current_dir) - strlen(current_dir) - 1);
}

static void get_previous_item(const char *path, char *out, size_t out_size) {
    const char *p = strrchr(path, '/');
    const char *name = p ? p + 1 : path;
    strncpy(out, name, out_size);
    out[out_size - 1] = '\0';
}

static void button_load(void)
{

	if (items.entries[selected_item].type == SDCARD_DIR) {
		append_dir(items.entries[selected_item].name);
		get_files(current_dir);
	}
	else {

		// selected file
		snprintf(selected_file_path, sizeof(selected_file_path), "%s/%s", current_dir, items.entries[selected_item].name);
		
		file_browser_file = sd_open(selected_file_path, &file_browser_file_len);
		
		if (file_browser_file == NULL) {
			sdcard_status = SD_ERR_FILE_OPEN;
			update_display = true;
		}
		else {
		
			file_browser_file_name = items.entries[selected_item].name;
			
			if (system_selected_index == 0) {
				commodore_player_main();
			}
			else if (system_selected_index == 1) {
				spectrum_player_main();
			}
			else if (system_selected_index == 2) {
				msx_player_main();
			}
		
		}
		
	}
}

static void button_item_down(void)
{
	if (items.count > 0) {
		
		if (selected_item > 0) {
			selected_item = selected_item - 1;
		}
	
		update_display = true;
	
	}

}

static void button_item_up(void)
{
	
	if (items.count > 0) {
		
		if (selected_item < items.count - 1) {
			selected_item = selected_item + 1;
		}
	
		update_display = true;
	}
}

static void button_back(void) {
	
	if (sdcard_status == SD_ERR_FILE_OPEN) {
		// open file error, go back to directory list
		get_previous_item(selected_file_path, previous_item, sizeof(previous_item));
		get_files(current_dir);
	}
	else if (sdcard_status != SD_OK || strcmp(current_dir, "/sdcard") == 0) {
		// root or sd card error, go back to main screen
		exit_mode = 1;
		file_browser_process = false;
	}
	else {
		get_previous_item(current_dir, previous_item, sizeof(previous_item));
		pop_dir();
		get_files(current_dir);
	}
}

static bool starts_with_letter(const char *name, char letter) {
    if (!name || name[0] == '\0') {
        return false;
    }
    return tolower((unsigned char)name[0]) == tolower((unsigned char)letter);
}

static bool is_upper_or_digit(char c) {
    return (isupper((unsigned char)c) || isdigit((unsigned char)c));
}

static void button_letter(char letter) {

	if (items.count > 0) {
		
		for (int i = 0; i < items.count; i++) {
			int idx = (selected_item + 1 + i) % items.count;
			if (starts_with_letter(items.entries[idx].name, letter)) { 
				selected_item = idx;
				break;
			}
		}
		
		update_display = true;
	}
	
}

static void process_keyboard(void)
{
	
	char key = keyboard_get_key();
	
	if (key == 0x86) {
        button_load();
    }
    else if (key == ';') {
		button_item_down();
	}
	 else if (key == '.') {
		button_item_up();
	}
    else if (key == 0x87) {
		button_back();
	}
	else if (is_upper_or_digit(key)) {
		button_letter(key);
	}
}

void file_browser_main(void) {

	sdcard_status = 0;
	previous_item[0] = '\0';
	
	sdcard_system_init();
	
	if (file_browser_file != NULL) {
		get_previous_item(selected_file_path, previous_item, sizeof(previous_item));
		fclose(file_browser_file);
		file_browser_file = NULL;
	}

	get_files(current_dir);
	
	exit_mode = 0;
	file_browser_process = true;
	
	while (file_browser_process) {
		process_keyboard();
		if (update_display && !display_transfer_in_progress) {
			update_display = false;
			display_screen();
		}
		vTaskDelay(pdMS_TO_TICKS(10));
		
	}
	
	if (exit_mode == 1) {
		system_main();
	}
		
}

