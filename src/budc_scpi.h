/* BUDC Controller - A cross-platform controller for BUC/BUDC devices.
 *
 * Copyright (C) 2024 Penthertz
 *
 * This file is part of BUDC Controller.
 *
 * BUDC Controller is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef BUDC_SCPI_H
#define BUDC_SCPI_H

#include <stddef.h>
#include <stdbool.h>

typedef struct budc_device budc_device;
typedef struct { char name[128]; char description[256]; } serial_port_info;

// Connection
int budc_find_ports(serial_port_info** port_list);
budc_device* budc_connect(const char* port_name);
void budc_disconnect(budc_device* dev);
bool budc_is_connected(budc_device* dev);

// Raw command
int budc_send_raw_command(budc_device* dev, const char* command, char* response, size_t response_len);

// Getters
int budc_get_identity(budc_device* dev, char* buffer, size_t len);
int budc_get_frequency_ghz(budc_device* dev, double* freq_ghz);
int budc_get_lock_status(budc_device* dev, bool* is_locked);
int budc_get_temperature_c(budc_device* dev, float* temp_c);
int budc_get_power_level(budc_device* dev, int* power_level);

// Setters
int budc_set_frequency_ghz(budc_device* dev, double freq_ghz);
int budc_set_frequency_mhz(budc_device* dev, double freq_mhz);
int budc_set_frequency_hz(budc_device* dev, double freq_hz);
int budc_set_power_level(budc_device* dev, int power_level);
int budc_save_settings(budc_device* dev);
int budc_preset(budc_device* dev);

// Robust High-Level Functions
int budc_wait_for_lock(budc_device* dev, unsigned int timeout_ms);
int budc_set_frequency_and_wait(budc_device* dev, double freq_ghz, unsigned int timeout_ms);


#endif // BUDC_SCPI_H