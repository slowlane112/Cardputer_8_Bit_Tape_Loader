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

static bool option_process = false;
static bool update_display = false;

static void display_screen(void) {
	
	for (int y = HEADER_HEIGHT; y < DISPLAY_HEIGHT; y++) {
		
		for (int x = 0; x < DISPLAY_WIDTH; x++) {
		
			framebuffer[(y * DISPLAY_WIDTH) + x] = BG_COLOR;
		}
	}
	
	draw_header("Options");
	
	int pos_x = 4;
	int pos_y = 38;
	
	graphic_display_text("Interface:", pos_y, pos_x, LABEL_COLOR, BG_COLOR);
	pos_x = pos_x + (11 * 8);
	
	bool down_active = false;
	bool up_active = false;
	
	if (is_cardputer_adv) {
		if (use_gove_port) {
			up_active = true;
		}
		else {
			down_active = true;
		}
	}
	
	graphic_display_text("<", pos_y, pos_x, down_active  ? LABEL_COLOR : DISABLED_LABEL_COLOR, BG_COLOR);
	pos_x = pos_x + (8 * 2);
	
	if (use_gove_port) {
		graphic_display_text("Grove Port", pos_y, pos_x, LABEL_COLOR, BG_COLOR);
		pos_x = pos_x + (11 * 8);
	}
	else {
		graphic_display_text("GPIO Header", pos_y, pos_x, LABEL_COLOR, BG_COLOR);	
		pos_x = pos_x + (12 * 8);	
	}
	
	graphic_display_text(">", pos_y, pos_x, up_active  ? LABEL_COLOR : DISABLED_LABEL_COLOR, BG_COLOR);
	
	display_draw();
	
}

static void button_back(void) {
	option_process = false;
}

static void button_option_up(void) {
	if (is_cardputer_adv && use_gove_port) {
		config_set_use_gove_port(false);
	}
	update_display = true;
}

static void button_option_down(void) {
	if (is_cardputer_adv && !use_gove_port) {
		config_set_use_gove_port(true);
	}
	update_display = true;	
}

static void process_keyboard(void)
{
	
	char key = keyboard_get_key();
	
	if (key == 0x87 || key == 0x86) { // Backspace or Enter
		button_back();
	}
	else if (key == '/') { // Option Up
		button_option_up();
	}
	else if (key == ',') { // Option Down
		button_option_down();
	}
    
}

void option_main(void) {
	
	option_process = true;
	update_display = true;
	
	while (option_process) {
		process_keyboard();
		if (update_display && !display_transfer_in_progress) {
			update_display = false;
			display_screen();
		}
		vTaskDelay(pdMS_TO_TICKS(10));
	}
	
	state = STATE_SYSTEM;
	
}
