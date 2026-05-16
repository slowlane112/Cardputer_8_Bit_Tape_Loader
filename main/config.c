/*
 * SPDX-FileCopyrightText: 2026 slowlane112
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "driver/gpio.h"
#include "config.h"
#include "nvs.h"

StaticTask_t mainTCB;
StackType_t mainStack[4096];
StaticTask_t tapeTCB;
StackType_t tapeStack[8192];

bool is_cardputer_adv = false;
bool use_gove_port = false;
gpio_num_t AUDIO_OUT_PIN;
gpio_num_t REMOTE_PIN;
uint32_t AUDIO_OUT_PIN_MASK;

static bool get_use_gove_port(void) {
	if (is_cardputer_adv) {
		return nvs_get_value("cf_use_g_port", 0) != 0;
	}
	return true;
}

static void player_audio_remote_config() {
	

	if (is_cardputer_adv) {
		if (use_gove_port) {
			gpio_reset_pin(GPIO_NUM_5);
			gpio_reset_pin(GPIO_NUM_15);
		}
		else {
			gpio_reset_pin(GPIO_NUM_1);
			gpio_reset_pin(GPIO_NUM_2);			
		}
	}
	
	if (use_gove_port) {
		AUDIO_OUT_PIN = GPIO_NUM_1;
        REMOTE_PIN = GPIO_NUM_2;
	}
	else {
		AUDIO_OUT_PIN = GPIO_NUM_5;
        REMOTE_PIN = GPIO_NUM_15;
	}
	
	AUDIO_OUT_PIN_MASK = (1u << AUDIO_OUT_PIN);
	
	gpio_config_t audio_out_conf = {
		.pin_bit_mask = (1ULL << AUDIO_OUT_PIN),
		.mode = GPIO_MODE_OUTPUT,
		.pull_up_en = GPIO_PULLUP_DISABLE,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.intr_type = GPIO_INTR_DISABLE
	};
	gpio_config(&audio_out_conf);
    gpio_set_level(AUDIO_OUT_PIN, 0);
    
    
     gpio_config_t remote_conf = {
        .pin_bit_mask = (1ULL << REMOTE_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    gpio_config(&remote_conf);
}

void config_set_use_gove_port(bool value) {
	use_gove_port = value;
	nvs_set_value("cf_use_g_port", use_gove_port ?  1 : 0);
	player_audio_remote_config();
}

void config_init(void)
{
	
	use_gove_port = get_use_gove_port();
	
	if (is_cardputer_adv) {

		// commodore data + sense outputs
		gpio_config_t commodore_io = {
			.pin_bit_mask = (1ULL << COMMODORE_DATA_PIN) | (1ULL << COMMODORE_SENSE_PIN),
			.mode = GPIO_MODE_OUTPUT,
			.pull_up_en = GPIO_PULLUP_DISABLE,
			.pull_down_en = GPIO_PULLDOWN_DISABLE,
			.intr_type = GPIO_INTR_DISABLE
		};
		gpio_config(&commodore_io);
		
		gpio_set_level(COMMODORE_DATA_PIN,  1);
		//sense out 0 open, 1 low
		gpio_set_level(COMMODORE_SENSE_PIN, 0);

		// commodore motor input
		gpio_config_t commodore_motor_cfg = {
			.pin_bit_mask = (1ULL << COMMODORE_MOTOR_PIN),
			.mode = GPIO_MODE_INPUT,
			.pull_up_en = GPIO_PULLUP_DISABLE,
			.pull_down_en = GPIO_PULLDOWN_ENABLE,
			.intr_type = GPIO_INTR_DISABLE
		};
		gpio_config(&commodore_motor_cfg);
		
	}
	
    player_audio_remote_config();
    
}


