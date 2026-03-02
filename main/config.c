/*
 * SPDX-FileCopyrightText: 2026 slowlane112
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "driver/gpio.h"
#include "esp_task_wdt.h"
#include "config.h"

void config_init(void)
{
	
	esp_task_wdt_deinit();

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
