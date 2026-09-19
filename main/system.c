/*
 * SPDX-FileCopyrightText: 2026 slowlane112
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h> 
#include "freertos/FreeRTOS.h"
#include "keyboard.h"
#include "display.h"
#include "graphic.h"
#include "file_browser.h"
#include "state.h"
#include "config.h"
#include "battery.h"

static bool system_process = false;
static int selected_item = 0;
static bool update_display = false;
static bool display_help_message = false;
static uint8_t battery_level = 0;
static int items_per_page = 6;

const char *systems[] = {
	"Commodore",
	"ZX Spectrum",
	"MSX",
	"Acorn / BBC Micro",
	"Dragon / Tandy CoCo",
	"Oric",
	"Amstrad"
};
const int systems_count = sizeof(systems) / sizeof(systems[0]);
const char **display_systems;
int display_systems_count = 0;

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
	
	if (display_help_message) {
		draw_header("8-Bit Tape Loader        Help");
		
		int line_start = (DISPLAY_WIDTH * 17) + 204;
		for (int i = line_start ; i < line_start + 8; i++) {
			framebuffer[i] = LABEL_COLOR;
		}
		
	}
	else {
		draw_header("8-Bit Tape Loader v1.2.2");
		graphic_draw_battery_level(battery_level);
	}
	
	int current_page = selected_item / items_per_page;
		
	int item_start = current_page * items_per_page;
	int item_end = item_start + items_per_page;
	
	if (item_end > display_systems_count) {
		item_end = display_systems_count;
	}
	
	int pos_y = 22;
	int pos_x = 4;

	for (int i = item_start; i < item_end; i++) {
		
		graphic_display_text((i == selected_item) ? ">" : " ", pos_y - 2, pos_x, LABEL_COLOR, BG_COLOR);
			
		graphic_draw_system_icon(pos_y, pos_x + 8 + 2, LABEL_COLOR, BG_COLOR);
		
		graphic_display_text(display_systems[i], pos_y, pos_x + (8 * 3) + 4, LABEL_COLOR, BG_COLOR);
		
		pos_y = pos_y + 19;
	
	}
	
	display_draw();
	
}

static int get_system_selected_index() {

	return use_gove_port ? selected_item + 1 : selected_item;
}

static void button_select(void)
{
	system_selected_index = get_system_selected_index();
	state = STATE_FILE_BROWSER;
	system_process = false;
}

static void button_option(void)
{
	selected_item = 0;
	state = STATE_OPTION;
	system_process = false;
}

static void button_help(void)
{
	state = STATE_HELP;
	system_process = false;
}

static void button_item_down(void)
{
	if (display_systems_count > 0) {
		
		if (selected_item > 0) {
			selected_item = selected_item - 1;
		}
		else {
			selected_item = display_systems_count - 1;
		}
	
		update_display = true;

	}

}

static void button_item_up(void)
{
	
	if (systems_count > 0) {
		
		if (selected_item < display_systems_count - 1) {
			selected_item = selected_item + 1;
		}
		else {
			selected_item = 0;
		}
	
		update_display = true;
	
	}

}

static void button_item_skip_start(void)
{
	if (display_systems_count > 0) {
		
		selected_item = 0;
	
		update_display = true;
	
	}

}

static void button_item_skip_end(void)
{
	if (display_systems_count > 0) {
		
		selected_item = display_systems_count - 1;
	
		update_display = true;
	
	}

}

static void process_keyboard(void)
{
	
	char key = keyboard_get_key();
	
	if (key == 0x86) {
		button_select();
	}
	else if (key == 0x82) { // opt
		button_option();
	}
	else if (key == 'H') { // help
		button_help();
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
	
	if (use_gove_port) { 
		display_systems = systems + 1;
		display_systems_count = systems_count - 1; 
	} else {
		display_systems = systems;
		display_systems_count = systems_count;
	}
	
	int timer_ticks = 0;
	display_help_message = false;
	system_process = true;
	update_display = true;
	
	battery_level = battery_get_level();
	
	while (system_process) {
		process_keyboard();
		if (update_display && !display_transfer_in_progress) {
			update_display = false;
			display_screen();
		}
		
		if (!display_help_message) {
			timer_ticks++;
			if (timer_ticks >= 500) {
				display_help_message = true;
				update_display = true;
			}
		}
		
		vTaskDelay(pdMS_TO_TICKS(10));
	}
	

	
}