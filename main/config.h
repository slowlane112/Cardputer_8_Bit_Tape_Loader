/*
 * SPDX-FileCopyrightText: 2026 slowlane112
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CONFIG_H
#define CONFIG_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#define COMMODORE_DATA_PIN    	GPIO_NUM_3
#define COMMODORE_SENSE_PIN   	GPIO_NUM_4
#define COMMODORE_MOTOR_PIN   	GPIO_NUM_6
#define COMMODORE_DATA_PIN_MASK (1ULL << 3)

extern bool is_cardputer_adv;
extern bool use_gove_port;
extern gpio_num_t AUDIO_OUT_PIN;
extern gpio_num_t REMOTE_PIN;
extern uint32_t AUDIO_OUT_PIN_MASK;

extern StaticTask_t mainTCB;
extern StackType_t mainStack[4096];
extern StaticTask_t tapeTCB;
extern StackType_t tapeStack[8192];

void config_init(void);
void config_set_use_gove_port(bool value);

#endif

