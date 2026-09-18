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
#include "amstrad_tzx.h"
#include "file_browser.h"
#include "tape_buffer.h"
#include "nvs.h"
#include "state.h"
#include "config.h"

volatile bool amstrad_player_file_valid = false;
volatile uint8_t amstrad_player_data_tracker = 0;
volatile size_t amstrad_player_pos = 0;
volatile bool amstrad_player_load_buffer = false;
volatile size_t amstrad_player_buffer_overlap = 0;
volatile bool amstrad_player_display_ready = false;
volatile bool amstrad_player_process_active = false; // tape_loaded
volatile bool amstrad_player_user_tape_status = false;
volatile bool amstrad_player_tape_status = false; // playing / stopped
static uint8_t processed_data_tracker = 0;
volatile size_t amstrad_player_stop_pos = 0;
volatile bool amstrad_use_remote = true;
static volatile int amstrad_data_type = 0;
static SemaphoreHandle_t rom_done_sem = NULL;

static void load_amstrad_use_remote() {
	uint8_t use_remote = nvs_get_value("am_use_remote", 1);
	amstrad_use_remote = use_remote > 0;
}

static void save_amstrad_use_remote() {
	nvs_set_value("am_use_remote", amstrad_use_remote ? 1 : 0);
}

static bool has_data_activity() {
	
	if (amstrad_player_data_tracker != processed_data_tracker) {
		processed_data_tracker = amstrad_player_data_tracker;
		return true;
	}
	
	return false;
}

static void display_progress(void) {
	
	for (int y = HEADER_HEIGHT; y < DISPLAY_HEIGHT - FOOTER_HEIGHT; y++) {
		
		for (int x = 0; x < DISPLAY_WIDTH; x++) {
		
			framebuffer[(y * DISPLAY_WIDTH) + x] = BG_COLOR;
		}
	}
	
	draw_header((const char *)file_name_scroll((const char *)file_browser_file_name));
	
	if (amstrad_player_file_valid) {
		
		int pos_x = 4;
		int pos_y = 22;

		graphic_display_text("|tape", pos_y, pos_x, LABEL_COLOR, BG_COLOR);
		
		pos_y = 42;
		
		graphic_display_text("run\"", pos_y, pos_x, LABEL_COLOR, BG_COLOR);
		
		pos_y = 24;
		pos_x = 162;
	
		if (amstrad_use_remote) {
			graphic_draw_status_indicator("Remote", !gpio_get_level(REMOTE_PIN), pos_x, pos_y, INDICATOR_MOTOR_COLOR, INDICATOR_OFF_COLOR, true);
		}
		else {
			graphic_draw_status_indicator("Remote", false, pos_x, pos_y, INDICATOR_OFF_COLOR, INDICATOR_OFF_COLOR, true);
		}
		
		pos_y = pos_y + 24;
		
		graphic_draw_status_indicator("Data", has_data_activity(), pos_x, pos_y, INDICATOR_DATA_COLOR, INDICATOR_OFF_COLOR, false);
		
		
		size_t display_pos = amstrad_player_stop_pos == 0 ? amstrad_player_pos : amstrad_player_stop_pos;
		
		pos_x = 4;
		pos_y = 70;

		char buf_pos[32]; 
		sprintf(buf_pos, "%zu of %zu", display_pos, file_browser_file_len);
		
		graphic_display_text(buf_pos, pos_y, pos_x, LABEL_COLOR, BG_COLOR);
		
		pos_y = 90;
		
		graphic_draw_progress_bar(display_pos, file_browser_file_len, pos_x, pos_y, amstrad_player_tape_status ? PROGRESS_BAR_ON_COLOR : PROGRESS_BAR_OFF_COLOR, BG_COLOR);
		
		graphic_footer_button_t btn1 = {
			.text = "1-Play",
			.fg_color = (amstrad_player_tape_status ? BUTTON_DISABLED_LABEL_COLOR : FOOTER_LABEL_COLOR),
			.bg_color = (amstrad_player_tape_status ? BUTTON_PLAY_ON_BG_COLOR : FOOTER_BG_COLOR)
		};
		
		graphic_footer_button_t btn2 = {
			.text = "2-Stop",
			.fg_color = (amstrad_player_tape_status && !amstrad_player_user_tape_status) ? BUTTON_STOP_STOPPING_LABEL_COLOR : (amstrad_player_tape_status ? FOOTER_LABEL_COLOR : BUTTON_DISABLED_LABEL_COLOR),
			.bg_color = FOOTER_BG_COLOR
		};
		
		graphic_footer_button_t btn3 = {
			.text = "3-Reset",
			.fg_color = (amstrad_player_tape_status ? BUTTON_DISABLED_LABEL_COLOR : FOOTER_LABEL_COLOR),
			.bg_color = FOOTER_BG_COLOR
		};

		draw_footer(&btn1, &btn2, &btn3);
		
	}
	else {
		
		graphic_display_invalid_file_screen("Amstrad");
	}
	
	
	display_draw();
	
}

