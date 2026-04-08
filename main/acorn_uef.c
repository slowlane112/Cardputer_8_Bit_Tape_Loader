/*
 * SPDX-FileCopyrightText: 2026 slowlane112
 *
 * SPDX-License-Identifier: Apache-2.0
 */
 
#include "driver/gpio.h"
#include "acorn_player.h"
#include "config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"
#include "soc/gpio_struct.h"
#include "file_browser.h"
#include "tape_buffer.h"

// 1200 Baud
#define HALF_LOW_US_1200      416 
#define HALF_HIGH_US_1200     208

static uint32_t HALF_LOW_US  = 0;
static uint32_t HALF_HIGH_US = 0;

static bool test_mode = false;

static uint8_t waveform_phase = 1;
static size_t last_block_pos = 0;

static IRAM_ATTR void initial_data(size_t start_pos) {
	tape_buffer_load_initial(acorn_player_buffer_overlap, start_pos);
	acorn_player_load_buffer = true;
}

static IRAM_ATTR void ensure_data() {
    
    if (tape_buffer_1_size == TAPE_BUFFER_SIZE && (acorn_player_pos - tape_buffer_1_offset) > tape_buffer_1_size - acorn_player_buffer_overlap) {
		tape_buffer_swap();
		acorn_player_load_buffer = true;
	}

}

