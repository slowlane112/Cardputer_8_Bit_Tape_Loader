/*
 * SPDX-FileCopyrightText: 2026 slowlane112
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CONFIG_H
#define CONFIG_H

#define COMMODORE_DATA_PIN    	GPIO_NUM_3
#define COMMODORE_SENSE_PIN   	GPIO_NUM_4
#define COMMODORE_MOTOR_PIN   	GPIO_NUM_6
#define AUDIO_OUT_PIN   		GPIO_NUM_5
#define REMOTE_PIN				GPIO_NUM_15

#define COMMODORE_DATA_PIN_MASK (1 << 3)
#define AUDIO_OUT_PIN_MASK 		(1 << 5)

void config_init(void);

#endif

