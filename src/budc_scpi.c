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

#include "budc_scpi.h"
#include <libserialport.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <stdbool.h>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <unistd.h>
#endif

// --- CONFIGURATION ---
// Debug can be controlled by CMake or disabled here
#ifndef BUDC_DEBUG
#define BUDC_DEBUG 0  // Changed from 1 to 0 to disable debug
#endif

#define READ_TIMEOUT_MS 800
#define COMMAND_TERMINATOR "\r\n"
#define MAX_RETRIES 3

struct budc_device {
    struct sp_port* port;
};

// --- HELPER FUNCTIONS ---
static void scpi_delay(int milliseconds) {
    #ifdef _WIN32
        Sleep(milliseconds);
    #else
        usleep(milliseconds * 1000);
    #endif
}

static void trim_whitespace(char* str) {
    if (!str || *str == '\0') return;
    char* start = str;
    while (isspace((unsigned char)*start)) start++;
    char* end = start + strlen(start) - 1;
    while (end > start && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    if (start != str) memmove(str, start, strlen(start) + 1);
}

// --- CONNECTION ---
budc_device* budc_connect(const char* port_name) {
    if (BUDC_DEBUG) printf("DEBUG: Connecting to %s...\n", port_name);
    struct sp_port* port;
    if (sp_get_port_by_name(port_name, &port) != SP_OK) return NULL;
    if (sp_open(port, SP_MODE_READ_WRITE) != SP_OK) {
        sp_free_port(port);
        return NULL;
    }

    if (BUDC_DEBUG) printf("DEBUG: Port opened. Configuring...\n");
    sp_set_baudrate(port, 9600);
    sp_set_bits(port, 8);
    sp_set_parity(port, SP_PARITY_NONE);
    sp_set_stopbits(port, 1);
    sp_set_flowcontrol(port, SP_FLOWCONTROL_NONE);

    if (BUDC_DEBUG) printf("DEBUG: Asserting DTR and RTS lines.\n");
    sp_set_dtr(port, SP_DTR_ON);
    sp_set_rts(port, SP_RTS_ON);

    budc_device* dev = malloc(sizeof(budc_device));
    if (!dev) { sp_close(port); sp_free_port(port); return NULL; }
    dev->port = port;

    if (BUDC_DEBUG) printf("DEBUG: Flushing buffers post-configuration.\n");
    sp_flush(port, SP_BUF_BOTH);
    scpi_delay(50); // A small delay after setup is good practice.

    return dev;
}

void budc_disconnect(budc_device* dev) {
    if (dev) {
        if (dev->port) {
            if (BUDC_DEBUG) printf("DEBUG: Closing port.\n");
            sp_close(dev->port);
            sp_free_port(dev->port);
        }
        free(dev);
    }
}

bool budc_is_connected(budc_device* dev) { return dev != NULL && dev->port != NULL; }

// --- REVISED RAW COMMAND WITH WINDOWS-SPECIFIC DELAY ---
int budc_send_raw_command(budc_device* dev, const char* command, char* response, size_t response_len) {
    if (!budc_is_connected(dev)) return -1;
    
    sp_flush(dev->port, SP_BUF_BOTH);
    
    char full_command[256];
    snprintf(full_command, sizeof(full_command), "%s%s", command, COMMAND_TERMINATOR);
    size_t command_len = strlen(full_command);

    if (BUDC_DEBUG) printf("\nDEBUG: Writing command: '%s'\n", command);
    int write_result = sp_blocking_write(dev->port, full_command, command_len, READ_TIMEOUT_MS);
    if (BUDC_DEBUG) printf("DEBUG: sp_blocking_write returned: %d (wrote %d of %zu bytes)\n", write_result, write_result, command_len);

    if (write_result < (int)command_len) {
        if (BUDC_DEBUG) fprintf(stderr, "DEBUG: Write failed or timed out.\n");
        return -1;
    }

    if (strchr(command, '?')) {
        if (!response || response_len == 0) return -1;
        response[0] = '\0';
        
        // --- THE FINAL FIX ---
        // Add a small delay ONLY on Windows to allow the device to process
        // slower commands like FREQ? or TEMP? before we try to read.
        #ifdef _WIN32
            if (BUDC_DEBUG) printf("DEBUG: Applying Windows-specific pre-read delay (100ms).\n");
            scpi_delay(100);
        #endif

        if (BUDC_DEBUG) printf("DEBUG: Attempting to read response...\n");
        int bytes_read = sp_blocking_read_next(dev->port, response, response_len - 1, READ_TIMEOUT_MS);
        if (BUDC_DEBUG) printf("DEBUG: sp_blocking_read_next returned %d bytes.\n", bytes_read);

        if (bytes_read > 0) {
            response[bytes_read] = '\0';
            trim_whitespace(response);
            if (BUDC_DEBUG) printf("DEBUG: Response after trim: '%s'\n", response);
            if (strlen(response) == 0) return -1;
        } else {
            return -1;
        }
    }
    return 0;
}

// --- ALL GETTER, SETTER, AND HIGH-LEVEL FUNCTIONS REMAIN THE SAME ---
int budc_find_ports(serial_port_info** port_list) {
    struct sp_port** ports;
    if (sp_list_ports(&ports) != SP_OK) return -1;
    int count = 0;
    for (int i = 0; ports[i] != NULL; i++) count++;
    if (count == 0) { *port_list = NULL; sp_free_port_list(ports); return 0; }
    *port_list = calloc(count, sizeof(serial_port_info));
    if (!(*port_list)) { sp_free_port_list(ports); return -1; }
    for (int i = 0; i < count; i++) {
        strncpy((*port_list)[i].name, sp_get_port_name(ports[i]), sizeof((*port_list)[i].name) - 1);
        char* desc = sp_get_port_description(ports[i]);
        if (desc) strncpy((*port_list)[i].description, desc, sizeof((*port_list)[i].description) - 1);
    }
    sp_free_port_list(ports);
    return count;
}

int budc_get_identity(budc_device* dev, char* buffer, size_t len) {
    for (int i = 0; i < MAX_RETRIES; i++) {
        if (budc_send_raw_command(dev, "*IDN?", buffer, len) == 0 && strlen(buffer) > 5) return 0;
        scpi_delay(100);
    }
    return -1;
}

int budc_get_frequency_ghz(budc_device* dev, double* freq_ghz) {
    char response[64];
    for (int i = 0; i < MAX_RETRIES; i++) {
        if (budc_send_raw_command(dev, "FREQ?", response, sizeof(response)) == 0) {
            *freq_ghz = atof(response) / 1e9;
            return 0;
        }
        scpi_delay(100);
    }
    return -1;
}

int budc_get_lock_status(budc_device* dev, bool* is_locked) {
    char response[16];
    for (int i = 0; i < MAX_RETRIES; i++) {
        if (budc_send_raw_command(dev, "LOCK?", response, sizeof(response)) == 0) {
            *is_locked = (atoi(response) == 1);
            return 0;
        }
        scpi_delay(100);
    }
    return -1;
}

int budc_get_temperature_c(budc_device* dev, float* temp_c) {
    char response[64];
    float temp_value;
    for (int i = 0; i < 5; i++) {
        if (budc_send_raw_command(dev, "TEMP?", response, sizeof(response)) == 0) {
            char* num_start = response;
            while (*num_start && !isdigit((unsigned char)*num_start) && *num_start != '-' && *num_start != '.') {
                num_start++;
            }
            if (*num_start) {
                temp_value = atof(num_start);
                if (temp_value >= -50.0f && temp_value <= 150.0f) {
                    if (temp_value != 0.0f || i == 4) {
                        *temp_c = temp_value;
                        return 0;
                    }
                }
            }
        }
        scpi_delay(250);
    }
    return -1;
}

int budc_get_power_level(budc_device* dev, int* power_level) {
    char response[16];
    for (int i = 0; i < MAX_RETRIES; i++) {
        if (budc_send_raw_command(dev, "PWR?", response, sizeof(response)) == 0) {
            *power_level = atoi(response);
            return 0;
        }
        scpi_delay(100);
    }
    return -1;
}

int budc_set_frequency_ghz(budc_device* dev, double freq_ghz) {
    char command[64]; snprintf(command, sizeof(command), "FREQ %.10gGHZ", freq_ghz);
    return budc_send_raw_command(dev, command, NULL, 0);
}
int budc_set_frequency_mhz(budc_device* dev, double freq_mhz) {
    char command[64]; snprintf(command, sizeof(command), "FREQ %.10gMHZ", freq_mhz);
    return budc_send_raw_command(dev, command, NULL, 0);
}
int budc_set_frequency_hz(budc_device* dev, double freq_hz) {
    char command[64]; snprintf(command, sizeof(command), "FREQ %.10g", freq_hz);
    return budc_send_raw_command(dev, command, NULL, 0);
}
int budc_set_power_level(budc_device* dev, int power_level) {
    char command[32]; snprintf(command, sizeof(command), "PWR %d", power_level);
    return budc_send_raw_command(dev, command, NULL, 0);
}
int budc_save_settings(budc_device* dev) {
    return budc_send_raw_command(dev, "SAVE", NULL, 0);
}
int budc_preset(budc_device* dev) {
    return budc_send_raw_command(dev, "PRESET", NULL, 0);
}

int budc_wait_for_lock(budc_device* dev, unsigned int timeout_ms) {
    bool locked = false;
    clock_t start = clock();
    while (!locked) {
        if (budc_get_lock_status(dev, &locked) == 0 && locked) return 0;
        clock_t current = clock();
        if (((double)(current - start) / CLOCKS_PER_SEC * 1000.0) > timeout_ms) return -1;
        scpi_delay(200);
    }
    return 0;
}

int budc_set_frequency_and_wait(budc_device* dev, double freq_ghz, unsigned int timeout_ms) {
    if (budc_set_frequency_ghz(dev, freq_ghz) != 0) return -1;
    scpi_delay(200);
    return budc_wait_for_lock(dev, timeout_ms);
}