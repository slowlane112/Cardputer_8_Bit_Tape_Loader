/*
 * SPDX-FileCopyrightText: 2026 slowlane112
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h> 
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "keyboard.h"
#include "sdcard.h"
#include "file.h"
#include "display.h"
#include "graphic.h"
#include "config.h"
#include "acorn_uef.h"
#include "file_browser.h"
#include "tape_buffer.h"
#include "nvs.h"

typedef enum {
    LOAD_CHAIN,
    LOAD_RUN
} acorn_load_method_t;

volatile bool acorn_player_file_valid = false;
volatile uint8_t acorn_player_data_tracker = 0;
volatile size_t acorn_player_pos = 0;
volatile bool acorn_player_load_buffer = false;
volatile size_t acorn_player_buffer_overlap = 0;
volatile bool acorn_player_display_ready = false;
volatile bool acorn_player_process_active = false; // tape_loaded
volatile bool acorn_player_user_tape_status = false;
volatile bool acorn_player_tape_status = false; // playing / stopped
static uint8_t processed_data_tracker = 0;
static size_t stop_pos = 0;
volatile bool acorn_use_remote = true;
static volatile acorn_load_method_t acorn_load_method = LOAD_CHAIN;
static SemaphoreHandle_t rom_done_sem = NULL;

static void load_acorn_use_remote() {
	uint8_t use_remote = nvs_get_value("an_use_remote", 1);
    acorn_use_remote = use_remote > 0;
}

static void save_acorn_use_remote() {
	nvs_set_value("an_use_remote", acorn_use_remote ? 1 : 0);
}

static bool has_data_activity() {
	
	if (acorn_player_data_tracker != processed_data_tracker) {
		processed_data_tracker = acorn_player_data_tracker;
		return true;
	}
	
	return false;
}

static bool uef_valid(const uint8_t *buf, size_t len) {
	
    const char magic[] = "UEF File!";
    const size_t magic_len = sizeof(magic) - 1;

    if (len < magic_len)
        return false;

    return memcmp(buf, magic, magic_len) == 0;
}

static acorn_load_method_t get_acorn_load_method()
{
	// use filename for now
		
	if (strcasestr(file_browser_file_name, "_RUN_") != NULL) {
		return LOAD_RUN;
	}
	
	return LOAD_CHAIN;
		
}

static void display_progress(void) {
	
	for (int y = HEADER_HEIGHT; y < DISPLAY_HEIGHT - FOOTER_HEIGHT; y++) {
		
		for (int x = 0; x < DISPLAY_WIDTH; x++) {
		
			framebuffer[(y * DISPLAY_WIDTH) + x] = BG_COLOR;
		}
	}
	
	draw_header((const char *)file_name_scroll((const char *)file_browser_file_name));
	
	if (acorn_player_file_valid) {
	
		int pos_x = 4;
		int pos_y = 22;

		char buf_type[32]; 
		sprintf(buf_type, "*TAPE");
		
		graphic_display_text(buf_type, pos_y, pos_x, LABEL_COLOR, BG_COLOR);
		
		pos_y = 42;
		
		char buf_command[32];
		if (acorn_load_method == LOAD_CHAIN) {
			sprintf(buf_command, "*CHAIN\"\"");
		} else if  (acorn_load_method == LOAD_RUN) {
			sprintf(buf_command, "*RUN");
		} else {
			sprintf(buf_command, "*CHAIN\"\" or *RUN");
		}
		
		graphic_display_text(buf_command, pos_y, pos_x, LABEL_COLOR, BG_COLOR);
		
		pos_y = 24;
		pos_x = 162;
		
		if (acorn_use_remote) {
			graphic_draw_status_indicator("Remote", !gpio_get_level(REMOTE_PIN), pos_x, pos_y, INDICATOR_MOTOR_COLOR, INDICATOR_OFF_COLOR);
		}
		else {
			graphic_draw_status_indicator("Remote", false, pos_x, pos_y, INDICATOR_OFF_COLOR, INDICATOR_OFF_COLOR);
		}
		
		pos_y = pos_y + 24;
		
		graphic_draw_status_indicator("Data", has_data_activity(), pos_x, pos_y, INDICATOR_DATA_COLOR, INDICATOR_OFF_COLOR);
		
		size_t display_pos = stop_pos == 0 ? acorn_player_pos : stop_pos;
		
		pos_x = 4;
		pos_y = 70;

		char buf_pos[32]; 
		sprintf(buf_pos, "%zu of %zu", display_pos, file_browser_file_len);
		
		graphic_display_text(buf_pos, pos_y, pos_x, LABEL_COLOR, BG_COLOR);
		
		pos_y = 90;
		
		graphic_draw_progress_bar(display_pos, file_browser_file_len, pos_x, pos_y, acorn_player_tape_status ? PROGRESS_BAR_ON_COLOR : PROGRESS_BAR_OFF_COLOR, BG_COLOR);
		
		graphic_footer_button_t btn1 = {
			.text = "1-Play",
			.fg_color = (acorn_player_tape_status ? BUTTON_DISABLED_LABEL_COLOR : FOOTER_LABEL_COLOR),
			.bg_color = (acorn_player_tape_status ? BUTTON_PLAY_ON_BG_COLOR : FOOTER_BG_COLOR)
		};
		
		graphic_footer_button_t btn2 = {
			.text = "2-Stop",
			.fg_color = (acorn_player_tape_status && !acorn_player_user_tape_status) ? BUTTON_STOP_STOPPING_LABEL_COLOR : (acorn_player_tape_status ? FOOTER_LABEL_COLOR : BUTTON_DISABLED_LABEL_COLOR),
			.bg_color = FOOTER_BG_COLOR
		};
		
		graphic_footer_button_t btn3 = {
			.text = "3-Reset",
			.fg_color = (acorn_player_tape_status ? BUTTON_DISABLED_LABEL_COLOR : FOOTER_LABEL_COLOR),
			.bg_color = FOOTER_BG_COLOR
		};

		draw_footer(&btn1, &btn2, &btn3);
		
	}
	else {
		
		graphic_display_invalid_file_screen("MSX");
	}
	
	
	display_draw();
	
}

static void use_remote(void)
{
	acorn_use_remote = !acorn_use_remote;
	save_acorn_use_remote();
}

static void process_keyboard(void)
{
	
	char key = keyboard_get_key();
	
	if (acorn_player_file_valid) {
	
		if (key == '1') { // PLAY
			if (!acorn_player_tape_status) {
				stop_pos = 0;
				processed_data_tracker = acorn_player_data_tracker;
				acorn_player_user_tape_status = true;
			}
		}
		else if (key == '2') { // STOP
			if (acorn_player_tape_status) {
				stop_pos = acorn_player_pos;
				acorn_player_user_tape_status = false;
			}
		}
		 else if (key == '3') { // Reset
			if (acorn_player_tape_status == false) { // tape stopped
				stop_pos = 0;
				acorn_player_pos = 0; // reset tape position
			}
		}
		else if (key == 0x87) { // Exit
			if (acorn_player_tape_status == false) { // tape stopped
				acorn_player_process_active = false; // exit tape
			}
		}
		else if (key == 'R') { // Use Remote
			if (acorn_player_tape_status == false) { // tape stopped
				use_remote();
			}
		}
	
	}
	else {
		if (key == 0x87) { // Exit
			acorn_player_process_active = false; // exit tape
		}
	}
	
}

static void main_task(void *arg)
{
    while (acorn_player_process_active) {
		
        process_keyboard();
        
        if (acorn_player_load_buffer) {
			acorn_player_load_buffer = false;
			tape_buffer_load(acorn_player_buffer_overlap);
		}
        
        if (acorn_player_display_ready && !display_transfer_in_progress) {
			display_progress();
		}
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    xSemaphoreGive(rom_done_sem);
    vTaskDelete(NULL);
}

static void tape_task(void *arg)
{
	uint8_t header_data[256];

	size_t header_len = sd_read_chunk(file_browser_file, file_browser_file_len, 0, header_data, 256);

	if (uef_valid(header_data, header_len)) {

		acorn_load_method = get_acorn_load_method();

		acorn_player_file_valid = true;
		acorn_player_display_ready = true;
		
		acorn_uef_main();
	
	}
	else {
		acorn_player_display_ready = true;
	}

 
    xSemaphoreGive(rom_done_sem);
    vTaskDelete(NULL);
}

void acorn_player_main()
{
	
	stop_pos = 0;
	acorn_player_file_valid = false;
    acorn_player_process_active = true;
    acorn_player_display_ready = false;
    
    load_acorn_use_remote();
    
    if (rom_done_sem == NULL)
        rom_done_sem = xSemaphoreCreateCounting(2, 0);
        
    xTaskCreatePinnedToCore(main_task, "main", 4096, NULL, 2, NULL, 0);
	xTaskCreatePinnedToCore(tape_task, "tape", 8192, NULL, 5, NULL, 1);
	
    xSemaphoreTake(rom_done_sem, portMAX_DELAY);
    xSemaphoreTake(rom_done_sem, portMAX_DELAY);
    
    file_browser_main();
    
}
