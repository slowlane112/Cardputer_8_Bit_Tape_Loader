/*
 * SPDX-FileCopyrightText: 2026 slowlane112
 *
 * SPDX-License-Identifier: Apache-2.0
 */
 
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "keyboard.h"
#include "sdcard.h"
#include "file.h"
#include "display.h"
#include "graphic.h"
#include "config.h"
#include "oric_tap.h"
#include "file_browser.h"
#include "tape_buffer.h"
#include "nvs.h"
#include "state.h"

volatile bool oric_player_file_valid = false;
volatile uint8_t oric_player_data_tracker = 0;
volatile size_t oric_player_pos = 0;
volatile bool oric_player_load_buffer = false;
volatile size_t oric_player_buffer_overlap = 0;
volatile bool oric_player_display_ready = false;
volatile bool oric_player_process_active = false; // tape_loaded
volatile bool oric_player_user_tape_status = false;
volatile bool oric_player_tape_status = false; // playing / stopped
static uint8_t processed_data_tracker = 0;
static size_t stop_pos = 0;
volatile bool oric_use_remote = true;
static volatile int oric_data_type = 0;
static SemaphoreHandle_t rom_done_sem = NULL;

static void load_oric_use_remote() {
	uint8_t use_remote = nvs_get_value("oc_use_remote", 1);
    oric_use_remote = use_remote > 0;
}

static void save_oric_use_remote() {
	nvs_set_value("oc_use_remote", oric_use_remote ? 1 : 0);
}

static bool has_data_activity() {
	
	if (oric_player_data_tracker != processed_data_tracker) {
		processed_data_tracker = oric_player_data_tracker;
		return true;
	}
	
	return false;
}

static bool tap_valid(const uint8_t *buf, size_t len) {
    if (len < 4) return false;
    return (buf[0] == 0x16 && buf[1] == 0x16 && buf[2] == 0x16 && buf[3] == 0x24);
}

const char *oric_data_types[] = {
    "Unknown",
    "Basic",
    "Binary"
};

const char *oric_load_commands[] = {
    "",
    "CLOAD\"\"",
    "CLOAD\"\""
};

static int get_oric_data_type(const uint8_t *buf, size_t len) {
	
    if (len < 7) return 0; // Unknown
    
    if (buf[6] == 0x00) {
        return 1; // Basic
    } else if (buf[6] == 0x80) {
        return 2; // Binary
    }
    
    return 0; // Unknown
}


static void display_progress(void) {
	
	for (int y = HEADER_HEIGHT; y < DISPLAY_HEIGHT - FOOTER_HEIGHT; y++) {
		
		for (int x = 0; x < DISPLAY_WIDTH; x++) {
		
			framebuffer[(y * DISPLAY_WIDTH) + x] = BG_COLOR;
		}
	}
	
	draw_header((const char *)file_name_scroll((const char *)file_browser_file_name));
	
	
	if (oric_player_file_valid) {
	
		int pos_x = 4;
		int pos_y = 22;

		char buf_type[32]; 
		sprintf(buf_type, "Type: %s", oric_data_types[oric_data_type]);
		
		graphic_display_text(buf_type, pos_y, pos_x, LABEL_COLOR, BG_COLOR);
		
		pos_y = 42;
		
		char buf_command[32]; 
		sprintf(buf_command, "%s", oric_load_commands[oric_data_type]);
		
		graphic_display_text(buf_command, pos_y, pos_x, LABEL_COLOR, BG_COLOR);
		
		pos_y = 24;
		pos_x = 162;
		
		
		if (oric_use_remote) {
			graphic_draw_status_indicator("Remote", !gpio_get_level(REMOTE_PIN), pos_x, pos_y, INDICATOR_MOTOR_COLOR, INDICATOR_OFF_COLOR);
		}
		else {
			graphic_draw_status_indicator("Remote", false, pos_x, pos_y, INDICATOR_OFF_COLOR, INDICATOR_OFF_COLOR);
		}
		
		pos_y = pos_y + 24;
		
		graphic_draw_status_indicator("Data", has_data_activity(), pos_x, pos_y, INDICATOR_DATA_COLOR, INDICATOR_OFF_COLOR);
		
		size_t display_pos = stop_pos == 0 ? oric_player_pos : stop_pos;
		
		pos_x = 4;
		pos_y = 70;

		char buf_pos[32]; 
		sprintf(buf_pos, "%zu of %zu", display_pos, file_browser_file_len);
		
		graphic_display_text(buf_pos, pos_y, pos_x, LABEL_COLOR, BG_COLOR);
		
		pos_y = 90;
		
		graphic_draw_progress_bar(display_pos, file_browser_file_len, pos_x, pos_y, oric_player_tape_status ? PROGRESS_BAR_ON_COLOR : PROGRESS_BAR_OFF_COLOR, BG_COLOR);
		
		graphic_footer_button_t btn1 = {
			.text = "1-Play",
			.fg_color = (oric_player_tape_status ? BUTTON_DISABLED_LABEL_COLOR : FOOTER_LABEL_COLOR),
			.bg_color = (oric_player_tape_status ? BUTTON_PLAY_ON_BG_COLOR : FOOTER_BG_COLOR)
		};
		
		graphic_footer_button_t btn2 = {
			.text = "2-Stop",
			.fg_color = (oric_player_tape_status && !oric_player_user_tape_status) ? BUTTON_STOP_STOPPING_LABEL_COLOR : (oric_player_tape_status ? FOOTER_LABEL_COLOR : BUTTON_DISABLED_LABEL_COLOR),
			.bg_color = FOOTER_BG_COLOR
		};
		
		graphic_footer_button_t btn3 = {
			.text = "3-Reset",
			.fg_color = (oric_player_tape_status ? BUTTON_DISABLED_LABEL_COLOR : FOOTER_LABEL_COLOR),
			.bg_color = FOOTER_BG_COLOR
		};

		draw_footer(&btn1, &btn2, &btn3);
		
	}
	else {
		
		graphic_display_invalid_file_screen("Oric");
	}
	
	display_draw();
	
}

