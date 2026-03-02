/*
 * SPDX-FileCopyrightText: 2026 slowlane112
 *
 * SPDX-License-Identifier: Apache-2.0
 */
 
#include "driver/gpio.h"
#include "spectrum_player.h"
#include "config.h"
#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"
#include "soc/gpio_struct.h"
#include "file_browser.h"
#include "tape_buffer.h"

#define PILOT_LEN_US        619
#define SYNC1_LEN_US        191
#define SYNC2_LEN_US        210
#define BIT0_LEN_US         244
#define BIT1_LEN_US         489
#define PILOT_HEADER_PULSES 8063
#define PILOT_DATA_PULSES   3223

static int out_level = 0;
static size_t last_block_pos = 0;

static IRAM_ATTR void initial_data(size_t start_pos) {
	tape_buffer_load_initial(spectrum_player_buffer_overlap, start_pos);
	spectrum_player_load_buffer = true;
}

static IRAM_ATTR void ensure_data() {
    
    if (tape_buffer_1_size == TAPE_BUFFER_SIZE && (spectrum_player_pos - tape_buffer_1_offset) > tape_buffer_1_size - spectrum_player_buffer_overlap) {
		tape_buffer_swap();
		spectrum_player_load_buffer = true;
	}

}

static IRAM_ATTR void pulse(uint32_t us) {
	
	if (spectrum_player_user_tape_status) {

		spectrum_player_data_tracker++;

		out_level ^= 1;

		if (out_level) {
			GPIO.out_w1ts = AUDIO_OUT_PIN_MASK; 
		} else {
			GPIO.out_w1tc = AUDIO_OUT_PIN_MASK;
		}
		
		esp_rom_delay_us(us);
    
	}
    
}

static void stop(void) {
	spectrum_player_pos = last_block_pos;
	spectrum_player_tape_status = false;
	spectrum_player_user_tape_status = false;
}

static IRAM_ATTR uint16_t read_word(size_t pos) {
	return tape_buffer_1[pos - tape_buffer_1_offset] | (tape_buffer_1[pos - tape_buffer_1_offset + 1] << 8);
}

static IRAM_ATTR uint8_t read_byte(size_t pos) {
	return tape_buffer_1[pos - tape_buffer_1_offset];
}

static void play(void) {

	while (spectrum_player_pos + 2 < file_browser_file_len) {

		last_block_pos = spectrum_player_pos;

		uint16_t data_length = read_word(spectrum_player_pos);

		spectrum_player_pos += 2;
		
		uint8_t flag_byte = read_byte(spectrum_player_pos);
		int pilot_pulses = flag_byte < 128 ? PILOT_HEADER_PULSES : PILOT_DATA_PULSES;
    
		for (int i = 0; i < pilot_pulses; i++) {
			pulse(PILOT_LEN_US);
		}

		pulse(SYNC1_LEN_US);
		pulse(SYNC2_LEN_US);
    
		uint8_t checksum_xor = 0;

		for (int i = 0; i < data_length; i++) {
			
			ensure_data();
			
			uint8_t data_byte = read_byte(spectrum_player_pos);
			
			checksum_xor ^= data_byte;
			
			for (int bit = 7; bit >= 0; bit--) {
				if (data_byte & (1 << bit)) {
					pulse(BIT1_LEN_US);
					pulse(BIT1_LEN_US);
				} else {
					pulse(BIT0_LEN_US);
					pulse(BIT0_LEN_US);
				}
			}
			
			spectrum_player_pos++;
		}
    
		for (int bit = 7; bit >= 0; bit--) {
			uint16_t p = (checksum_xor & (1 << bit)) ? BIT1_LEN_US : BIT0_LEN_US;
			pulse(p); pulse(p);
		}

		if (spectrum_player_user_tape_status) {
			esp_rom_delay_us(1000 * 1000);
		}

		if (!spectrum_player_user_tape_status) {
			break;
		}

		if (spectrum_player_pos == file_browser_file_len) { // reached end
			last_block_pos = spectrum_player_pos;
		}
	
	}
  
    stop();
	
}

void spectrum_tap_main() {
	
	out_level = 0;
	spectrum_player_buffer_overlap = 4;
	spectrum_player_pos = 0;
	spectrum_player_tape_status = false;
	
	while (spectrum_player_process_active) {
		
		if (!spectrum_player_tape_status) {
			// stopped
			
			if (spectrum_player_user_tape_status) {
				// start tape
				
				initial_data((spectrum_player_pos == 0 ? 0 : last_block_pos));
				
				spectrum_player_tape_status = true;

				play();
				
			}
		}
		
		vTaskDelay(pdMS_TO_TICKS(10));
	}
	
}

