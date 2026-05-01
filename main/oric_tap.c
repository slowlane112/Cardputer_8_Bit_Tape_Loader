/*
 * SPDX-FileCopyrightText: 2026 slowlane112
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "driver/gpio.h"
#include "oric_player.h"
#include "config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "soc/gpio_struct.h"
#include "file_browser.h"
#include "tape_buffer.h"
#include "esp_rom_sys.h"

#define ORIC_SAMPLE_US  208  

static size_t last_block_pos = 0;
static uint8_t current_level = 0;

static IRAM_ATTR void initial_data(size_t start_pos) {
    tape_buffer_load_initial(oric_player_buffer_overlap, start_pos);
    oric_player_load_buffer = true;
}

static IRAM_ATTR void ensure_data() {
    if (tape_buffer_1_size == TAPE_BUFFER_SIZE && (oric_player_pos - tape_buffer_1_offset) > tape_buffer_1_size - oric_player_buffer_overlap) {
        tape_buffer_swap();
        oric_player_load_buffer = true;
    }
}

static IRAM_ATTR uint8_t read_byte(size_t pos) {
    return tape_buffer_1[pos - tape_buffer_1_offset];
}

static IRAM_ATTR void pluse(uint8_t size) {
	
	if (oric_player_user_tape_status) {
	
		current_level ^= 1;

		if (current_level) {
			GPIO.out_w1ts = AUDIO_OUT_PIN_MASK;  // set high
		} else {
			GPIO.out_w1tc = AUDIO_OUT_PIN_MASK;  // set low
		}

		esp_rom_delay_us(size * ORIC_SAMPLE_US);
    
	}
}

static IRAM_ATTR void play_bit(uint8_t bit) {
    pluse(1);
    if (bit) {
        pluse(1);
    } else {
        pluse(2);
    }
}

static IRAM_ATTR void play_byte(uint8_t val) {
	
    uint8_t parity = 1;

    play_bit(0);

    for (uint8_t i = 0; i < 8; i++) {
        int b = (val >> i) & 1;
        parity += b;
        play_bit(b);
    }

    play_bit(parity & 1);

    play_bit(1);
    play_bit(1);
    play_bit(1);
    play_bit(1);
    
}

static IRAM_ATTR void play_gap(void) {
    for (uint8_t i = 0; i < 20; i++) {
        play_bit(1);
    }
}

static void stop(void) {
    oric_player_pos = last_block_pos;
    oric_player_tape_status = false;
    oric_player_user_tape_status = false;
}

static void play(void)
{
    if (oric_player_pos != 0)
    {
        oric_player_pos = last_block_pos;
    }

    uint16_t leader_count = 0;
    uint16_t last_sync_pos = 0;
    bool played_gap = false;

    while (oric_player_pos < file_browser_file_len)
    {
        ensure_data();
        
        while (oric_use_remote && gpio_get_level(REMOTE_PIN) == 1) {
			if (!oric_player_user_tape_status) {
				break;
			}
			vTaskDelay(pdMS_TO_TICKS(1));
		}

        uint8_t b = read_byte(oric_player_pos);
        
		if (b == 0x16)
		{
			leader_count++;
		}
		else if (b == 0x24) // sync
		{
			if (leader_count > 2) {
				
				last_block_pos = oric_player_pos - leader_count;
				last_sync_pos = oric_player_pos;
				played_gap = false;
				
				// play longer leader
				
				if (leader_count < 256) {
				
					for (uint16_t i = 0; i < (256 - leader_count); i++)
					{
						play_byte(0x16);
					}
				
				}

			}
			
			leader_count = 0;
		}
		else {
			leader_count = 0;
		}

		if (oric_player_user_tape_status && b != 0x00) {
			oric_player_data_tracker++;
		}
		
        play_byte(b);
        
		if (b == 0x00 && played_gap == false && (oric_player_pos - last_sync_pos) > 9) {
			play_gap();
			played_gap = true;
		}
		
        oric_player_pos++;

        if (!oric_player_user_tape_status)
            break;

        if (oric_player_pos == file_browser_file_len)
        {
            last_block_pos = oric_player_pos;
        }
    }

    stop();
}

void oric_tap_main() {
	
	current_level = 0;
    gpio_set_level(AUDIO_OUT_PIN, 0);
    oric_player_buffer_overlap = 12;
    oric_player_pos = 0;
    oric_player_tape_status = false;

    while (oric_player_process_active) {

        if (!oric_player_tape_status) {

            if (oric_player_user_tape_status) {

                if (oric_player_pos == 0) {
                    gpio_set_level(AUDIO_OUT_PIN, 0);
                    initial_data(0);
                }
                oric_player_tape_status = true;
                play();
            }
            
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
}