static void use_remote(void)
{
	oric_use_remote = !oric_use_remote;
	save_oric_use_remote();
}

static void process_keyboard(void)
{
	
	char key = keyboard_get_key();
	
	if (oric_player_file_valid) {
	
		if (key == '1') { // PLAY
			if (!oric_player_tape_status) {
				stop_pos = 0;
				processed_data_tracker = oric_player_data_tracker;
				oric_player_user_tape_status = true;
			}
		}
		else if (key == '2') { // STOP
			if (oric_player_tape_status) {
				stop_pos = oric_player_pos;
				oric_player_user_tape_status = false;
			}
		}
		 else if (key == '3') { // Reset
			if (oric_player_tape_status == false) { // tape stopped
				stop_pos = 0;
				oric_player_pos = 0; // reset tape position
			}
		}
		else if (key == 0x87) { // Exit
			if (oric_player_tape_status == false) { // tape stopped
				oric_player_process_active = false; // exit tape
			}
		}
		else if (key == 'R') { // Use Remote
			if (oric_player_tape_status == false) { // tape stopped
				use_remote();
			}
		}
	
	}
	else {
		if (key == 0x87) { // Exit
			oric_player_process_active = false; // exit tape
		}
	}
	
}

static void main_task(void *arg)
{
    while (oric_player_process_active) {
		
        process_keyboard();
        
        if (oric_player_load_buffer) {
			oric_player_load_buffer = false;
			tape_buffer_load(oric_player_buffer_overlap);
		}
        
        if (oric_player_display_ready && !display_transfer_in_progress) {
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

	if (tap_valid(header_data, header_len)) {

		oric_data_type = get_oric_data_type(header_data,  header_len);

		oric_player_file_valid = true;
		oric_player_display_ready = true;
		
		oric_tap_main();
	
	}
	else {
		oric_player_display_ready = true;
	}

 
    xSemaphoreGive(rom_done_sem);
    vTaskDelete(NULL);
}

void oric_player_main()
{
	
	stop_pos = 0;
	oric_player_file_valid = false;
    oric_player_process_active = true;
    oric_player_display_ready = false;
    
    load_oric_use_remote();
    
    if (rom_done_sem == NULL)
        rom_done_sem = xSemaphoreCreateCounting(2, 0);
        
    xTaskCreatePinnedToCore(main_task, "main", 4096, NULL, 2, NULL, 0);
	xTaskCreatePinnedToCore(tape_task, "tape", 8192, NULL, 5, NULL, 1);
	
    xSemaphoreTake(rom_done_sem, portMAX_DELAY);
    xSemaphoreTake(rom_done_sem, portMAX_DELAY);
    
	state = STATE_FILE_BROWSER;
    
}
