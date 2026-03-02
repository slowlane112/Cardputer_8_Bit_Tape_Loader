/*
 * SPDX-FileCopyrightText: 2026 slowlane112
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "driver/gpio.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "display.h"

#define LCD_PIXEL_CLOCK_HZ  (80 * 1000 * 1000)
#define LCD_CMD_BITS        8
#define LCD_PARAM_BITS      8

#define LCD_HOST       		SPI2_HOST
#define LCD_MOSI   			35
#define LCD_SCLK    		36
#define LCD_CS     			37
#define LCD_DC     			34
#define LCD_RST    			33
#define LCD_BLK     		38
#define LCD_MISO    		-1

static esp_lcd_panel_handle_t display_handle;
bool display_transfer_in_progress = false;
uint16_t *framebuffer = NULL;

static bool display_on_color_trans_done(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *event_data, void *user_ctx) {
    display_transfer_in_progress = false;
    return false;
}

static esp_lcd_panel_handle_t display_get_handle() {

	esp_lcd_panel_handle_t spi_lcd_handle = NULL;
    esp_lcd_panel_io_handle_t io_handle = NULL;

    gpio_config_t bk_gpio_config = {
        .pin_bit_mask = 1ULL << LCD_BLK,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));
    gpio_set_level(LCD_BLK, 1);

    spi_bus_config_t buscfg = {
        .sclk_io_num = LCD_SCLK,
        .mosi_io_num = LCD_MOSI,
        .miso_io_num = LCD_MISO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = DISPLAY_WIDTH * 80 * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = LCD_DC,
        .cs_gpio_num = LCD_CS,
        .pclk_hz = LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = LCD_CMD_BITS,
        .lcd_param_bits = LCD_PARAM_BITS,
        .spi_mode = 0,
        .trans_queue_depth = 10,
        .on_color_trans_done = display_on_color_trans_done
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };

    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &spi_lcd_handle));

	esp_lcd_panel_set_gap(spi_lcd_handle, 40, 53);

    ESP_ERROR_CHECK(esp_lcd_panel_reset(spi_lcd_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(spi_lcd_handle));
    esp_lcd_panel_swap_xy(spi_lcd_handle, true);
	esp_lcd_panel_mirror(spi_lcd_handle, true, false);
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(spi_lcd_handle, true));
    esp_lcd_panel_invert_color(spi_lcd_handle, true);

    return spi_lcd_handle;    
}

void display_init(void)
{
	framebuffer = (uint16_t *)heap_caps_malloc(DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
	
	display_handle = display_get_handle();

}

void display_draw(void)
{
	esp_lcd_panel_draw_bitmap(display_handle, 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, framebuffer);
	display_transfer_in_progress = true;
}





