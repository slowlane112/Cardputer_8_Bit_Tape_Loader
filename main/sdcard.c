/*
 * SPDX-FileCopyrightText: 2026 slowlane112
 *
 * SPDX-License-Identifier: Apache-2.0
 */
 
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include <dirent.h>
#include <sys/stat.h>
#include "sdcard.h"
#include "system.h"

#define TAG "SDCARD"

#define PIN_NUM_MISO 39
#define PIN_NUM_MOSI 14
#define PIN_NUM_CLK  40
#define PIN_NUM_CS   12

static sdmmc_card_t *sdcard = NULL;
static spi_host_device_t host_slot = SPI3_HOST;

static const char *allow_extension[4];
static size_t allow_extension_count = 0;

void sdcard_system_init(void) {
	
	if (system_selected_index == 0) {
        allow_extension[0] = ".tap";
        allow_extension_count = 1;
    } else if (system_selected_index == 1) {
        allow_extension[0] = ".tap";
        allow_extension[1] = ".tzx";
		allow_extension_count = 2;
    } else if (system_selected_index == 2) {
        allow_extension[0] = ".cas";
		allow_extension_count = 1;
	} else if (system_selected_index == 3) {
        allow_extension[0] = ".uef";
        allow_extension[1] = ".hq";
		allow_extension_count = 2;
    }
}

void sdcard_init(void)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "Initializing SD card (SPI mode)");

    // ---- Enable pull-ups (important for SD cards) ---- 
    gpio_set_pull_mode(PIN_NUM_MISO, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(PIN_NUM_MOSI, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(PIN_NUM_CLK,  GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(PIN_NUM_CS,   GPIO_PULLUP_ONLY);

    // ---- SPI bus config ---- 
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 16 * 1024,   // better performance
    };

    // ---- SDSPI host config ---- 
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI3_HOST;             // required for ESP32-S3
    host.max_freq_khz = 8000;          // safe speed (increase later if stable)
	
    // ---- Initialize SPI bus ---- 
    ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "spi_bus_initialize failed: %s", esp_err_to_name(ret));
        return;
    }

    // ---- SD card device config ---- 
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = host.slot;

    // ---- FATFS mount config ---- 
    esp_vfs_fat_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 0,
    };

    ESP_LOGI(TAG, "Mounting filesystem...");

    ret = esp_vfs_fat_sdspi_mount("/sdcard", &host, &slot_config, &mount_config, &sdcard);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SD mount failed: %s", esp_err_to_name(ret));
        spi_bus_free(host.slot);
        return;
    }

    sdmmc_card_print_info(stdout, sdcard);
    ESP_LOGI(TAG, "SD card mounted successfully");
}

void sdcard_deinit(void)
{
    if (sdcard) {
        ESP_LOGI(TAG, "Unmounting SD card...");
        esp_vfs_fat_sdcard_unmount("/sdcard", sdcard);
        spi_bus_free(host_slot);
        sdcard = NULL;
        ESP_LOGI(TAG, "SD card unmounted");
    }
}

static int sdcard_entry_compare(const void *a, const void *b)
{
    const sdcard_entry_t *ea = a;
    const sdcard_entry_t *eb = b;

    // Directories first
    if (ea->type != eb->type)
        return (ea->type == SDCARD_DIR) ? -1 : 1;

    // Alphabetical
    return strcasecmp(ea->name, eb->name);
}

static bool has_allowed_extension(const char *filename)
{

    const char *dot = strrchr(filename, '.');
    if (!dot)
        return false;

    for (size_t i = 0; i < allow_extension_count; i++) {
        if (strcasecmp(dot, allow_extension[i]) == 0) {
            return true;
        }
    }

    return false;
}

sdcard_list_t sdcard_list_dir(const char *path)
{
    sdcard_list_t list = {0};
    
    if (sdcard == NULL || sdmmc_get_status(sdcard) != ESP_OK) {
        ESP_LOGE("SDCARD", "Card is missing or unresponsive (0x107 logic)");
        list.status = SD_ERR_TIMEOUT; 
        return list;
    }
    
    list.status = SD_OK;
    
    DIR *dir = opendir(path);
    if (!dir) {
        ESP_LOGE("SDCARD", "Failed to open directory: %s", path);
        list.status = SD_ERR_NOT_FOUND;
        return list;
    }

    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        // Skip hidden entries
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        bool is_dir = (entry->d_type == DT_DIR);
        bool allowed_file = false;
        
        // Check for .tap extension if it's not a directory
        if (!is_dir) {
			allowed_file = has_allowed_extension(entry->d_name);
			
        }
        
        if (is_dir || allowed_file) {
			// 1. Try to resize the array
			sdcard_entry_t *new_entries = realloc(list.entries, (list.count + 1) * sizeof(sdcard_entry_t));
			if (!new_entries) {
				ESP_LOGE("SDCARD", "Out of memory (array)");
				list.status = SD_ERR_NO_MEM;
				goto memory_error; // Exit and clean up everything
			}
			list.entries = new_entries;

			// 2. Try to copy the filename
			sdcard_entry_t *e = &list.entries[list.count];
			e->name = strdup(entry->d_name);
			if (!e->name) {
				ESP_LOGE("SDCARD", "Out of memory (string)");
				list.status = SD_ERR_NO_MEM;
				goto memory_error; // Exit and clean up everything
			}

			e->type = is_dir ? SDCARD_DIR : SDCARD_FILE;
			list.count++;
		}
    }

    closedir(dir);

    if (list.count > 1) {
        qsort(list.entries, list.count, sizeof(sdcard_entry_t), sdcard_entry_compare);
    }

    return list;
    
    memory_error:
		if (dir) closedir(dir);
		// Free everything we allocated so far to prevent a "Memory Leak"
		for (size_t i = 0; i < list.count; i++) {
			free(list.entries[i].name);
		}
		free(list.entries);
		list.entries = NULL;
		list.count = 0;
		// list.status is already set to SD_ERR_NO_MEM
		return list;
}

FILE *sd_open(const char *path, size_t *out_len)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    struct stat st;
    if (stat(path, &st) != 0) {
        fclose(f);
        return NULL;
    }

    *out_len = st.st_size;
    return f;
}

size_t sd_read_chunk(FILE *f, size_t file_len, size_t pos, uint8_t *buf, size_t chunk_size)
{
    if (pos >= file_len)
        return 0;

    fseek(f, pos, SEEK_SET);

    size_t remaining = file_len - pos;
    size_t to_read = remaining < chunk_size ? remaining : chunk_size;

    return fread(buf, 1, to_read, f);
}


