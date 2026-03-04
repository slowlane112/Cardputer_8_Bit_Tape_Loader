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
#include "msx_cas.h"
#include "file_browser.h"
#include "tape_buffer.h"

volatile bool msx_player_file_valid = false;
volatile uint8_t msx_player_data_tracker = 0;
volatile size_t msx_player_pos = 0;
volatile bool msx_player_load_buffer = false;
volatile size_t msx_player_buffer_overlap = 0;
volatile bool msx_player_display_ready = false;
volatile bool msx_player_process_active = false; // tape_loaded
volatile bool msx_player_user_tape_status = false;
volatile bool msx_player_tape_status = false; // playing / stopped
static uint8_t processed_data_tracker = 0;
static size_t stop_pos = 0;
static volatile int msx_data_type = 0;
static SemaphoreHandle_t rom_done_sem = NULL;

static const uint8_t MAGIC_ID[8] = {
    0x1F, 0xA6, 0xDE, 0xBA, 0xCC, 0x13, 0x7D, 0x74
};

static bool has_data_activity() {
	
	if (msx_player_data_tracker != processed_data_tracker) {
		processed_data_tracker = msx_player_data_tracker;
		return true;
	}
	
	return false;
}

static bool cas_valid(const uint8_t *buf, size_t len) {
	
    if (len < 8) return false;

    for (size_t i = 0; i <= len - 8; i++) {
        if (memcmp(buf + i, MAGIC_ID, 8) == 0)
            return true;
    }

    return false;
}

const char *msx_data_types[] = {
    "Unknown",
    "Basic",
    "Binary",
    "ASCII"
};

const char *msx_load_commands[] = {
    "",
    "RUN\"CAS:\"",
    "BLOAD\"CAS:\",R",
    "CLOAD + RUN"
};

static int get_msx_data_type(const uint8_t *buf, size_t len)
{
    if (len < 9)
        return 0;

    for (size_t i = 0; i + 8 < len; i++) {

        if (memcmp(buf + i, MAGIC_ID, 8) == 0) {

            if (i + 8 >= len)
                return 0;

            uint8_t id = buf[i + 8];

            switch (id) {
                case 0xEA: return 1;   // Basic
                case 0xD0: return 2;   // Binary
                case 0xD3: return 3;   // ASCII
                default:   return 0;   // Unknown
            }
        }
    }

    return 0;
}

static void display_progress(void) {
	
	for (int y = HEADER_HEIGHT; y < DISPLAY_HEIGHT - FOOTER_HEIGHT; y++) {
		
		for (int x = 0; x < DISPLAY_WIDTH; x++) {
		
			framebuffer[(y * DISPLAY_WIDTH) + x] = BG_COLOR;
		}
	}
	
	draw_header((const char *)file_name_scroll((const char *)file_browser_file_name));
	
	
	if (msx_player_file_valid) {
	
		int pos_x = 4;
		int pos_y = 22;

		char buf_type[32]; 
		sprintf(buf_type, "Type: %s", msx_data_types[msx_data_type]);
		
		graphic_display_text(buf_type, pos_y, pos_x, LABEL_COLOR, BG_COLOR);
		
		pos_y = 42;
		
		char buf_command[32]; 
		sprintf(buf_command, "%s", msx_load_commands[msx_data_type]);
		
		graphic_display_text(buf_command, pos_y, pos_x, LABEL_COLOR, BG_COLOR);
		
		pos_y = 24;
		pos_x = 162;
		
		graphic_draw_status_indicator("Remote", !gpio_get_level(REMOTE_PIN), pos_x, pos_y, INDICATOR_MOTOR_COLOR, INDICATOR_OFF_COLOR);
		
		pos_y = pos_y + 24;
		
		graphic_draw_status_indicator("Data", has_data_activity(), pos_x, pos_y, INDICATOR_DATA_COLOR, INDICATOR_OFF_COLOR);
		
		size_t display_pos = stop_pos == 0 ? msx_player_pos : stop_pos;
		
		pos_x = 4;
		pos_y = 70;

		char buf_pos[32]; 
		sprintf(buf_pos, "%zu of %zu", display_pos, file_browser_file_len);
		
		graphic_display_text(buf_pos, pos_y, pos_x, LABEL_COLOR, BG_COLOR);
		
		pos_y = 90;
		
		graphic_draw_progress_bar(display_pos, file_browser_file_len, pos_x, pos_y, msx_player_tape_status ? PROGRESS_BAR_ON_COLOR : PROGRESS_BAR_OFF_COLOR, BG_COLOR);
		
		graphic_footer_button_t btn1 = {
			.text = "1-Play",
			.fg_color = (msx_player_tape_status ? BUTTON_DISABLED_LABEL_COLOR : FOOTER_LABEL_COLOR),
			.bg_color = (msx_player_tape_status ? BUTTON_PLAY_ON_BG_COLOR : FOOTER_BG_COLOR)
		};
		
		graphic_footer_button_t btn2 = {
			.text = "2-Stop",
			.fg_color = (msx_player_tape_status && !msx_player_user_tape_status) ? BUTTON_STOP_STOPPING_LABEL_COLOR : (msx_player_tape_status ? FOOTER_LABEL_COLOR : BUTTON_DISABLED_LABEL_COLOR),
			.bg_color = FOOTER_BG_COLOR
		};
		
		graphic_footer_button_t btn3 = {
			.text = "3-Reset",
			.fg_color = (msx_player_tape_status ? BUTTON_DISABLED_LABEL_COLOR : FOOTER_LABEL_COLOR),
			.bg_color = FOOTER_BG_COLOR
		};

		draw_footer(&btn1, &btn2, &btn3);
		
	}
	else {
		
		graphic_display_invalid_file_screen("MSX");
	}
	
	
	display_draw();
	
}

