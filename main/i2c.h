/*
 * SPDX-FileCopyrightText: 2026 slowlane112
 *
 * SPDX-License-Identifier: Apache-2.0
 */
 
#ifndef I2C_H
#define I2C_H

#include "driver/i2c_master.h"

extern i2c_master_bus_handle_t g_i2c_bus;
extern i2c_master_dev_handle_t g_tca8418_dev;

void i2c_init(void);

#endif