static void use_remote(void)
{
	amstrad_use_remote = !amstrad_use_remote;
	save_amstrad_use_remote();
}

static void process_keyboard(void)
{
	
	char key = keyboard_get_key();
	
	if (amstrad_player_file_valid) {
	
		if (key == '1') { // PLAY
			if (!amstrad_player_tape_status) {
				amstrad_player_stop_pos = 0;
				processed_data_tracker = amstrad_player_data_tracker;
				amstrad_player_user_tape_status = true;
			}
		}
		else if (key == '2') { // STOP
			if (amstrad_player_tape_status) {
				amstrad_player_stop_pos = amstrad_player_pos;
				amstrad_player_user_tape_status = false;
			}
		}
		 else if (key == '3') { // Reset
			if (amstrad_player_tape_status == false) { // tape stopped
				amstrad_player_stop_pos = 0;
				amstrad_player_pos = 0; // reset tape position
			}
		}
		else if (key == 0x87) { // Exit
			if (amstrad_player_tape_status == false) { // tape stopped
				amstrad_player_process_active = false; // exit tape
			}
		}
		else if (key == 'R') { // Use Remote
			if (amstrad_player_tape_status == false) { // tape stopped
				use_remote();
			}
		}
	
	}
	else {
		if (key == 0x87) { // Exit
			amstrad_player_process_active = false; // exit tape
		}
	}
	
}

static void main_task(void *arg)
{
	while (amstrad_player_process_active) {

		process_keyboard();

		if (amstrad_player_load_buffer) {
			amstrad_player_load_buffer = false;
			tape_buffer_load(amstrad_player_buffer_overlap);
		}

		if (amstrad_player_display_ready && !display_transfer_in_progress) {
			display_progress();
		}
		vTaskDelay(pdMS_TO_TICKS(100));
	}

	xSemaphoreGive(rom_done_sem);
	vTaskDelete(NULL);
}

static void tape_task(void *arg)
{
	uint8_t header_data[128];

	size_t header_len = sdcard_read_chunk(file_browser_file, file_browser_file_len, 0, header_data, 128);

	if (header_len > 7 && memcmp(header_data, "ZXTape!", 7) == 0) {

		amstrad_player_file_valid = true;
		amstrad_player_display_ready = true;
		
		amstrad_tzx_main();
	
	}
	else {
		amstrad_player_display_ready = true;
	}

 
	xSemaphoreGive(rom_done_sem);
	vTaskDelete(NULL);
}

void amstrad_player_main()
{
	
	amstrad_player_stop_pos = 0;
	amstrad_player_file_valid = false;
	amstrad_player_process_active = true;
	amstrad_player_display_ready = false;

	load_amstrad_use_remote();

	if (rom_done_sem == NULL) {
		rom_done_sem = xSemaphoreCreateCounting(2, 0);
	}
	
	xTaskCreateStaticPinnedToCore(
		main_task,
		"main",
		4096,
		NULL,
		2,
		mainStack,
		&mainTCB,
		0
	);

	xTaskCreateStaticPinnedToCore(
		tape_task,
		"tape",
		8192,
		NULL,
		5,
		tapeStack,
		&tapeTCB,
		1
	);
	
	xSemaphoreTake(rom_done_sem, portMAX_DELAY);
	xSemaphoreTake(rom_done_sem, portMAX_DELAY);

	state = STATE_FILE_BROWSER;

}
