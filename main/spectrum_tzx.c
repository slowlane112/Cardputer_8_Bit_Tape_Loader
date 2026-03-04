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
#include "puff.h"

#define PILOT_LEN_US        619
#define SYNC1_LEN_US        191
#define SYNC2_LEN_US        210
#define BIT0_LEN_US         244
#define BIT1_LEN_US         489
#define PILOT_HEADER_PULSES 8063
#define PILOT_DATA_PULSES   3223

static int out_level = 0;
static size_t last_block_pos = 0;
static uint16_t loop_block_count = 0;
static size_t loop_block_start_pos = 0;
static bool loop_block_did_buffer_swap = false;

static bool test_mode = false;

static IRAM_ATTR void initial_data(size_t start_pos) {
	tape_buffer_load_initial(spectrum_player_buffer_overlap, start_pos);
	spectrum_player_load_buffer = true;
	if (test_mode) {
		esp_rom_delay_us(2000 * 1000);
	}
}

static IRAM_ATTR void ensure_data() {
    
    if (tape_buffer_1_size == TAPE_BUFFER_SIZE && (spectrum_player_pos - tape_buffer_1_offset) > tape_buffer_1_size - spectrum_player_buffer_overlap) {
		tape_buffer_swap();
		loop_block_did_buffer_swap = true;
		if (loop_block_count == 0) { // dont do while in loop_block
			spectrum_player_load_buffer = true;
		}
		if (test_mode) {
			esp_rom_delay_us(2000 * 1000);
		}
	}

}

static IRAM_ATTR inline uint32_t tstates_to_us(uint16_t t_states)
{
    return (uint32_t)(((t_states * 2) + 4) / 7);
}


static IRAM_ATTR void low() {
	
	if (test_mode == false && spectrum_player_user_tape_status) {
		
		spectrum_player_data_tracker++;
		
		out_level = 0;
		GPIO.out_w1tc = AUDIO_OUT_PIN_MASK;
	
	}
}

static IRAM_ATTR void high() {
	
	 if (test_mode == false && spectrum_player_user_tape_status) {
		
		spectrum_player_data_tracker++;
		
		out_level = 1;
		GPIO.out_w1ts = AUDIO_OUT_PIN_MASK; 
    
	}
}

static IRAM_ATTR void flip() {
    
    if (test_mode == false && spectrum_player_user_tape_status) {
		
		spectrum_player_data_tracker++;
		
		out_level ^= 1;
		if (out_level) {
			GPIO.out_w1ts = AUDIO_OUT_PIN_MASK; 
		} else {
			GPIO.out_w1tc = AUDIO_OUT_PIN_MASK;
		}
		
	}

}

static IRAM_ATTR void pulse_same(uint32_t us) {
	
	if (test_mode == false && spectrum_player_user_tape_status) {
		
		spectrum_player_data_tracker++;
		esp_rom_delay_us(us);
	}
}

static IRAM_ATTR void pulse_low(uint32_t us) {
	
	if (test_mode == false && spectrum_player_user_tape_status) {
		
		spectrum_player_data_tracker++;
		
		out_level = 0;
		GPIO.out_w1tc = AUDIO_OUT_PIN_MASK;
		esp_rom_delay_us(us);
	}
}

static IRAM_ATTR void pulse_high(uint32_t us) {
	
	if (test_mode == false && spectrum_player_user_tape_status) {
		
		spectrum_player_data_tracker++;
		
		out_level = 1;
		GPIO.out_w1ts = AUDIO_OUT_PIN_MASK;
		esp_rom_delay_us(us);
	}
}

