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
#include "file.h"

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
    } else if (system_selected_index == 4) {
        allow_extension[0] = ".cas";
		allow_extension_count = 1;
    } else if (system_selected_index == 5) {
        allow_extension[0] = ".tap";
		allow_extension_count = 1;		
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

void sdcard_list_free(sdcard_list_t *list)
{
    if (!list) return;

    // free all names
    for (sdcard_chunk_t *c = list->chunks; c; c = c->next) {
        for (size_t i = 0; i < c->count; i++) {
            free(c->entries[i].name);
        }
    }

    // free chunks
    sdcard_chunk_t *c = list->chunks;
    while (c) {
        sdcard_chunk_t *next = c->next;
        free(c);
        c = next;
    }

    // free pointer array
    free(list->entries);

    list->entries = NULL;
    list->chunks = NULL;
    list->count = 0;
}

static int sdcard_entry_compare(const void *a, const void *b)
{
    const sdcard_entry_t *ea = a;
    const sdcard_entry_t *eb = b;

    // Directories first
    if (ea->type != eb->type) {
        return (ea->type == SDCARD_DIR) ? -1 : 1;
	}
    // Alphabetical
    return strcasecmp(ea->name, eb->name);
}

int compare_ptrs(const void *a, const void *b)
{
    const sdcard_entry_t *ea = *(const sdcard_entry_t * const *)a;
    const sdcard_entry_t *eb = *(const sdcard_entry_t * const *)b;

    return sdcard_entry_compare(ea, eb);
}

static inline size_t free_heap(void) {
    return heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
}

sdcard_list_t sdcard_list_dir(const char *path)
{
	
    sdcard_list_t list = {0};

    if (sdcard == NULL || sdmmc_get_status(sdcard) != ESP_OK) {
        list.status = SD_ERR_TIMEOUT;
        return list;
    }

    DIR *dir = opendir(path);
    if (!dir) {
        list.status = SD_ERR_NOT_FOUND;
        return list;
    }

    list.status = SD_OK;

    sdcard_chunk_t *head = calloc(1, sizeof(sdcard_chunk_t));
    if (!head) {
        closedir(dir);
        list.status = SD_ERR_NO_MEM;
        return list;
    }

    sdcard_chunk_t *current = head;
    uint32_t file_index = 0;
    struct dirent *entry;
    
	while ((entry = readdir(dir)) != NULL) {
		
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) {
            file_index++;
            continue;
        }

        bool is_dir = (entry->d_type == DT_DIR);
        bool allowed = is_dir || has_allowed_extension(entry->d_name);

        if (!allowed) {
            file_index++;
            continue;
        }
        
        //size_t name_len = strlen(entry->d_name) + 1;
        
        char temp_name[FILE_NAME_MAX_LEN];
		file_display_file_name(entry->d_name, is_dir ? SDCARD_DIR : SDCARD_FILE, temp_name, sizeof(temp_name));
        
        size_t name_len = strlen(temp_name) + 1;

		size_t required = sizeof(sdcard_entry_t) +
                  name_len +
                  ((list.count + 1) * sizeof(sdcard_entry_t *)) +
                  ((current->count == SDCARD_CHUNK_SIZE) ? sizeof(sdcard_chunk_t) : 0);
				

        size_t free_now = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);

		if (free_now < required + 8192) {        
            list.status = SD_OK_PARTIAL;
            break;
        }

        if (current->count == SDCARD_CHUNK_SIZE) {
            current->next = calloc(1, sizeof(sdcard_chunk_t));
            if (!current->next) {
                list.status = SD_OK_PARTIAL;
                break;
            }
            current = current->next;
        }

        sdcard_entry_t *e = &current->entries[current->count];

        e->type = is_dir ? SDCARD_DIR : SDCARD_FILE;
        e->file_index = file_index;

        //e->name = strdup(entry->d_name);
        e->name = strdup(temp_name);
        
        if (!e->name) {
            list.status = SD_OK_PARTIAL;
            break;
        }

        current->count++;
        list.count++;
        file_index++;
    }

    closedir(dir);
    
    if (list.count > 0) {

		size_t ptr_array_size = list.count * sizeof(sdcard_entry_t *);
		list.entries = malloc(ptr_array_size);
		
		if (!list.entries) {
			list.status = SD_ERR_NO_MEM;
			list.entries = NULL;
		} else {
			size_t out_i = 0;
			for (sdcard_chunk_t *c = head; c; c = c->next) {
				for (size_t i = 0; i < c->count; i++) {
					list.entries[out_i++] = &c->entries[i];
				}
			}

			if (list.count > 1) {
				qsort(list.entries, list.count, sizeof(sdcard_entry_t *), compare_ptrs);
			}
		}
		
	}
	else {
		list.entries = NULL;
		list.status  = SD_OK;
	}

    list.chunks = head;

    return list;
}

void sdcard_get_filename_by_index(const char *path, uint32_t target_index, char *out, size_t out_size) {
    DIR *dir = opendir(path);
    if (!dir) {
        if (out_size > 0) out[0] = '\0';
        return;
    }

    struct dirent *entry;
    uint32_t current_index = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (current_index == target_index) {
            strncpy(out, entry->d_name, out_size - 1);
            out[out_size - 1] = '\0';
            break;
        }
        current_index++;
    }

    closedir(dir);
}

uint32_t sdcard_get_index_by_filename(const char *path, const char *filename) {
    DIR *dir = opendir(path);
    if (!dir) {
        return UINT32_MAX;
    }

    struct dirent *entry;
    uint32_t current_index = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, filename) == 0) {
            closedir(dir);
            return current_index;
        }
        current_index++;
    }

    closedir(dir);
    return UINT32_MAX;
}

FILE *sdcard_open(const char *path, size_t *out_len)
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

size_t sdcard_read_chunk(FILE *f, size_t file_len, size_t pos, uint8_t *buf, size_t chunk_size)
{
    if (pos >= file_len)
        return 0;

    fseek(f, pos, SEEK_SET);

    size_t remaining = file_len - pos;
    size_t to_read = remaining < chunk_size ? remaining : chunk_size;

    return fread(buf, 1, to_read, f);
}


