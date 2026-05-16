/*
 * SPDX-FileCopyrightText: 2026 slowlane112
 *
 * SPDX-License-Identifier: Apache-2.0
 */
 
#include <stdio.h>
#include "esp_task_wdt.h"
#include "config.h"
#include "i2c.h"
#include "display.h"
#include "keyboard.h"
#include "sdcard.h"
#include "system.h"
#include "nvs.h"
#include "state.h"
#include "option.h"
#include "help.h"
#include "file_browser.h"
#include "commodore_player.h"
#include "spectrum_player.h"
#include "msx_player.h"
#include "acorn_player.h"
#include "dragon_player.h"
#include "oric_player.h"

//(Top) → Component config → FreeRTOS → Kernel - (1000) configTICK_RATE_HZ
//(Top) → Component config → ESP System Settings - CPU frequency (240 MHz)
//(Top) → Component config → FAT Filesystem support - Long filename support (Long filename buffer in heap)

void app_main(void)
{

	esp_task_wdt_deinit();
	
    nvs_init();	
	i2c_init();
	config_init();
	display_init();
    keyboard_init();
	sdcard_init();
    
    state = STATE_SYSTEM;
	
    for (;;) {
		switch (state) {
			case STATE_SYSTEM:    			system_main();           	break;
			case STATE_OPTION:    			option_main();           	break;
			case STATE_HELP:    			help_main();           		break;			
			case STATE_FILE_BROWSER:   	 	file_browser_main();     	break;
			case STATE_PLAYER_COMMODORE: 	commodore_player_main(); 	break;
			case STATE_PLAYER_SPECTRUM: 	spectrum_player_main();  	break;
			case STATE_PLAYER_MSX: 			msx_player_main();  		break;
			case STATE_PLAYER_ACORN: 		acorn_player_main();  		break;									
			case STATE_PLAYER_DRAGON: 		dragon_player_main();  		break;					
			case STATE_PLAYER_ORIC: 		oric_player_main();  		break;			
		}
	}
	

}
