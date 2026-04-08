/*
 * SPDX-FileCopyrightText: 2026 slowlane112
 *
 * SPDX-License-Identifier: Apache-2.0
 */
 
#include <stdio.h>
#include "config.h"
#include "i2c.h"
#include "display.h"
#include "keyboard.h"
#include "sdcard.h"
#include "system.h"
#include "nvs.h"

//(Top) → Component config → FreeRTOS → Kernel - (1000) configTICK_RATE_HZ
//(Top) → Component config → ESP System Settings - CPU frequency (240 MHz)
//(Top) → Component config → FAT Filesystem support - Long filename support (Long filename buffer in heap)

void app_main(void)
{
	
	config_init();
	display_init();
	i2c_init();
    keyboard_init();
    sdcard_init();
    nvs_init();
	
    system_main();

}


