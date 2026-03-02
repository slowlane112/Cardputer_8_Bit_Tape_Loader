/*
 * SPDX-FileCopyrightText: 2026 slowlane112
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"
#include "file_browser.h"
#include "commodore_player.h"
#include "commodore_tap.h"
#include "config.h"
#include "tape_buffer.h"
#include "soc/gpio_struct.h"

static uint32_t tape_task_current_pulse_us = 0;

static IRAM_ATTR void initial_data(size_t start_pos) {
	tape_buffer_load_initial(commodore_player_buffer_overlap, start_pos);
	commodore_player_load_buffer = true;
}

static IRAM_ATTR void ensure_data() {
    
	if (tape_buffer_1_size == TAPE_BUFFER_SIZE && commodore_player_pos - tape_buffer_1_offset > tape_buffer_1_size - commodore_player_buffer_overlap) {
		tape_buffer_swap();
		commodore_player_load_buffer = true;
	}

}

static inline uint32_t tap_cycles(const uint8_t *buf, size_t len, size_t *pos)
{
	
	size_t index = *pos - tape_buffer_1_offset;

    if (*pos >= len) return 0;

	uint8_t v = buf[index]; 
	(*pos)++;
	
    if (v != 0) return (uint32_t)v * 8;

	index = *pos - tape_buffer_1_offset;

    if (*pos + 3 > len) return 0;

    uint32_t c = (uint32_t)buf[index] |
                 ((uint32_t)buf[index + 1] << 8) |
                 ((uint32_t)buf[index + 2] << 16);

    *pos += 3;
    return c;
}

static inline uint32_t cycles_to_us(uint32_t cycles)
{
    if (commodore_player_format == 1) {
		// NTSC: 1,022,727 Hz
        return (uint32_t)((cycles * 1000000ULL + 511363ULL) / 1022727ULL);
    }
    else {
		// PAL: 985,248 Hz
        return (uint32_t)((cycles * 1000000ULL + 492624ULL) / 985248ULL);
    }
}

static uint32_t compute_next_pulse_us(void)
{
	
    uint32_t cycles = tap_cycles(tape_buffer_1, file_browser_file_len, (size_t *)&commodore_player_pos);
    
    if (cycles == 0) return 0;
    
    return cycles_to_us(cycles);
}

static void stop_tape() {
	
	//sense out 0 open, 1 low
	gpio_set_level(COMMODORE_SENSE_PIN, 0);
	
	gpio_set_level(COMMODORE_DATA_PIN,  1);
	commodore_player_tape_status = false;
	commodore_player_user_tape_status = false;
	
}

static void handle_motor_pause(void)
{
	
    while (!gpio_get_level(COMMODORE_MOTOR_PIN)) {
        if (commodore_player_tape_status && (!commodore_player_user_tape_status || commodore_player_pos >= file_browser_file_len)) {  
			//STOP while paused - user stopped or reached end
			stop_tape();
            return;
        }
		vTaskDelay(pdMS_TO_TICKS(1));
    }

}

static IRAM_ATTR void pulse() {
	

	uint32_t next_us = tape_task_current_pulse_us / 2;
    GPIO.out_w1tc = COMMODORE_DATA_PIN_MASK;
    esp_rom_delay_us(next_us);
    
    next_us = tape_task_current_pulse_us - (tape_task_current_pulse_us / 2);
    GPIO.out_w1ts = COMMODORE_DATA_PIN_MASK;
    esp_rom_delay_us(next_us);
    
}

void commodore_tap_main()
{
	commodore_player_buffer_overlap = 4;
	commodore_player_pos = TAP_HEADER_SIZE;
	commodore_player_display_ready = true;
    commodore_player_tape_status  = false;
    commodore_player_user_tape_status = false;

    while (commodore_player_process_active) {
		
		if (commodore_player_tape_status) {
			// playing
			if (!commodore_player_user_tape_status) { 
				// stop tape
				stop_tape();
				continue;
			}
		}
		else {
			
			// stopped
			if (commodore_player_user_tape_status) {
				// start tape
				
				if (commodore_player_pos == TAP_HEADER_SIZE) {
					initial_data(0);
				}
				
				commodore_player_tape_status = true;

				//sense out 0 open, 1 low
				gpio_set_level(COMMODORE_SENSE_PIN, 1);
	
				vTaskDelay(pdMS_TO_TICKS(100));

			}
			else {
				//staying stopped
				vTaskDelay(pdMS_TO_TICKS(10));
				continue;
			}
			
		}
		
		ensure_data();

        if (!gpio_get_level(COMMODORE_MOTOR_PIN)) {
            handle_motor_pause();
            continue;
        }

        commodore_player_data_tracker++;

        tape_task_current_pulse_us = compute_next_pulse_us();
		if (tape_task_current_pulse_us == 0) {
			stop_tape();
			continue;
		}
		
        pulse();
    }

}
