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
#include "dragon_cas.h"
#include "file_browser.h"
#include "tape_buffer.h"
#include "nvs.h"
#include "state.h"

volatile bool dragon_player_file_valid = false;
volatile uint8_t dragon_player_data_tracker = 0;
volatile size_t dragon_player_pos = 0;
volatile bool dragon_player_load_buffer = false;
volatile size_t dragon_player_buffer_overlap = 0;
volatile bool dragon_player_display_ready = false;
volatile bool dragon_player_process_active = false; // tape_loaded
volatile bool dragon_player_user_tape_status = false;
volatile bool dragon_player_tape_status = false; // playing / stopped
static uint8_t processed_data_tracker = 0;
static size_t stop_pos = 0;
volatile bool dragon_use_remote = true;
static volatile int dragon_data_type = 0;
static SemaphoreHandle_t rom_done_sem = NULL;

static void load_dragon_use_remote() {
	uint8_t use_remote = nvs_get_value("dg_use_remote", 1);
    dragon_use_remote = use_remote > 0;
}

static void save_dragon_use_remote() {
	nvs_set_value("dg_use_remote", dragon_use_remote ? 1 : 0);
}

static bool has_data_activity() {
	
	if (dragon_player_data_tracker != processed_data_tracker) {
		processed_data_tracker = dragon_player_data_tracker;
		return true;
	}
	
	return false;
}

static bool cas_valid(const uint8_t *buf, size_t len) {
    
    if (buf == NULL || len < 3) {
        return false;
    }

    if (buf[0] != 0x55) {
        return false;
    }

    for (size_t i = 0; i <= len - 3; i++) {

        if (buf[i] == 0x55 && buf[i + 1] == 0x3C) {
            uint8_t blockType = buf[i + 2];
            
            if (blockType == 0x00 || blockType == 0x01) {
                return true;
            }
        }
    }

    return false;
}

const char *dragon_data_types[] = {
    "Unknown",
    "Basic",
    "Binary",
    "ASCII"
};

const char *dragon_load_commands[] = {
    "",
    "CLOAD\"\" + RUN",
    "CLOADM\"\" + EXEC",
    "CLOAD\"\" + RUN"
};

