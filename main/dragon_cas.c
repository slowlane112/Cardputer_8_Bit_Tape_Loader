/*
 * SPDX-FileCopyrightText: 2026 slowlane112
 *
 * SPDX-License-Identifier: Apache-2.0
 */
 
#include "driver/gpio.h"
#include "dragon_player.h"
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

static size_t last_block_pos = 0;

static IRAM_ATTR void initial_data(size_t start_pos) {
	tape_buffer_load_initial(dragon_player_buffer_overlap, start_pos);
	dragon_player_load_buffer = true;
}

static IRAM_ATTR void ensure_data() {
    
    if (tape_buffer_1_size == TAPE_BUFFER_SIZE && (dragon_player_pos - tape_buffer_1_offset) > tape_buffer_1_size - dragon_player_buffer_overlap) {
		tape_buffer_swap();
		dragon_player_load_buffer = true;
	}

}

static IRAM_ATTR void pulse(uint8_t bit)
{
	
	if (dragon_player_user_tape_status) {
	
		GPIO.out_w1tc = AUDIO_OUT_PIN_MASK; // Start Low

		if (bit) {
			// Logic 1 - total 416us - 2400Hz
			esp_rom_delay_us(HALF_HIGH_US);      // 208
			GPIO.out_w1ts = AUDIO_OUT_PIN_MASK;  // Switch to High
			esp_rom_delay_us(HALF_HIGH_US);      // 208
		} 
		else {
			// Logic 0 - total 832us - 1200Hz
			esp_rom_delay_us(HALF_LOW_US);       // 416
			GPIO.out_w1ts = AUDIO_OUT_PIN_MASK;  // Switch to High
			esp_rom_delay_us(HALF_LOW_US);       // 416
		}
    
	}
}

static IRAM_ATTR void play_byte(uint8_t b)
{
    for (int i = 0; i < 8; i++)
    {
        pulse((b >> i) & 0x01);
    }
}

static IRAM_ATTR uint8_t read_byte(size_t pos) {
	return tape_buffer_1[pos - tape_buffer_1_offset];
}

static void stop(void) {
	dragon_player_pos = last_block_pos;
	dragon_player_tape_status = false;
	dragon_player_user_tape_status = false;
}

static void play(void)
{
    if (dragon_player_pos != 0)
    {
        dragon_player_pos = last_block_pos;
    }

    uint16_t leader_count = 0;

    while (dragon_player_pos < file_browser_file_len)
    {
        ensure_data();
        
        while (dragon_use_remote && gpio_get_level(REMOTE_PIN) == 1) {
			if (!dragon_player_user_tape_status) {
				break;
			}
			vTaskDelay(pdMS_TO_TICKS(1));
		}

        uint8_t b = read_byte(dragon_player_pos);

		if (b == 0x55)
		{
			leader_count++;
		}
		else if (b == 0x3C && dragon_player_pos != file_browser_file_len) // sync
		{
			// after the 0x3C sync, the next byte should be a 0x00 or 0x01
			
			uint8_t block_type = read_byte(dragon_player_pos + 1);
			
			if (block_type == 0x00 || block_type == 0x01) {
			
				if (leader_count > 0 ) {
					last_block_pos = dragon_player_pos - leader_count;
				}
				
				if (leader_count > 0 && leader_count < 256) {
					
					// play longer leader
					
					for (uint16_t i = 0; i < (256 - leader_count); i++)
					{
						play_byte(0x55);
					}
					
				}
			
			}
			
			leader_count = 0;
		}
		else {
			
			// leader without sync - tandy Donkey Monkey 
			if (leader_count > 63 && leader_count < 256) {
				
				for (uint16_t i = 0; i < (256 - leader_count); i++)
				{
					play_byte(0x55);
				}
				
			}
			
			leader_count = 0;
		}

		if (dragon_player_user_tape_status && b != 0x00) {
			dragon_player_data_tracker++;
		}
		
        play_byte(b);
        dragon_player_pos++;

        if (!dragon_player_user_tape_status)
            break;

        if (dragon_player_pos == file_browser_file_len)
        {
			
			// play silence
			for (uint16_t i = 0; i < 16; i++)
			{
				play_byte(0x55);
			}
			
            last_block_pos = dragon_player_pos;
        }
    }

    stop();
}

void dragon_cas_main() {
	
	gpio_set_level(AUDIO_OUT_PIN, 0);
	dragon_player_buffer_overlap = 12;
	dragon_player_pos = 0;
	dragon_player_tape_status = false;
	
	while (dragon_player_process_active) {
		
		if (!dragon_player_tape_status) {
			
			// stopped
			if (dragon_player_user_tape_status) {

				// start tape
				if (dragon_player_pos == 0) {
					gpio_set_level(AUDIO_OUT_PIN, 0);
					initial_data((dragon_player_pos == 0 ? 0 : last_block_pos));
				}
				
				dragon_player_tape_status = true;

				play();
				
			}
		}
		
		vTaskDelay(pdMS_TO_TICKS(10));
	}
	
}