static IRAM_ATTR void play_pulses(int half_period, int count) {
	
    if (test_mode == false && acorn_player_user_tape_status) {
		
        acorn_player_data_tracker++;

        for (int i = 0; i < count; i++) {

            // apply phase only to first pulse if inverted
            if (waveform_phase == 0)
                GPIO.out_w1tc = AUDIO_OUT_PIN_MASK; // low first
            else
                GPIO.out_w1ts = AUDIO_OUT_PIN_MASK; // high first

            esp_rom_delay_us(half_period);

            // normal toggle
            GPIO.out_w1ts ^= AUDIO_OUT_PIN_MASK;
            GPIO.out_w1tc ^= AUDIO_OUT_PIN_MASK;

            esp_rom_delay_us(half_period);

            // reset phase after first pulse
            waveform_phase = 1;
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

static IRAM_ATTR void play_leader_cycles(uint32_t cycles) {
    play_pulses(HALF_HIGH_US, cycles);
}

static IRAM_ATTR void play_silence(uint32_t ms) {
	if (test_mode == false) {
		gpio_set_level(AUDIO_OUT_PIN, 0);
		esp_rom_delay_us(ms * 1000);
	}
}

static IRAM_ATTR void play_packet(uint8_t data, uint8_t data_bits, char parity, int stop_bits, bool extra_wave)
{
    
    play_bit(0);

    for (int i = 0; i < data_bits; i++)
        play_bit((data >> i) & 1);

    if (parity != 'N') {

        uint8_t p = 0;

        for (int i = 0; i < data_bits; i++)
            p ^= (data >> i) & 1;

        if (parity == 'E')
            play_bit(p);
        else
            play_bit(!p);
    }

    for (int i = 0; i < stop_bits; i++)
        play_bit(1);

    if (extra_wave)
        play_pulses(HALF_HIGH_US, 1);
}

static void stop(void) {
	acorn_player_pos = last_block_pos;
	acorn_player_tape_status = false;
	acorn_player_user_tape_status = false;
}

static IRAM_ATTR uint16_t read_word(size_t pos) {
	return tape_buffer_1[pos - tape_buffer_1_offset] | (tape_buffer_1[pos - tape_buffer_1_offset + 1] << 8);
}

static IRAM_ATTR uint8_t read_byte(size_t pos) {
	return tape_buffer_1[pos - tape_buffer_1_offset];
}

static IRAM_ATTR uint32_t read_uint24(size_t pos) {
	return tape_buffer_1[pos - tape_buffer_1_offset] |
								(tape_buffer_1[pos - tape_buffer_1_offset + 1] << 8) |
								(tape_buffer_1[pos - tape_buffer_1_offset + 2] << 16);
}

static IRAM_ATTR uint32_t read_uint32(size_t pos) {
    return tape_buffer_1[pos - tape_buffer_1_offset] |
                                (tape_buffer_1[pos - tape_buffer_1_offset + 1] << 8) |
                                (tape_buffer_1[pos - tape_buffer_1_offset + 2] << 16) |
                                (tape_buffer_1[pos - tape_buffer_1_offset + 3] << 24);
}

static IRAM_ATTR uint8_t read_bit(uint8_t *current_byte, int *bit_index) {
    if (*bit_index == 8) {
        ensure_data();
        *current_byte = read_byte(acorn_player_pos++);
        *bit_index = 0;
    }
    uint8_t bit = (*current_byte >> (7 - *bit_index)) & 1;
    (*bit_index)++;
    return bit;
}

static IRAM_ATTR void chunk_0100(uint32_t len) { // implicit start/stop bit tape data block 

	for (uint32_t i = 0; i < len; i++) {

		ensure_data();

		uint8_t b = read_byte(acorn_player_pos);

		play_packet(b, 8, 'N', 1, false);
		
		acorn_player_pos++;
	}

}

static IRAM_ATTR void chunk_0104(uint32_t len) { // defined tape format data block 

	uint8_t data_bits = read_byte(acorn_player_pos);
	char parity       = read_byte(acorn_player_pos + 1);
	int8_t stop_bits  = (int8_t)read_byte(acorn_player_pos + 2);

	bool extra_wave = false;

	if (stop_bits < 0) {
		stop_bits = -stop_bits;
		extra_wave = true;
	}

	acorn_player_pos += 3;

	uint32_t packets = len - 3;

	for (uint32_t i = 0; i < packets; i++) {

		ensure_data();

		uint8_t b = read_byte(acorn_player_pos);

		play_packet(b, data_bits, parity, stop_bits, extra_wave);
		
		acorn_player_pos++;
	}


}

static IRAM_ATTR void chunk_0110(uint32_t len) { // carrier tone

	if (len >= 2) {
		uint16_t cycles = read_word(acorn_player_pos);
		play_leader_cycles(cycles);
	}

	acorn_player_pos += len;

}

static IRAM_ATTR void chunk_0111(uint32_t len) { // carrier tone with dummy byte

    if (len >= 4) {

        uint16_t before_cycles = read_word(acorn_player_pos);
        uint16_t after_cycles  = read_word(acorn_player_pos + 2);

        play_leader_cycles(before_cycles);

        play_packet(0xAA, 8, 'N', 1, false);

        play_leader_cycles(after_cycles);
        
    }

    acorn_player_pos += len;
}

static IRAM_ATTR void chunk_0112(uint32_t len) { // integer gap

	if (len >= 2) {
		uint16_t ms = read_word(acorn_player_pos);
		play_silence(ms);
	}

	acorn_player_pos += len;

}

static IRAM_ATTR void chunk_0113(uint32_t len) {  // change of base frequency
    if (len < 4) {
        acorn_player_pos += len;
        return;
    }

    uint32_t raw = read_uint32(acorn_player_pos); // read 4 bytes as float
    union {
        uint32_t u;
        float f;
    } conv;
    conv.u = raw;

    float new_freq = conv.f;

    if (new_freq > 0.0f) {
        HALF_LOW_US  = (uint32_t)(1000000.0f / (2.0f * new_freq));
		HALF_HIGH_US = (uint32_t)(1000000.0f / (4.0f * new_freq));
    }

    acorn_player_pos += 4;
}

static IRAM_ATTR void chunk_0114(uint32_t len) { // security cycles

    size_t start = acorn_player_pos;

    if (len < 5) {
        acorn_player_pos = start + len;
        return;
    }

    uint32_t num_cycles = read_uint24(acorn_player_pos);
    char first_flag     = read_byte(acorn_player_pos + 3);
    char last_flag      = read_byte(acorn_player_pos + 4);

    acorn_player_pos += 5;

    uint8_t current_byte = 0;
    int bit_index = 8;

    uint32_t cycles_left = num_cycles;

    if (cycles_left == 0) {
        acorn_player_pos = start + len;
        return;
    }

    uint8_t bit = read_bit(&current_byte, &bit_index);

    if (first_flag == 'P') {

        if (bit)
            play_pulses(HALF_HIGH_US, 1);
        else
            play_pulses(HALF_LOW_US, 1);

        cycles_left--;

        if (cycles_left == 0) {
            acorn_player_pos = start + len;
            return;
        }

        bit = read_bit(&current_byte, &bit_index);
    }

    while (cycles_left > 1) {

        if (bit)
            play_pulses(HALF_HIGH_US, 1);
        else
            play_pulses(HALF_LOW_US, 1);

        cycles_left--;

        bit = read_bit(&current_byte, &bit_index);
    }

    if (cycles_left == 1) {

        if (last_flag == 'P') {

            GPIO.out_w1tc = AUDIO_OUT_PIN_MASK;
            esp_rom_delay_us(bit ? HALF_HIGH_US : HALF_LOW_US);

        } else {

            if (bit)
                play_pulses(HALF_HIGH_US, 1);
            else
                play_pulses(HALF_LOW_US, 1);
        }
    }

    acorn_player_pos = start + len;
}

static IRAM_ATTR void chunk_0115(uint32_t len) { // phase change 
	
    if (len < 2) {
        acorn_player_pos += len;
        return;
    }

    uint16_t phase_deg = read_word(acorn_player_pos); // 0–359 degrees
    
    // Simple implementation: only care about 0 or 180
    if (phase_deg >= 90 && phase_deg <= 270) {
        waveform_phase = 0; // inverted
    } else {
        waveform_phase = 1; // normal
    }

    acorn_player_pos += 2; // advance past the chunk
}

static IRAM_ATTR void chunk_0116(uint32_t len) { // floating point gap

    if (len >= 4) {

        uint32_t raw = read_uint32(acorn_player_pos);

        union {
            uint32_t u;
            float f;
        } conv;

        conv.u = raw;

        uint32_t ms = (uint32_t)(conv.f * 1000.0f);
        
        play_silence(ms);
    }

    acorn_player_pos += len;
}

static void play(void) {
	
	//printf("acorn play\n");
	
	while (acorn_use_remote && gpio_get_level(REMOTE_PIN) == 1) {
		if (!acorn_player_user_tape_status) {
			break;
		}
		vTaskDelay(pdMS_TO_TICKS(1));
	}

    if (acorn_player_pos != 0) {
        acorn_player_pos = last_block_pos;
	}
	else {

		HALF_LOW_US = HALF_LOW_US_1200;
		HALF_HIGH_US = HALF_HIGH_US_1200;

		waveform_phase = 1; // normal phase at tape start

		acorn_player_pos = 12; // header
	}

    while (acorn_player_pos < file_browser_file_len) {
		
        ensure_data();
        
        while (acorn_use_remote && gpio_get_level(REMOTE_PIN) == 1) {
			if (!acorn_player_user_tape_status) {
				break;
			}
			vTaskDelay(pdMS_TO_TICKS(1));
		}

        uint16_t chunk = read_word(acorn_player_pos);
        uint32_t len   = read_uint32(acorn_player_pos + 2);

		//printf("chunk 0x%04X - len %ld - pos: %d\n", chunk, len, acorn_player_pos);

        acorn_player_pos += 6;

        switch (chunk) {

            case 0x0100:  // implicit start/stop bit tape data block 

				chunk_0100(len);

                break;
                
            case 0x0104: // defined tape format data block 

				chunk_0104(len);

				break;


            case 0x0110:  // carrier tone

                chunk_0110(len);
                
                break;
                
			case 0x0111:  // carrier tone with dummy byte

				chunk_0111(len);

				break;

            case 0x0112:  // integer gap
            
				chunk_0112(len);

                break;
                
            case 0x0113:  // change of base frequency
            
				chunk_0113(len);

                break;                
                
            case 0x0114:  // security cycles

				chunk_0114(len);

				break;
				
			case 0x0115:  // phase change

				chunk_0115(len);

				break;
            
            case 0x0116:  // floating point gap

				chunk_0116(len);

				break;
               
            default:
				//printf("Not handled\n");
				acorn_player_pos += len;
                break;
        }


        if (!acorn_player_user_tape_status) {
            break;
		}

        if (acorn_player_pos == file_browser_file_len)
            last_block_pos = acorn_player_pos;
    }

    stop();
}

void acorn_uef_main() {
	
	gpio_set_level(AUDIO_OUT_PIN, 0);
	acorn_player_buffer_overlap = 24;
	acorn_player_pos = 0;
	acorn_player_tape_status = false;
	
	while (acorn_player_process_active) {
		
		if (!acorn_player_tape_status) {
			// stopped
			
			if (acorn_player_user_tape_status) {
				// start tape
				
				if (acorn_player_pos == 0) {
					gpio_set_level(AUDIO_OUT_PIN, 0);
					initial_data((acorn_player_pos == 0 ? 0 : last_block_pos));
				}
				
				acorn_player_tape_status = true;

				play();
				
			}
		}
		
		vTaskDelay(pdMS_TO_TICKS(10));
	}
	
}