static int get_dragon_data_type(const uint8_t *buf, size_t len)
{
    if (len < 20) return 0; 

    for (size_t i = 1; i < len; i++) {
        
        if (buf[i] == 0x3C && buf[i - 1] == 0x55) {
            
            if (i + 18 < len) {
                
                if (buf[i + 1] == 0x00) {
                    
                    uint8_t type_byte = buf[i + 11];

                    switch (type_byte) {
                        case 0x00: return 1; // Basic
                        case 0x01: return 3; // ASCII
                        case 0x02: return 2; // Binary
                        default:   return 0; 
                    }
                }
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
	
	if (dragon_player_file_valid) {
	
		int pos_x = 4;
		int pos_y = 22;

		char buf_type[32]; 
		sprintf(buf_type, "Type: %s", dragon_data_types[dragon_data_type]);
		
		graphic_display_text(buf_type, pos_y, pos_x, LABEL_COLOR, BG_COLOR);
		
		pos_y = 42;
		
		char buf_command[32]; 
		sprintf(buf_command, "%s", dragon_load_commands[dragon_data_type]);
		
		graphic_display_text(buf_command, pos_y, pos_x, LABEL_COLOR, BG_COLOR);
		
		pos_y = 24;
		pos_x = 162;
		
		
		if (dragon_use_remote) {
			graphic_draw_status_indicator("Remote", !gpio_get_level(REMOTE_PIN), pos_x, pos_y, INDICATOR_MOTOR_COLOR, INDICATOR_OFF_COLOR);
		}
		else {
			graphic_draw_status_indicator("Remote", false, pos_x, pos_y, INDICATOR_OFF_COLOR, INDICATOR_OFF_COLOR);
		}
		
		pos_y = pos_y + 24;
		
		graphic_draw_status_indicator("Data", has_data_activity(), pos_x, pos_y, INDICATOR_DATA_COLOR, INDICATOR_OFF_COLOR);
		
		size_t display_pos = stop_pos == 0 ? dragon_player_pos : stop_pos;
		
		pos_x = 4;
		pos_y = 70;

		char buf_pos[32]; 
		sprintf(buf_pos, "%zu of %zu", display_pos, file_browser_file_len);
		
		graphic_display_text(buf_pos, pos_y, pos_x, LABEL_COLOR, BG_COLOR);
		
		pos_y = 90;
		
		graphic_draw_progress_bar(display_pos, file_browser_file_len, pos_x, pos_y, dragon_player_tape_status ? PROGRESS_BAR_ON_COLOR : PROGRESS_BAR_OFF_COLOR, BG_COLOR);
		
		graphic_footer_button_t btn1 = {
			.text = "1-Play",
			.fg_color = (dragon_player_tape_status ? BUTTON_DISABLED_LABEL_COLOR : FOOTER_LABEL_COLOR),
			.bg_color = (dragon_player_tape_status ? BUTTON_PLAY_ON_BG_COLOR : FOOTER_BG_COLOR)
		};
		
		graphic_footer_button_t btn2 = {
			.text = "2-Stop",
			.fg_color = (dragon_player_tape_status && !dragon_player_user_tape_status) ? BUTTON_STOP_STOPPING_LABEL_COLOR : (dragon_player_tape_status ? FOOTER_LABEL_COLOR : BUTTON_DISABLED_LABEL_COLOR),
			.bg_color = FOOTER_BG_COLOR
		};
		
		graphic_footer_button_t btn3 = {
			.text = "3-Reset",
			.fg_color = (dragon_player_tape_status ? BUTTON_DISABLED_LABEL_COLOR : FOOTER_LABEL_COLOR),
			.bg_color = FOOTER_BG_COLOR
		};

		draw_footer(&btn1, &btn2, &btn3);
		
	}
	else {
		
		graphic_display_invalid_file_screen("Dragon / Tandy CoCo");
	}
	
	display_draw();
	
}

static void use_remote(void)
{
	dragon_use_remote = !dragon_use_remote;
	save_dragon_use_remote();
}

static void process_keyboard(void)
{
	
	char key = keyboard_get_key();
	
	if (dragon_player_file_valid) {
	
		if (key == '1') { // PLAY
			if (!dragon_player_tape_status) {
				stop_pos = 0;
				processed_data_tracker = dragon_player_data_tracker;
				dragon_player_user_tape_status = true;
			}
		}
		else if (key == '2') { // STOP
			if (dragon_player_tape_status) {
				stop_pos = dragon_player_pos;
				dragon_player_user_tape_status = false;
			}
		}
		 else if (key == '3') { // Reset
			if (dragon_player_tape_status == false) { // tape stopped
				stop_pos = 0;
				dragon_player_pos = 0; // reset tape position
			}
		}
		else if (key == 0x87) { // Exit
			if (dragon_player_tape_status == false) { // tape stopped
				dragon_player_process_active = false; // exit tape
			}
		}
		else if (key == 'R') { // Use Remote
			if (dragon_player_tape_status == false) { // tape stopped
				use_remote();
			}
		}
	
	}
	else {
		if (key == 0x87) { // Exit
			dragon_player_process_active = false; // exit tape
		}
	}
	
}

static void main_task(void *arg)
{
    while (dragon_player_process_active) {
		
        process_keyboard();
        
        if (dragon_player_load_buffer) {
			dragon_player_load_buffer = false;
			tape_buffer_load(dragon_player_buffer_overlap);
		}
        
        if (dragon_player_display_ready && !display_transfer_in_progress) {
			display_progress();
		}
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    xSemaphoreGive(rom_done_sem);
    vTaskDelete(NULL);
}

static void tape_task(void *arg)
{
	uint8_t header_data[1024];

	size_t header_len = sd_read_chunk(file_browser_file, file_browser_file_len, 0, header_data, 1024);

	if (cas_valid(header_data, header_len)) {

		dragon_data_type = get_dragon_data_type(header_data,  header_len);

		dragon_player_file_valid = true;
		dragon_player_display_ready = true;
		
		dragon_cas_main();
	
	}
	else {
		dragon_player_display_ready = true;
	}

 
    xSemaphoreGive(rom_done_sem);
    vTaskDelete(NULL);
}

void dragon_player_main()
{
	
	stop_pos = 0;
	dragon_player_file_valid = false;
    dragon_player_process_active = true;
    dragon_player_display_ready = false;
    
    load_dragon_use_remote();
    
    if (rom_done_sem == NULL)
        rom_done_sem = xSemaphoreCreateCounting(2, 0);
        
    xTaskCreatePinnedToCore(main_task, "main", 4096, NULL, 2, NULL, 0);
	xTaskCreatePinnedToCore(tape_task, "tape", 8192, NULL, 5, NULL, 1);
	
    xSemaphoreTake(rom_done_sem, portMAX_DELAY);
    xSemaphoreTake(rom_done_sem, portMAX_DELAY);
    
	state = STATE_FILE_BROWSER;
    
}