static void process_keyboard(void)
{
	
	char key = keyboard_get_key();
	
	if (msx_player_file_valid) {
	
		if (key == '1') { // PLAY
			if (!msx_player_tape_status) {
				stop_pos = 0;
				processed_data_tracker = msx_player_data_tracker;
				msx_player_user_tape_status = true;
			}
		}
		else if (key == '2') { // STOP
			if (msx_player_tape_status) {
				stop_pos = msx_player_pos;
				msx_player_user_tape_status = false;
			}
		}
		 else if (key == '3') { // Reset
			if (msx_player_tape_status == false) { // tape stopped
				stop_pos = 0;
				msx_player_pos = 0; // reset tape position
			}
		}
		else if (key == 0x87) { // Exit
			if (msx_player_tape_status == false) { // tape stopped
				msx_player_process_active = false; // exit tape
			}
		}
	
	}
	else {
		if (key == 0x87) { // Exit
			msx_player_process_active = false; // exit tape
		}
	}
	
}

static void main_task(void *arg)
{
    while (msx_player_process_active) {
		
        process_keyboard();
        
        if (msx_player_load_buffer) {
			msx_player_load_buffer = false;
			tape_buffer_load(msx_player_buffer_overlap);
		}
        
        if (msx_player_display_ready && !display_transfer_in_progress) {
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

	size_t header_len = sd_read_chunk(file_browser_file, file_browser_file_len, 0, header_data, 128);

	if (cas_valid(header_data, header_len)) {

		msx_data_type = get_msx_data_type(header_data,  header_len);

		msx_player_file_valid = true;
		msx_player_display_ready = true;
		
		msx_cas_main();
	
	}
	else {
		msx_player_display_ready = true;
	}

 
    xSemaphoreGive(rom_done_sem);
    vTaskDelete(NULL);
}

void msx_player_main()
{
	
	stop_pos = 0;
	msx_player_file_valid = false;
    msx_player_process_active = true;
    msx_player_display_ready = false;
    
    if (rom_done_sem == NULL)
        rom_done_sem = xSemaphoreCreateCounting(2, 0);
        
    xTaskCreatePinnedToCore(main_task, "main", 4096, NULL, 2, NULL, 0);
	xTaskCreatePinnedToCore(tape_task, "tape", 8192, NULL, 5, NULL, 1);
	
    xSemaphoreTake(rom_done_sem, portMAX_DELAY);
    xSemaphoreTake(rom_done_sem, portMAX_DELAY);
    
    file_browser_main();
    
}
