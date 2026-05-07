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
#include "display.h"
#include "graphic.h"
#include "file.h"
#include "state.h"

FILE *file_browser_file = NULL;
size_t file_browser_file_len = 0;
char file_browser_file_name[256];
static sdcard_list_t items = {0};
static int selected_item = 0;
static char current_dir[2048] = "/sdcard";
static char previous_item[256];
static char selected_file_path[2304]; 
static bool file_browser_process = false;
static bool update_display = false;
static sdcard_result_t sdcard_status = 0;
static sdcard_result_t file_sdcard_status = 0;
static int items_per_page = 6;

static int get_selected_item() {
	
	uint32_t selected_file_index = sdcard_get_index_by_filename(current_dir, previous_item);
	
	for (size_t i = 0; i < items.count; i++) {
		if (selected_file_index == items.entries[i]->file_index) {
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

	char directory_name[40];
	file_display_directory_name(current_dir, directory_name, sizeof(directory_name));
			
	if (sdcard_status == SD_OK || sdcard_status == SD_OK_PARTIAL) {
	
		int current_page = selected_item / items_per_page;
		
		int item_start = current_page * items_per_page;
		int item_end = item_start + items_per_page;
		
		if (item_end > items.count) {
			item_end = items.count;
		}
		
		int text_y_start = 2;
		int text_x_start = 4;
		

		
		if (sdcard_status == SD_OK_PARTIAL) {
			draw_header_error((const char *)directory_name);
		}
		else {
			draw_header((const char *)directory_name);
		}
		
		text_y_start = text_y_start + 20;
		
		for (size_t i = item_start; i < item_end; i++) {
			
			char item_name[30];
			snprintf(item_name, sizeof(item_name), "%s%s", ((i == selected_item) ? ">" : " "), items.entries[i]->name);

			graphic_display_text(item_name, text_y_start, text_x_start, ((items.entries[i]->type == SDCARD_DIR) ? FOLDER_COLOR : LABEL_COLOR), BG_COLOR);
			
			text_y_start = text_y_start + 19;
		}
	
	}
	else {
		draw_header((const char *)directory_name);
		graphic_display_text("SD Card Error", 30, 4, LABEL_COLOR, BG_COLOR);
		graphic_display_text("Can't Read SD Card.", 60, 4, LABEL_COLOR, BG_COLOR);
		graphic_display_text("Please insert an SD card", 80, 4, LABEL_COLOR, BG_COLOR);
		graphic_display_text("and reset device.", 100, 4, LABEL_COLOR, BG_COLOR);
	}
	
	display_draw();
	
}

static void get_files(const char *directory)
{
	sdcard_list_free(&items);
	items = sdcard_list_dir(directory);
	sdcard_status = items.status;
	
	if (sdcard_status == SD_OK || sdcard_status == SD_OK_PARTIAL) {

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

	if (items.count > 0) {
		
		graphic_display_loading_screen();
		
		sdcard_get_filename_by_index(current_dir, items.entries[selected_item]->file_index, file_browser_file_name, sizeof(file_browser_file_name));
		
		if (items.entries[selected_item]->type == SDCARD_DIR) {
			append_dir(file_browser_file_name);
			get_files(current_dir);
		}
		else {

			// selected file
			snprintf(selected_file_path, sizeof(selected_file_path), "%s/%s", current_dir, file_browser_file_name);
			
			file_browser_file = sdcard_open(selected_file_path, &file_browser_file_len);
			
			if (file_browser_file == NULL) {
				sdcard_status = SD_ERR_FILE_OPEN;
				update_display = true;
			}
			else {
			
				file_sdcard_status = sdcard_status;
				
				if (system_selected_index == 0) {
					state = STATE_PLAYER_COMMODORE;
				}
				else if (system_selected_index == 1) {
					state = STATE_PLAYER_SPECTRUM;
				}
				else if (system_selected_index == 2) {
					state = STATE_PLAYER_MSX;
				}
				else if (system_selected_index == 3) {
					state = STATE_PLAYER_ACORN;
				}
				else if (system_selected_index == 4) {
					state = STATE_PLAYER_DRAGON;
				}
				else if (system_selected_index == 5) {
					state = STATE_PLAYER_ORIC;
				}				
				
				file_browser_process = false;
				
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

static void button_item_page_down(void)
{
	if (items.count > 0) {
		
		int current_page = (int)(selected_item / (float)items_per_page);
		int new_page = current_page - 1;
		
		if (new_page < 0) {
			new_page = 0;
		}
		
		selected_item = new_page * items_per_page;
	
		update_display = true;
	
	}

}

static void button_item_page_up(void)
{
	if (items.count > 0) {
		
		int total_pages = (items.count + items_per_page - 1) / items_per_page;
		int current_page = (int)(selected_item / (float)items_per_page);
		int new_page = current_page + 1;
		
		if (new_page > total_pages - 1) {
			new_page = total_pages - 1;
		}
		
		selected_item = new_page * items_per_page;
		
		update_display = true;

	}

}

static void button_item_skip_down(void)
{
	if (items.count > 0) {
		
		int new_selected_item = selected_item - 20;
		
		if (new_selected_item < 0) {
			new_selected_item = 0;
		}
		
		selected_item = new_selected_item;
	
		update_display = true;
	
	}

}

static void button_item_skip_up(void)
{
	if (items.count > 0) {
		
		int new_selected_item = selected_item + 20;
		
		if (new_selected_item > items.count - 1) {
			new_selected_item = items.count - 1;
		}
		
		selected_item = new_selected_item;
	
		update_display = true;
	
	}

}

static void button_item_skip_start(void)
{
	if (items.count > 0) {
		
		selected_item = 0;
	
		update_display = true;
	
	}

}

static void button_item_skip_end(void)
{
	if (items.count > 0) {
		
		selected_item = items.count - 1;
	
		update_display = true;
	
	}

}

static void button_back(void) {
	
	graphic_display_loading_screen();
	
	if (sdcard_status == SD_ERR_FILE_OPEN) {
		// open file error, go back to directory list
		get_previous_item(selected_file_path, previous_item, sizeof(previous_item));
		get_files(current_dir);
	}
	else if ((sdcard_status != SD_OK && sdcard_status != SD_OK_PARTIAL) || strcmp(current_dir, "/sdcard") == 0) {
		// root or sd card error, go back to main screen
		strcpy(current_dir, "/sdcard");
		state = STATE_SYSTEM;
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
			if (starts_with_letter(items.entries[idx]->name, letter)) { 
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
	else if (key == ',') {
		button_item_page_down();
	}
	else if (key == '/') {
		button_item_page_up();
	}	
	else if (key == 0x88) { // tab
		button_item_skip_down();
	}
	else if (key == 0x85) { // fn
		button_item_skip_up();
	}
	else if (key == '`') {
		button_item_skip_start();
	}
	else if (key == 0x80) { // Ctrl
		button_item_skip_end();
	}		
    else if (key == 0x87) {
		button_back();
	}
	else if (is_upper_or_digit(key)) {
		button_letter(key);
	}
}

void return_from_file(void) {
	
	// return from file
	get_previous_item(selected_file_path, previous_item, sizeof(previous_item));
	sdcard_status = file_sdcard_status;
	selected_item = 0;
	if (previous_item[0] != '\0') {
		selected_item = get_selected_item();
		previous_item[0] = '\0';
	}
	update_display = true;
		
}

void file_browser_main(void) {

	sdcard_status = 0;
	previous_item[0] = '\0';
	
	sdcard_system_init();
	
	graphic_display_loading_screen();
	
	if (file_browser_file != NULL) {
		fclose(file_browser_file);
		file_browser_file = NULL;
		return_from_file();
	}
	else {
		get_files(current_dir);
	}
	
	file_browser_process = true;
	
	while (file_browser_process) {
		process_keyboard();
		if (update_display && !display_transfer_in_progress) {
			update_display = false;
			display_screen();
		}
		vTaskDelay(pdMS_TO_TICKS(10));
		
	}
		
}