static IRAM_ATTR void pulse(uint32_t us) {
    
    if (test_mode == false && spectrum_player_user_tape_status) {
		
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

static IRAM_ATTR void pause(uint16_t pause_length_ms) {
    
	if (test_mode == false && spectrum_player_user_tape_status) {
		flip();
		esp_rom_delay_us(1000);
		low();
		esp_rom_delay_us((pause_length_ms - 1) * 1000);
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

static IRAM_ATTR void block_10_standard_speed_data_block(void) {
	
	uint16_t pause_ms = read_word(spectrum_player_pos);
	uint16_t data_length = read_word(spectrum_player_pos + 2);
	
	spectrum_player_pos += 4; 
	
	uint8_t flag_byte = read_byte(spectrum_player_pos);
	int pilot_pulses = flag_byte < 128 ? PILOT_HEADER_PULSES : PILOT_DATA_PULSES;

	// Pilot tone
	for (int i = 0; i < pilot_pulses; i++) {
		pulse(PILOT_LEN_US);
	}

	// Sync pulses
	pulse(SYNC1_LEN_US);
	pulse(SYNC2_LEN_US);

	// Data bits
	for (int i = 0; i < data_length; i++) {
		
		ensure_data();
		
		uint8_t data_byte = read_byte(spectrum_player_pos);
		
		for (int b = 7; b >= 0; b--) {
			if (data_byte & (1 << b)) {
				pulse(BIT1_LEN_US); pulse(BIT1_LEN_US);
			} else {
				pulse(BIT0_LEN_US); pulse(BIT0_LEN_US);
			}
		}
		
		spectrum_player_pos++;
	}

	// Pause
	if (pause_ms > 0) {
		pause(pause_ms);
	}
	
}

static IRAM_ATTR void block_11_turbo_speed_data_block(void) {
	
	uint16_t pilot_pulse = read_word(spectrum_player_pos);
	uint16_t sync1_pulse = read_word(spectrum_player_pos + 2);
	uint16_t sync2_pulse = read_word(spectrum_player_pos + 4);
	uint16_t bit0_pulse  = read_word(spectrum_player_pos + 6);
	uint16_t bit1_pulse  = read_word(spectrum_player_pos + 8);
	uint16_t pilot_count = read_word(spectrum_player_pos + 10);
	uint8_t last_bits = read_byte(spectrum_player_pos + 12);
	uint16_t pause_ms = read_word(spectrum_player_pos + 13);
	uint32_t data_length = read_uint24(spectrum_player_pos + 15);
	
	spectrum_player_pos += 18;
	
	uint32_t pilot_us = tstates_to_us(pilot_pulse);
	uint32_t s1_us    = tstates_to_us(sync1_pulse);
	uint32_t s2_us    = tstates_to_us(sync2_pulse);
	uint32_t b0_us    = tstates_to_us(bit0_pulse);
	uint32_t b1_us    = tstates_to_us(bit1_pulse);

	// Pilot tone
	for (int i = 0; i < pilot_count; i++) {
		pulse(pilot_us);
	}

	// Sync pulses
	pulse(s1_us);
	pulse(s2_us);

	// Data bits
	for (uint32_t i = 0; i < data_length; i++) {

		ensure_data();

		uint8_t data_byte = read_byte(spectrum_player_pos);

		int bits_to_pulse = (i == data_length - 1) ? (last_bits == 0 ? 8 : last_bits) : 8;

		for (int b = 7; b >= (8 - bits_to_pulse); b--) {
			if (data_byte & (1 << b)) {
				pulse(b1_us); pulse(b1_us);
			} else {
				pulse(b0_us); pulse(b0_us);
			}
		}

		spectrum_player_pos++;
	}

	// Pause
	if (pause_ms > 0) {
		pause(pause_ms);
	}
				
}

static IRAM_ATTR void block_12_pure_tone(void) {
	
	uint16_t pulse_length = read_word(spectrum_player_pos);
	uint16_t count = read_word(spectrum_player_pos + 2);

	spectrum_player_pos += 4;
	
	uint32_t pulse_us = tstates_to_us(pulse_length);
	
	for (uint16_t i = 0; i < count; i++) {
		ensure_data();
		pulse(pulse_us);
	}
	
}

static IRAM_ATTR void block_13_pulse_sequence(void) {
	
	uint8_t count = read_byte(spectrum_player_pos);
				
	spectrum_player_pos++;
	
	for (uint8_t i = 0; i < count; i++) {
		
		ensure_data();
		
		uint16_t pulse_length = read_word(spectrum_player_pos);
	
		spectrum_player_pos += 2;

		uint32_t pulse_us = tstates_to_us(pulse_length);
	
		pulse(pulse_us);
	}
	
}

static IRAM_ATTR void block_14_pure_data_block(void) {
	
	uint16_t bit0_pulse  = read_word(spectrum_player_pos);
	uint16_t bit1_pulse  = read_word(spectrum_player_pos + 2);
	uint8_t last_bits = read_byte(spectrum_player_pos + 4);
	uint16_t pause_ms = read_word(spectrum_player_pos + 5);
	uint32_t data_length = read_uint24(spectrum_player_pos + 7);

	spectrum_player_pos += 10;

	uint32_t b0_us    = tstates_to_us(bit0_pulse);
	uint32_t b1_us    = tstates_to_us(bit1_pulse);

	// data bits
	for (uint32_t i = 0; i < data_length; i++) {

		ensure_data();

		uint8_t data_byte = read_byte(spectrum_player_pos);
		int bits_to_pulse = (i == data_length - 1) ? (last_bits == 0 ? 8 : last_bits) : 8;

		for (int b = 7; b >= (8 - bits_to_pulse); b--) {
			if (data_byte & (1 << b)) {
				pulse(b1_us); pulse(b1_us);
			} else {
				pulse(b0_us); pulse(b0_us);
			}
		}
		
		spectrum_player_pos++;
	}


	if (pause_ms > 0) {
		pause(pause_ms);
	}
	
}

static IRAM_ATTR void block_15_direct_recording(void) {

    uint16_t t_states_per_sample = read_word(spectrum_player_pos);
    uint16_t pause_ms = read_word(spectrum_player_pos + 2);
    uint8_t last_bits = read_byte(spectrum_player_pos + 4);
    uint32_t data_length = read_uint24(spectrum_player_pos + 5);
    spectrum_player_pos += 8;

    uint32_t sample_duration_us = tstates_to_us(t_states_per_sample);

	for (uint32_t i = 0; i < data_length; i++) {
		
		ensure_data();

		uint8_t data_byte = read_byte(spectrum_player_pos);
		
		int bits_to_pulse = (i == data_length - 1) ? (last_bits == 0 ? 8 : last_bits) : 8;

		for (int b = 7; b >= (8 - bits_to_pulse); b--) {
			if (data_byte & (1 << b)) {
				pulse_high(sample_duration_us);
			} else {
				pulse_low(sample_duration_us);
			}
		}
		
		spectrum_player_pos++;
	}

    if (pause_ms > 0) {
        pause(pause_ms);
    }
}

static IRAM_ATTR void block_18_csw_recording(void) {
	
	uint32_t block_len = read_uint32(spectrum_player_pos); 
	
	size_t block_end = spectrum_player_pos + 4 + block_len;
	
	uint16_t pause_ms = read_word(spectrum_player_pos + 4);
	uint32_t sample_rate = read_uint24(spectrum_player_pos + 6);
	uint8_t compression = read_byte(spectrum_player_pos + 9);
	uint32_t total_pulses = read_uint32(spectrum_player_pos + 10);
	
	spectrum_player_pos += 14;
	
	if (compression == 1) { //RLE
	
		for (uint32_t i = 0; i < total_pulses; i++) {
			
			ensure_data();
			
			uint32_t pulse_count = 0;
			uint8_t b = read_byte(spectrum_player_pos);
			spectrum_player_pos++;
			
			if (b == 0) {
				pulse_count = read_uint32(spectrum_player_pos);
				spectrum_player_pos += 4;
			} else {
				pulse_count = b;
			}

			uint32_t us = (uint32_t)(((uint64_t)pulse_count * 1000000ULL) / sample_rate);

			pulse(us); 
		}
    
	}
	else if (compression == 2) { // Z-RLE
		
		size_t comp_payload_len = block_len - 10; 
		unsigned long out_len = total_pulses * 5; 
		uint8_t *rle = malloc(out_len);
		
		uint8_t *comp = malloc(comp_payload_len);

		if (rle && comp) {

			for (uint32_t i = 0; i < comp_payload_len; i++) {
				ensure_data();
				comp[i] = read_byte(spectrum_player_pos++);
			}

			// skip 2-byte zlib header
			unsigned long in_len = comp_payload_len - 2; 
			int ret = puff(rle, &out_len, comp + 2, &in_len);

			if (ret == 0) {
				size_t pos = 0;
				for (uint32_t i = 0; i < total_pulses; i++) {
					if (pos >= out_len) break;

					uint32_t pulse_count;
					uint8_t b = rle[pos++];

					if (b == 0) {
						pulse_count  = (uint32_t)rle[pos++];
						pulse_count |= (uint32_t)rle[pos++] << 8;
						pulse_count |= (uint32_t)rle[pos++] << 16;
						pulse_count |= (uint32_t)rle[pos++] << 24;
					} else {
						pulse_count = b;
					}

					uint32_t us = (uint32_t)(((uint64_t)pulse_count * 1000000ULL) / sample_rate);
					pulse(us);
				}
			} 
		}

		if (comp) free(comp);
		if (rle) free(rle);
	}


     spectrum_player_pos = block_end;

    if (pause_ms > 0) {
        pause(pause_ms);
    }
    
}

static IRAM_ATTR void block_19_generalized_data_block(void) {
    
    //uint32_t length = read_uint32(spectrum_player_pos);
    uint16_t pause_ms = read_word(spectrum_player_pos + 4);
    uint32_t totp = read_uint32(spectrum_player_pos + 6);
    uint8_t npp = read_byte(spectrum_player_pos + 10);
    uint8_t asp_raw = read_byte(spectrum_player_pos + 11);
    uint16_t asp = (asp_raw == 0) ? 256 : asp_raw;
    uint32_t totd = read_uint32(spectrum_player_pos + 12);
    uint8_t npd = read_byte(spectrum_player_pos + 16);
    uint8_t asd_raw = read_byte(spectrum_player_pos + 17);
    uint16_t asd = (asd_raw == 0) ? 256 : asd_raw;

    spectrum_player_pos += 18;

    // pilot section
    if (totp > 0) {
        size_t alpha_size = (asp * (1 + (npp * 2)));
        uint8_t pilot_alpha[alpha_size]; 
        for (size_t j = 0; j < alpha_size; j++) {
			ensure_data();
            pilot_alpha[j] = read_byte(spectrum_player_pos++);
        }
        
        for (uint32_t i = 0; i < totp; i++) {
			ensure_data();
            uint8_t sym_idx = read_byte(spectrum_player_pos++);
            uint16_t repeats = read_word(spectrum_player_pos); 
            spectrum_player_pos += 2;
            
            uint8_t* sym_data = &pilot_alpha[sym_idx * (1 + (npp * 2))];
            uint8_t edge_type = sym_data[0];
           
            for (uint16_t r = 0; r < repeats; r++) {
				
				uint8_t polarity = edge_type & 0x03;
   
				if (polarity == 0x02) { 
					low();
				} else if (polarity == 0x03) { 
					high();
				}

				for (uint8_t p = 0; p < npp; p++) {

					uint16_t t_states = *(uint16_t*)(&sym_data[1 + (p * 2)]);
					if (t_states == 0) break;

					uint32_t us = tstates_to_us(t_states); 

					if (p == 0 && polarity == 0x01) {
						pulse_same(us);
					} else {
						pulse(us);
					}
				}
			}
            
        }
    }

    // data section
    if (totd > 0) {
        size_t alpha_size = (asd * (1 + (npd * 2)));
        uint8_t data_alpha[alpha_size];
        for (size_t j = 0; j < alpha_size; j++) {
			ensure_data();
            data_alpha[j] = read_byte(spectrum_player_pos++);
        }

        uint8_t bits_per_symbol = 0;
        if (asd > 1) {
            uint16_t val = asd - 1;
            while (val > 0) { val >>= 1; bits_per_symbol++; }
        }

        uint32_t bit_offset = 0;
        uint8_t current_byte = 0;

        for (uint32_t i = 0; i < totd; i++) {
            uint8_t sym_idx = 0;
            for (uint8_t b = 0; b < bits_per_symbol; b++) {
                
                if ((bit_offset & 7) == 0) {
					ensure_data();
                    current_byte = read_byte(spectrum_player_pos++);
                }
                uint8_t bit = (current_byte >> (7 - (bit_offset & 7))) & 1;
                sym_idx = (sym_idx << 1) | bit;
                bit_offset++;
            }

            uint8_t* sym_data = &data_alpha[sym_idx * (1 + (npd * 2))];
            uint8_t edge_type = sym_data[0];
         
			uint8_t polarity = edge_type & 0x03;
			
			if (polarity == 0x02) { 
				low();
			} else if (polarity == 0x03) { 
				high();
			}
            
            for (uint8_t p = 0; p < npd; p++) {
                uint16_t t_states = *(uint16_t*)(&sym_data[1 + (p * 2)]);
                if (t_states == 0) break;
                
                uint32_t us = tstates_to_us(t_states);

				if (p == 0 && polarity == 0x01) {
					pulse_same(us);
				} else {
					pulse(us);
				}
					
					
            }
            
        }
    }

    if (pause_ms > 0) {
        pause(pause_ms);
    }
    
}

static IRAM_ATTR void block_20_pause(void) {
	
	uint16_t pause_ms = read_word(spectrum_player_pos);
	
	spectrum_player_pos += 2;
			
	if (pause_ms == 0) {
		// set to next block
		last_block_pos = spectrum_player_pos;
		// set to stopping
		spectrum_player_user_tape_status = false;
	}
	else {
		pause(pause_ms);
	}
				
}

static IRAM_ATTR void block_21_group_start(void) {
	
	uint8_t length = read_byte(spectrum_player_pos);
	
	spectrum_player_pos += (1 + length);
				
}

static IRAM_ATTR void block_24_loop_start(void) {
	
	loop_block_count = read_word(spectrum_player_pos);
	
	spectrum_player_pos += 2;
	
	loop_block_did_buffer_swap = false;
	loop_block_start_pos = spectrum_player_pos;
				
}

static IRAM_ATTR void block_25_loop_end(void) {
	
	if (loop_block_count > 0) {
		loop_block_count = loop_block_count - 1;
	}
	
			
	if (loop_block_count > 0) {
		
		spectrum_player_pos = loop_block_start_pos;
		
		if (loop_block_did_buffer_swap) {
			tape_buffer_swap();
			loop_block_did_buffer_swap = false;
		}
		
	}
				
}

static IRAM_ATTR void block_2A_stop_tape_48k(void) {
	
	uint32_t length = read_uint32(spectrum_player_pos);
			
	spectrum_player_pos += (4 + length);
	
	if (spectrum_player_system_type == 0) { //48k
		
		// set to next block
		last_block_pos = spectrum_player_pos;
		// set to stopping
		spectrum_player_user_tape_status = false;
	
	}
				
}

static IRAM_ATTR void block_2B_set_signal_level(void) {
	
	uint8_t level = read_byte(spectrum_player_pos + 4);
	
	if (level == 1) {
		high();
	}
	else {
		low();
	}
			
	spectrum_player_pos += 5;
	
}

static IRAM_ATTR void block_30_text_description(void) {
	
	uint8_t length = read_byte(spectrum_player_pos);
			
	spectrum_player_pos += (1 + length);
		
}

static IRAM_ATTR void block_31_message_block(void) {
	
	// second byte
	uint8_t length = read_byte(spectrum_player_pos + 1);
			
	spectrum_player_pos += (2 + length);
	
}

static IRAM_ATTR void block_32_archive_info(void) {
	
	uint16_t length = read_word(spectrum_player_pos);
			
	spectrum_player_pos += (2 + length);
	
}

static IRAM_ATTR void block_33_hardware_type(void) {
	
	uint8_t length = read_byte(spectrum_player_pos);
			
	spectrum_player_pos += (1 + (length * 3));
	
}

static IRAM_ATTR void block_35_custom_info_block(void) {
	
	// offset 0x10 - 16
	uint32_t length = read_uint32(spectrum_player_pos + 16);
	
	spectrum_player_pos += (20 + length);
	
}

static IRAM_ATTR void play(void) {

	if (spectrum_player_pos == 0) { // skip header
		spectrum_player_pos = 10;
	}

	while (spectrum_player_pos < file_browser_file_len) {
		
		ensure_data();
		
		last_block_pos = spectrum_player_pos;
		
		uint8_t block_id = tape_buffer_1[spectrum_player_pos - tape_buffer_1_offset];
		spectrum_player_pos++;
		
		//printf("Block: 0x%02X | Pos: %u (0x%X)\n", block_id, spectrum_player_pos - 1, spectrum_player_pos - 1);

		switch (block_id) {
			
			case 0x10: // ID 10 - Standard Speed Data Block
			
				block_10_standard_speed_data_block();
				
				break;
				
			case 0x11: // ID 11 - Turbo Speed Data Block
			
				block_11_turbo_speed_data_block();
				
				break;
				
			case 0x12: // ID 12 - Pure Tone
			
				block_12_pure_tone();
			
				break;
				
			case 0x13: // ID 13 - Pulse sequence
				
				block_13_pulse_sequence();
				
				break;
				
			case 0x14: // ID 14 - Pure Data Block
			
				block_14_pure_data_block();
			
				break;
				
			case 0x15: // ID 15 - Direct Recording
			
				block_15_direct_recording();
			
				break;		
				
			case 0x18: // ID 18 - CSW Recording
			
				block_18_csw_recording();
			
				break;		
				
			case 0x19: // ID 19 - Generalized Data Block
			
				block_19_generalized_data_block();
			
				break;				
				
			case 0x20: // ID 20 - Pause
				
				block_20_pause();
				
				break;				
				
			case 0x21: // ID 21 - Group start
				
				block_21_group_start();
				
				break;
				
			case 0x22: // ID 22 - Group end
				
				// no length
				
				break;
				
			case 0x24: // ID 24 - Loop start
				
				block_24_loop_start();
				
				break;
				
			case 0x25: // ID 25 - Loop end
				
				block_25_loop_end();
				
				break;
				
			case 0x2A: // ID 2A - Stop the tape if in 48K mode
				
				block_2A_stop_tape_48k();
				
				break;
				
			case 0x2B: // ID 2B - Set signal level
				
				block_2B_set_signal_level();
				
				break;				
				
			case 0x30: // ID 30 - Text description
				
				block_30_text_description();
				
				break;
				
			case 0x31: // ID 31 - Message block
				
				block_31_message_block();
				
				break;
				
			case 0x32: // ID 32 - Archive info
				
				block_32_archive_info();
				
				break;
				
			case 0x33: // ID 33 - Hardware type
				
				block_33_hardware_type();
				
				break;		
				
			case 0x35: // ID 35 - Custom info block
				
				block_35_custom_info_block();
				
				break;					
				
			default:
			
				//printf("block_id not handled: 0x%02X\n", block_id);
		}


		if (!spectrum_player_user_tape_status) { // stopping
			break;
		}

		
		if (spectrum_player_pos == file_browser_file_len) { // reached end
			last_block_pos = spectrum_player_pos;
		}
		
	}
	
	if (spectrum_player_user_tape_status) { // not stopping
		flip();
	}
	
    stop();
	
}

void spectrum_tzx_main() {
	
	out_level = 0;
	spectrum_player_buffer_overlap = 32;
	spectrum_player_pos = 0;
	spectrum_player_tape_status = false;
	
	gpio_set_level(AUDIO_OUT_PIN, out_level);
	
	while (spectrum_player_process_active) {
		
		if (!spectrum_player_tape_status) {
			// stopped
			
			if (spectrum_player_user_tape_status) {
				// start tape
				
				initial_data(spectrum_player_pos == 0 ? 0 : last_block_pos);
				
				spectrum_player_tape_status = true;

				play();
				
			}
		}
		
		vTaskDelay(pdMS_TO_TICKS(10));

	}
	
}

