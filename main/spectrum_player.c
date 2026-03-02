/*
 * SPDX-FileCopyrightText: 2026 slowlane112
 *
 * SPDX-License-Identifier: Apache-2.0
 */
 
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "keyboard.h"
#include "sdcard.h"
#include "file.h"
#include "display.h"
#include "graphic.h"
#include "spectrum_tap.h"
#include "spectrum_tzx.h"
#include "file_browser.h"
#include "tape_buffer.h"
#include "nvs_flash.h"

volatile uint8_t spectrum_player_data_tracker = 0;
volatile size_t spectrum_player_pos = 0;
volatile bool spectrum_player_load_buffer = false;
volatile size_t spectrum_player_buffer_overlap = 0;
volatile bool spectrum_player_display_ready = false;
volatile bool spectrum_player_process_active = false; // tape_loaded
volatile bool spectrum_player_user_tape_status = false;
volatile bool spectrum_player_tape_status = false; // playing / stopped
static SemaphoreHandle_t rom_done_sem = NULL;
static uint8_t processed_data_tracker = 0;
static size_t stop_pos = 0;
volatile uint8_t spectrum_player_system_type = 0;

const char *spectrum_system_types[] = {
    "48K",
    "128K"
};
const int spectrum_player_system_types_count = sizeof(spectrum_system_types) / sizeof(spectrum_system_types[0]);

static void load_spectrum_system_type() {
	uint8_t system_type = 0;
    nvs_handle_t nvs_handle;
    ESP_ERROR_CHECK(nvs_open("tape_player", NVS_READWRITE, &nvs_handle));
    nvs_get_u8(nvs_handle, "sp_system_type", &system_type);
    nvs_close(nvs_handle);

    if (system_type < spectrum_player_system_types_count) {
        spectrum_player_system_type = system_type;
    }
}

static void save_spectrum_system_type() {
	nvs_handle_t nvs_handle;
    ESP_ERROR_CHECK(nvs_open("tape_player", NVS_READWRITE, &nvs_handle));
    ESP_ERROR_CHECK(nvs_set_u8(nvs_handle, "sp_system_type", spectrum_player_system_type));
    ESP_ERROR_CHECK(nvs_commit(nvs_handle));
    nvs_close(nvs_handle);
}

