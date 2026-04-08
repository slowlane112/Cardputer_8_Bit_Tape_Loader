/*
 * SPDX-FileCopyrightText: 2026 slowlane112
 *
 * SPDX-License-Identifier: Apache-2.0
 */
 
#include "driver/gpio.h"
#include "msx_player.h"
#include "config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"
#include "soc/gpio_struct.h"
#include "file_browser.h"
#include "tape_buffer.h"

// 1200 Baud
#define HALF_LOW_US      416 
#define HALF_HIGH_US     208 

const uint8_t MAGIC_ID[] = {0x1F, 0xA6, 0xDE, 0xBA, 0xCC, 0x13, 0x7D, 0x74};
static size_t last_block_pos = 0;

static IRAM_ATTR void initial_data(size_t start_pos) {
	tape_buffer_load_initial(msx_player_buffer_overlap, start_pos);
	msx_player_load_buffer = true;
}

static IRAM_ATTR void ensure_data() {
    
    if (tape_buffer_1_size == TAPE_BUFFER_SIZE && (msx_player_pos - tape_buffer_1_offset) > tape_buffer_1_size - msx_player_buffer_overlap) {
		tape_buffer_swap();
		msx_player_load_buffer = true;
	}

}

static IRAM_ATTR void play_pulses(int half_period, int count) {
	
	if (msx_player_user_tape_status) {
		
		msx_player_data_tracker++;
		
		for (int i = 0; i < count; i++) {
			
			GPIO.out_w1ts = AUDIO_OUT_PIN_MASK; 
			esp_rom_delay_us(half_period);
			
			GPIO.out_w1tc = AUDIO_OUT_PIN_MASK;
			esp_rom_delay_us(half_period);
		}
	}
}

static IRAM_ATTR void play_bit(uint8_t bit) {
    if (bit) {
        play_pulses(HALF_HIGH_US, 2);
    } else {
        play_pulses(HALF_LOW_US, 1);
    }
}

static IRAM_ATTR void play_byte(uint8_t b) {
    play_bit(0);
    for (int i = 0; i < 8; i++) {
        play_bit((b >> i) & 0x01);
    }
    play_bit(1);
    play_bit(1);
}

static IRAM_ATTR void play_header(int duration_ms) {
	// 1200 Baud: 2400 Hz
    int cycles = (duration_ms * 2400) / 1000;
    play_pulses(HALF_HIGH_US, cycles);
}

static IRAM_ATTR uint8_t read_byte(size_t pos) {
	return tape_buffer_1[pos - tape_buffer_1_offset];
}

static void stop(void) {
	msx_player_pos = last_block_pos;
	msx_player_tape_status = false;
	msx_player_user_tape_status = false;
}

static void play(void) {

	if (msx_player_pos != 0) {
		msx_player_pos = last_block_pos;
	}

	while (msx_player_pos < file_browser_file_len) {
		
		ensure_data();

		if (msx_player_pos + 8 <= file_browser_file_len && memcmp(&tape_buffer_1[msx_player_pos - tape_buffer_1_offset], MAGIC_ID, 8) == 0) {
			
			// header
			
			esp_rom_delay_us(500000);
			
			while (msx_use_remote && gpio_get_level(REMOTE_PIN) == 1) {
				if (!msx_player_user_tape_status) {
					break;
				}
				vTaskDelay(pdMS_TO_TICKS(1));
			}
			
			last_block_pos = msx_player_pos;
			
			uint8_t block_id = read_byte(msx_player_pos + 8);
			
			if (block_id == 0x00) {
				play_header(2000);
			} 
			else {
				play_header(5000); 
			}
			
			
			msx_player_pos += 8; 
		}
		
		// data
		play_byte(read_byte(msx_player_pos));
		
		msx_player_pos++;
		
		if (!msx_player_user_tape_status) {
			break;
		}
		
		if (msx_player_pos == file_browser_file_len) { // reached end
			last_block_pos = msx_player_pos;
		}
	
	}
  
    stop();
}

void msx_cas_main() {
	
	gpio_set_level(AUDIO_OUT_PIN, 0);
	msx_player_buffer_overlap = 12;
	msx_player_pos = 0;
	msx_player_tape_status = false;
	
	while (msx_player_process_active) {
		
		if (!msx_player_tape_status) {
			// stopped
			
			if (msx_player_user_tape_status) {
				// start tape
				
				if (msx_player_pos == 0) {
					gpio_set_level(AUDIO_OUT_PIN, 0);
					initial_data((msx_player_pos == 0 ? 0 : last_block_pos));
				}
				
				msx_player_tape_status = true;

				play();
				
			}
		}
		
		vTaskDelay(pdMS_TO_TICKS(10));
	}
	
}

