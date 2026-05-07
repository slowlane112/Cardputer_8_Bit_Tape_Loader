/*
 * SPDX-FileCopyrightText: 2026 slowlane112
 *
 * SPDX-License-Identifier: Apache-2.0
 */
 
#include "freertos/FreeRTOS.h"
#include "keyboard.h"
#include "display.h"
#include "graphic.h"
#include "file_browser.h"
#include "state.h"

static bool system_process = false;
static int selected_item = 0;
static bool update_display = false;

const char *systems[] = {
    "Commodore",
    "ZX Spectrum",
    "MSX",
    "Acorn / BBC Micro",
    "Dragon / Tandy CoCo",
    "Oric",    
};

const int systems_count = sizeof(systems) / sizeof(systems[0]);
int system_selected_index = 0;

const char* system_get_name(int index)
{
    if (index < 0 || index >= systems_count) {
        return "Unknown";
    }

    return systems[index];
}

static void display_screen(void) {
	
	for (int y = HEADER_HEIGHT; y < DISPLAY_HEIGHT; y++) {
		
		for (int x = 0; x < DISPLAY_WIDTH; x++) {
		
			framebuffer[(y * DISPLAY_WIDTH) + x] = BG_COLOR;
		}
	}
	
	draw_header("8-Bit Tape Loader      v1.1.1");
	
	int pos_y = 22;
	int pos_x = 4;
	
	for (int i = 0; i < systems_count; i++) {
		
		graphic_display_text((i == selected_item) ? ">" : " ", pos_y, pos_x, LABEL_COLOR, BG_COLOR);
		
		graphic_draw_system_icon(pos_y, pos_x + 8 + 2, LABEL_COLOR, BG_COLOR);
		
		graphic_display_text(systems[i], pos_y, pos_x + (8 * 3) + 4, LABEL_COLOR, BG_COLOR);
		
		pos_y = pos_y + 19;
    }
	
	display_draw();
	
}

static void button_select(void)
{
	system_process = false;
}

static void button_item_down(void)
{
	if (systems_count > 0) {
		
		if (selected_item > 0) {
			selected_item = selected_item - 1;
		}
	
		update_display = true;
	
	}

}

static void button_item_up(void)
{
	
	if (systems_count > 0) {
		
		if (selected_item < systems_count - 1) {
			selected_item = selected_item + 1;
		}
	
		update_display = true;
	
	}

}

static void button_item_skip_start(void)
{
	if (systems_count > 0) {
		
		selected_item = 0;
	
		update_display = true;
	
	}

}

static void button_item_skip_end(void)
{
	if (systems_count > 0) {
		
		selected_item = systems_count - 1;
	
		update_display = true;
	
	}

}

static void process_keyboard(void)
{
	
	char key = keyboard_get_key();
	
	if (key == 0x86) {
        button_select();
    }
    else if (key == ';') {
		button_item_down();
	}
	else if (key == '.') {
		button_item_up();
	}
	else if (key == '`') {
		button_item_skip_start();
	}
	else if (key == 0x80) { // Ctrl
		button_item_skip_end();
	}	
    
}

void system_main(void) {
	
	system_process = true;
	update_display = true;
	
	while (system_process) {
		process_keyboard();
		if (update_display && !display_transfer_in_progress) {
			update_display = false;
			display_screen();
		}
		vTaskDelay(pdMS_TO_TICKS(10));
	}
	
	system_selected_index = selected_item;
	state = STATE_FILE_BROWSER;
	
}
