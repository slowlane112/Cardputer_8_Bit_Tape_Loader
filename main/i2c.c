/*
 * SPDX-FileCopyrightText: 2026 slowlane112
 *
 * SPDX-License-Identifier: Apache-2.0
 */
 
#include "driver/i2c.h"

#define I2C_MASTER_SDA_IO  GPIO_NUM_8
#define I2C_MASTER_SCL_IO  GPIO_NUM_9
#define I2C_MASTER_NUM     I2C_NUM_0

void i2c_deinit(void)
{
    // Uninstall I2C driver
    ESP_ERROR_CHECK(i2c_driver_delete(I2C_MASTER_NUM));

    // Reset pins so they can be reused
    gpio_reset_pin(I2C_MASTER_SDA_IO);
    gpio_reset_pin(I2C_MASTER_SCL_IO);
}

void i2c_init()
{
    i2c_config_t cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
    };
    ESP_ERROR_CHECK(i2c_param_config(I2C_MASTER_NUM, &cfg));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_MASTER_NUM, cfg.mode, 0, 0, 0));
    
}