static bool has_data_activity() {
	if (spectrum_player_data_tracker != processed_data_tracker) {
		processed_data_tracker = spectrum_player_data_tracker;
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
	
	int pos_x = 4;
	int pos_y = 30;

	graphic_display_text("System:", pos_y, pos_x, LABEL_COLOR, BG_COLOR);
	pos_x = pos_x + (8 * 8);
	
	if (!spectrum_player_tape_status) {
		graphic_display_text("<", pos_y, pos_x, spectrum_player_system_type == 0  ? DISABLED_LABEL_COLOR : LABEL_COLOR, BG_COLOR);
		pos_x = pos_x + (8 * 2);
	}
	
	graphic_display_text(spectrum_system_types[spectrum_player_system_type], pos_y, pos_x, LABEL_COLOR, BG_COLOR);
	
	if (!spectrum_player_tape_status) {
		pos_x = pos_x + ((strlen(spectrum_system_types[spectrum_player_system_type]) + 1) * 8);
		graphic_display_text(">", pos_y, pos_x, spectrum_player_system_type == spectrum_player_system_types_count - 1  ? DISABLED_LABEL_COLOR : LABEL_COLOR, BG_COLOR);
	}
	
	pos_y = 30;
	pos_x = 178;
	
	graphic_draw_status_indicator("Data", has_data_activity(), pos_x, pos_y, INDICATOR_DATA_COLOR, INDICATOR_OFF_COLOR);
	
	
	size_t display_pos = stop_pos == 0 ? spectrum_player_pos : stop_pos;
	
	pos_x = 4;
	pos_y = 70;

	char buf_pos[32]; 
	sprintf(buf_pos, "%zu of %zu", display_pos, file_browser_file_len);
	
	graphic_display_text(buf_pos, pos_y, pos_x, LABEL_COLOR, BG_COLOR);
	
	pos_y = 90;

	graphic_draw_progress_bar(display_pos, file_browser_file_len, pos_x, pos_y, spectrum_player_tape_status ? PROGRESS_BAR_ON_COLOR : PROGRESS_BAR_OFF_COLOR, BG_COLOR);
	
	
	graphic_footer_button_t btn1 = {
		.text = "1-Play",
		.fg_color = (spectrum_player_tape_status ? BUTTON_DISABLED_LABEL_COLOR : FOOTER_LABEL_COLOR),
		.bg_color = (spectrum_player_tape_status ? BUTTON_PLAY_ON_BG_COLOR : FOOTER_BG_COLOR)
	};
	
	graphic_footer_button_t btn2 = {
		.text = "2-Stop",
		.fg_color = (spectrum_player_tape_status && !spectrum_player_user_tape_status) ? BUTTON_STOP_STOPPING_LABEL_COLOR : (spectrum_player_tape_status ? FOOTER_LABEL_COLOR : BUTTON_DISABLED_LABEL_COLOR),
		.bg_color = FOOTER_BG_COLOR
	};
	
	graphic_footer_button_t btn3 = {
		.text = "3-Reset",
		.fg_color = (spectrum_player_tape_status ? BUTTON_DISABLED_LABEL_COLOR : FOOTER_LABEL_COLOR),
		.bg_color = FOOTER_BG_COLOR
	};

	draw_footer(&btn1, &btn2, &btn3);
	
	display_draw();
	
}

static void system_up(void)
{
	if (spectrum_player_system_type < spectrum_player_system_types_count - 1) {
		spectrum_player_system_type = spectrum_player_system_type + 1;
		save_spectrum_system_type();
	}
}

static void system_down(void)
{
	if (spectrum_player_system_type > 0) {
		spectrum_player_system_type = spectrum_player_system_type - 1;
		save_spectrum_system_type();
	}
}


static void process_keyboard(void)
{
	
	char key = keyboard_get_key();
	
	if (key == '1') { // PLAY
        if (!spectrum_player_tape_status) {
			stop_pos = 0;
			processed_data_tracker = spectrum_player_data_tracker;
			spectrum_player_user_tape_status = true;
		}
    }
    else if (key == '2') { // STOP
		if (spectrum_player_tape_status) {
			stop_pos = spectrum_player_pos;
			spectrum_player_user_tape_status = false;
		}
	}
	 else if (key == '3') { // Reset
		if (spectrum_player_tape_status == false) { // tape stopped
			stop_pos = 0;
			spectrum_player_pos = 0; // reset tape position
		}
	}
    else if (key == 0x87) { // Exit
		if (spectrum_player_tape_status == false) { // tape stopped
			spectrum_player_process_active = false; // exit tape
		}
	}
	else if (key == '/') { // System Up
		if (spectrum_player_tape_status == false) { // tape stopped
			system_up();
		}
	}
	else if (key == ',') { // System Down
		if (spectrum_player_tape_status == false) { // tape stopped
			system_down();
		}
	}
}

static void main_task(void *arg)
{
    while (spectrum_player_process_active) {
        process_keyboard();
        
        if (spectrum_player_load_buffer) {
			spectrum_player_load_buffer = false;
			tape_buffer_load(spectrum_player_buffer_overlap);
		}
        
        if (spectrum_player_display_ready && !display_transfer_in_progress) {
			display_progress();
		}
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    xSemaphoreGive(rom_done_sem);
    vTaskDelete(NULL);
}

static void tape_task(void *arg)
{

	uint8_t header_data[20];

	size_t size = sd_read_chunk(file_browser_file, file_browser_file_len, 0, header_data, 20);

	if (size > 0) {
		
		spectrum_player_display_ready = true;
			
		if (memcmp(header_data, "ZXTape!", 7) == 0) {
			spectrum_tzx_main(); 
			
		}
		else {
			spectrum_tap_main(); 
			
		}
	
	}
	
    xSemaphoreGive(rom_done_sem);
    vTaskDelete(NULL);
}

void spectrum_player_main()
{
	stop_pos = 0;
    spectrum_player_process_active = true;
	spectrum_player_display_ready = false;
	
	load_spectrum_system_type();
    
    if (rom_done_sem == NULL)
        rom_done_sem = xSemaphoreCreateCounting(2, 0);
        
    xTaskCreatePinnedToCore(main_task, "main", 4096, NULL, 2, NULL, 0);
	xTaskCreatePinnedToCore(tape_task, "tape", 8192, NULL, 5, NULL, 1);

    xSemaphoreTake(rom_done_sem, portMAX_DELAY);
    xSemaphoreTake(rom_done_sem, portMAX_DELAY);
    
    file_browser_main();

}
