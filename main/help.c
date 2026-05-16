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

static bool help_process = false;
static bool update_display = false;
static int help_pos = 0;
static const char * const help_content[] = {
	"#Options",
	"From the main screen, use the",
	"opt button to access the",
	"options menu.",
	"",
	"You can select between Grove",
	"Port or GPIO Header for the",
	"tape player interface.",
	"",
    "#File Browser",
    "Browse and select tape files",
    "from the SD card.",
    "",    
    "Arrow Up: Move up",
    "Arrow Down: Move down",
    "Arrow Left: Previous page",
    "Arrow Right: Next page",
    "Enter: Select",
    "Backspace: Back",
    "Letter: Move to letter",
    "Esc: First item",
    "Tab: Up 20 items",
    "Fn: Down 20 items",
    "Ctrl: Last item",
	"",
    "#Tape Player",
    "Tape files must be",
    "uncompressed tape images.",
    "",      
    "1: Play",
    "2: Stop",
    "3: Reset",
    "Backspace: Exit",
    "",    
    "#Commodore",
    "Only available when using",
    "GPIO Header for interface.",
	"Supports tap files.",
	"M: Enable/disable motor.",
	"",
	"#ZX Spectrum",
	"Supports tap and tzx files.",
	"Arrow Left: 48K mode",
	"Arrow Right: 128K mode",
    "",
	"#MSX",
	"Supports cas files.",
	"R: Enable/disable remote.",
	"",
	"#Acorn and BBC Micro",
	"Supports uef and hq files.",
	"R: Enable/disable remote.",
    "", 	
	"#Dragon and Tandy CoCo",
	"Supports cas files.",
	"R: Enable/disable remote.",
	"", 
	"#Oric",
	"Supports tap files.",
	"R: Enable/disable remote.",
	"",
	"#Project",
	"https://github.com/slowlane11",
	"2/Cardputer_8_Bit_Tape_Loader"
};

const int help_content_count = sizeof(help_content) / sizeof(help_content[0]);

static void display_screen(void) {
	
	for (int y = HEADER_HEIGHT; y < DISPLAY_HEIGHT; y++) {
		
		for (int x = 0; x < DISPLAY_WIDTH; x++) {
		
			framebuffer[(y * DISPLAY_WIDTH) + x] = BG_COLOR;
		}
	}
	
	draw_header("Help");
	
	int pos_x = 4;
	int pos_y = 22;
	
	int item_count = 0;
	
	for (int i = help_pos; i < help_content_count; i++) {
		
		item_count++;
		
		if (help_content[i] && help_content[i][0] == '#') {
			graphic_display_text_underline(help_content[i] + 1, pos_y, pos_x, LABEL_COLOR, BG_COLOR);
		}
		else {
			graphic_display_text(help_content[i], pos_y, pos_x, LABEL_COLOR, BG_COLOR);
		}
		
		pos_y = pos_y + 19;
		
		if (item_count == 6) {
			break;
		}
			
    }
    
    float ratio = (float)(help_pos + 1) / help_content_count;
	int scroll_pos = HEADER_HEIGHT + (int)((DISPLAY_HEIGHT - HEADER_HEIGHT - 2) * ratio);
	
	for (int i = scroll_pos ; i < scroll_pos + 14; i++) {
		framebuffer[(DISPLAY_WIDTH  * i) - 2] = DISABLED_LABEL_COLOR;
		framebuffer[(DISPLAY_WIDTH  * i) - 1] = DISABLED_LABEL_COLOR;
	}

	display_draw();
	
}

static void button_back(void) {
	help_process = false;
}

static void button_help_up(void) {
	if (help_pos > 0) {
		help_pos--;
	}
	update_display = true;	
}

static void button_help_down(void) {
	
	if (help_pos < help_content_count - 6) {
		help_pos++;
	}
	update_display = true;	
}

static void button_help_page_up(void) {
	int temp = help_pos - 6;
	if (temp < 0) {
		temp = 0;
	}
	help_pos = temp;
	update_display = true;	
}

static void button_help_page_down(void) {
	int temp = help_pos + 6;
	if (temp > help_content_count - 6) {
		temp = help_content_count - 6;
	}
	help_pos = temp;
	update_display = true;	
}

static void process_keyboard(void)
{
	
	char key = keyboard_get_key();
	
	if (key == 0x87 || key == 0x86) { // Backspace or Enter
		button_back();
	}
	else if (key == ';') { // Help Up
		button_help_up();
	}
	else if (key == '.') { // Help Down
		button_help_down();
	}
    	else if (key == ',') { // Help Page Up
		button_help_page_up();
	}
	else if (key == '/') { // Help Page Down
		button_help_page_down();
	}	
	
}

void help_main(void) {
	
	help_process = true;
	update_display = true;
	help_pos = 0;
	
	while (help_process) {
		process_keyboard();
		if (update_display && !display_transfer_in_progress) {
			update_display = false;
			display_screen();
		}
		vTaskDelay(pdMS_TO_TICKS(10));
	}
	
	state = STATE_SYSTEM;
	
}
