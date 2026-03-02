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
#include "display.h"
#include "graphic.h"
#include "file_browser.h"
#include "file.h"
#include "commodore_tap.h"
#include "config.h"
#include "tape_buffer.h"

/*
Not playing: SENSE = OPEN - DATA = HIGH
Playing: SENSE = LOW - DATA = PULSE
Paused by motor: No change to SENSE and DATA
*/

volatile uint8_t commodore_player_data_tracker = 0;
volatile size_t commodore_player_pos = 0;
volatile bool commodore_player_load_buffer = false;
volatile size_t commodore_player_buffer_overlap = 0;
volatile bool commodore_player_display_ready = false;
volatile bool commodore_player_tape_status = false; // playing / stopped
volatile bool commodore_player_process_active = false; // tape_loaded
volatile bool commodore_player_user_tape_status = false;
static uint8_t processed_data_tracker = 0;
static size_t stop_pos = 0;
static SemaphoreHandle_t rom_done_sem = NULL;
volatile int8_t commodore_player_system = 0;
volatile int8_t commodore_player_format = 0;

typedef struct {
	char     	signature[12];
	uint8_t  	version;
	uint8_t  	machine;
	uint8_t  	video;
	uint8_t  	unused;
	uint32_t	data_size;
} tap_header_t;

static bool has_data_activity() {
	
	if (commodore_player_data_tracker != processed_data_tracker) {
		processed_data_tracker = commodore_player_data_tracker;
		return true;
	}
	
	return false;
}

void display_progress(void) {
	
	for (int y = HEADER_HEIGHT; y < DISPLAY_HEIGHT - FOOTER_HEIGHT; y++) {
		
		for (int x = 0; x < DISPLAY_WIDTH; x++) {
		
			framebuffer[(y * DISPLAY_WIDTH) + x] = BG_COLOR;
		}
	}
	
	draw_header((const char *)file_name_scroll((const char *)file_browser_file_name));
	
	int pos_x = 4;
	int pos_y = 22;

	char buf_system[32]; 
	sprintf(buf_system, "System: %s", (commodore_player_system == 0 ? "C64" : (commodore_player_system == 1 ? "VIC20" : "C16")));
	
	graphic_display_text(buf_system, pos_y, pos_x, LABEL_COLOR, BG_COLOR);
	
	pos_y = 42;

	char buf_format[32]; 
	sprintf(buf_format, "Format: %s", (commodore_player_format == 0 ? "PAL" : "NTSC"));
	
	graphic_display_text(buf_format, pos_y, pos_x, LABEL_COLOR, BG_COLOR);
	
	pos_y = 24;
	pos_x = 170;
	
	graphic_draw_status_indicator("Motor", gpio_get_level(COMMODORE_MOTOR_PIN), pos_x, pos_y, INDICATOR_MOTOR_COLOR, INDICATOR_OFF_COLOR);
	
	
	pos_y = pos_y + 24;
	
	graphic_draw_status_indicator("Data", has_data_activity(), pos_x, pos_y, INDICATOR_DATA_COLOR, INDICATOR_OFF_COLOR);
	
	size_t display_pos = stop_pos == 0 ? commodore_player_pos : stop_pos;
	
	pos_x = 4;
	pos_y = 70;

	char buf_pos[32]; 
	sprintf(buf_pos, "%zu of %zu", display_pos - TAP_HEADER_SIZE, file_browser_file_len - TAP_HEADER_SIZE);
	
	graphic_display_text(buf_pos, pos_y, pos_x, LABEL_COLOR, BG_COLOR);
	
	pos_y = 90;

	graphic_draw_progress_bar(display_pos - TAP_HEADER_SIZE, file_browser_file_len - TAP_HEADER_SIZE, pos_x, pos_y, commodore_player_tape_status ? PROGRESS_BAR_ON_COLOR : PROGRESS_BAR_OFF_COLOR, BG_COLOR);

	
	graphic_footer_button_t btn1 = {
		.text = "1-Play",
		.fg_color = (commodore_player_tape_status ? BUTTON_DISABLED_LABEL_COLOR : FOOTER_LABEL_COLOR),
		.bg_color = (commodore_player_tape_status ? BUTTON_PLAY_ON_BG_COLOR : FOOTER_BG_COLOR)
	};
	
	graphic_footer_button_t btn2 = {
		.text = "2-Stop",
		.fg_color = (commodore_player_tape_status && !commodore_player_user_tape_status) ? BUTTON_STOP_STOPPING_LABEL_COLOR : (commodore_player_tape_status ? FOOTER_LABEL_COLOR : BUTTON_DISABLED_LABEL_COLOR),
		.bg_color = FOOTER_BG_COLOR
	};
	
	graphic_footer_button_t btn3 = {
		.text = "3-Reset",
		.fg_color = (commodore_player_tape_status ? BUTTON_DISABLED_LABEL_COLOR : FOOTER_LABEL_COLOR),
		.bg_color = FOOTER_BG_COLOR
	};

	draw_footer(&btn1, &btn2, &btn3);
	
	
	display_draw();
	
}

static void process_keyboard(void)
{
	
	char key = keyboard_get_key();
	
	if (key == '1') { // PLAY
        if (!commodore_player_tape_status) {
			stop_pos = 0;
			processed_data_tracker = commodore_player_data_tracker;
			commodore_player_user_tape_status = true;
		}
    }
    else if (key == '2') { // STOP
		if (commodore_player_tape_status) {
			stop_pos = commodore_player_pos;
			commodore_player_user_tape_status = false;
		}
	}
	 else if (key == '3') { // Reset
		if (commodore_player_tape_status == false) { // tape stopped
			stop_pos = 0;
			commodore_player_pos = TAP_HEADER_SIZE; // reset tape position
		}
	}
    else if (key == 0x87) { // Exit
		if (commodore_player_tape_status == false) { // tape stopped
			commodore_player_process_active = false; // exit tape
		}
	}
}

static void main_task(void *arg)
{
	
    while (commodore_player_process_active) {
		process_keyboard();
        if (commodore_player_display_ready && !display_transfer_in_progress) {
            display_progress(); 
        }
        
        if (commodore_player_load_buffer) {
			commodore_player_load_buffer = false;
			tape_buffer_load(commodore_player_buffer_overlap);
		}
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    xSemaphoreGive(rom_done_sem);
    vTaskDelete(NULL);
}

static void tape_task(void *arg) {
	
	uint8_t header_data[TAP_HEADER_SIZE];

	size_t header_len = sd_read_chunk(file_browser_file, file_browser_file_len, 0, header_data, TAP_HEADER_SIZE);

    if (header_len == 20) {

		tap_header_t *hdr = (tap_header_t *)header_data;
    
		commodore_player_system = hdr->machine;
		commodore_player_format = hdr->video;

		commodore_tap_main();

	}

    xSemaphoreGive(rom_done_sem);
    vTaskDelete(NULL);
}

void commodore_player_main()
{
	
	stop_pos = 0;
	commodore_player_process_active = true;
	commodore_player_display_ready = false;
	
	if (rom_done_sem == NULL) { 
		rom_done_sem = xSemaphoreCreateCounting(2, 0); 
	}
	
    xTaskCreatePinnedToCore(main_task, "main", 4096, NULL, 2, NULL, 0);
	xTaskCreatePinnedToCore(tape_task, "tape", 8192, NULL, 5, NULL, 1);
	
    xSemaphoreTake(rom_done_sem, portMAX_DELAY); 
    xSemaphoreTake(rom_done_sem, portMAX_DELAY);
    
    file_browser_main();
}



