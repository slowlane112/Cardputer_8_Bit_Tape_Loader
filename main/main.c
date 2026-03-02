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
#include "nvs_flash.h"

//(Top) → Component config → FreeRTOS → Kernel - (1000) configTICK_RATE_HZ
//(Top) → Component config → ESP System Settings - CPU frequency (240 MHz)
//(Top) → Component config → FAT Filesystem support - Long filename support (Long filename buffer in heap)

void app_main(void)
{
	
	config_init();
	display_init();
	i2c_init();
    keyboard_setup();
    sdcard_init();
    
	esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        printf("NVS Error\n");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }
	
    system_main();

}


