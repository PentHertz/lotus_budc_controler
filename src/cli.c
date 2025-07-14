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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void print_usage() {
    printf("BUDC Command Line Interface\n");
    printf("Usage:\n");
    printf("  budc_cli --list                           List available serial ports\n");
    printf("  budc_cli --port <name> [COMMANDS]\n\n");
    printf("Commands:\n");
    printf("  --status              Get a full status report\n");
    printf("  --cmd \"<cmd>\"           Send raw SCPI command\n");
    printf("  --freq <ghz>          Set frequency in GHz\n");
    printf("  --freq-hz <hz>        Set frequency in Hz\n");
    printf("  --freq-mhz <mhz>      Set frequency in MHz\n");
    printf("  --power <level>       Set power level\n");
    printf("  --get-freq            Get current frequency\n");
    printf("  --get-power           Get current power level\n");
    printf("  --get-temp            Get temperature\n");
    printf("  --get-lock            Get lock status\n");
    printf("  --preset              Reset to preset values\n");
    printf("  --save                Save settings to flash\n");
    printf("  --wait-lock           Wait for PLL to lock (5s timeout) after a set command\n");
    printf("\nExamples:\n");
    printf("  budc_cli --port /dev/ttyACM0 --status\n");
    printf("  budc_cli --port COM3 --freq 5.5\n");
    printf("  budc_cli --port COM3 --freq 2.4 --wait-lock\n");
}

int main(int argc, char* argv[]) {
    const char* port_name = NULL;
    const char* raw_command = NULL;
    bool list_ports = false, get_status = false, get_freq = false;
    bool get_power = false, get_temp = false, get_lock = false;
    bool do_preset = false, do_save = false, wait_for_lock_after_set = false;
    
    double set_freq_ghz = -1.0, set_freq_hz = -1.0, set_freq_mhz = -1.0;
    int set_power_level = -1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--list") == 0) list_ports = true;
        else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) port_name = argv[++i];
        else if (strcmp(argv[i], "--cmd") == 0 && i + 1 < argc) raw_command = argv[++i];
        else if (strcmp(argv[i], "--status") == 0) get_status = true;
        else if (strcmp(argv[i], "--freq") == 0 && i + 1 < argc) set_freq_ghz = atof(argv[++i]);
        else if (strcmp(argv[i], "--freq-hz") == 0 && i + 1 < argc) set_freq_hz = atof(argv[++i]);
        else if (strcmp(argv[i], "--freq-mhz") == 0 && i + 1 < argc) set_freq_mhz = atof(argv[++i]);
        else if (strcmp(argv[i], "--power") == 0 && i + 1 < argc) set_power_level = atoi(argv[++i]);
        else if (strcmp(argv[i], "--get-freq") == 0) get_freq = true;
        else if (strcmp(argv[i], "--get-power") == 0) get_power = true;
        else if (strcmp(argv[i], "--get-temp") == 0) get_temp = true;
        else if (strcmp(argv[i], "--get-lock") == 0) get_lock = true;
        else if (strcmp(argv[i], "--preset") == 0) do_preset = true;
        else if (strcmp(argv[i], "--save") == 0) do_save = true;
        else if (strcmp(argv[i], "--wait-lock") == 0) wait_for_lock_after_set = true;
        else if (strcmp(argv[i], "--help") == 0) { print_usage(); return 0; }
    }

    if (list_ports) {
        serial_port_info* port_list = NULL;
        int count = budc_find_ports(&port_list);
        if (count < 0) { fprintf(stderr, "Error listing ports.\n"); return 1; }
        if (count == 0) printf("No serial ports found.\n");
        else {
            printf("Found %d serial port(s):\n", count);
            for (int i = 0; i < count; i++) printf("  %s (%s)\n", port_list[i].name, port_list[i].description);
        }
        free(port_list);
        return 0;
    }

    if (!port_name) { print_usage(); return 0; }

    budc_device* dev = budc_connect(port_name);
    if (!dev) { fprintf(stderr, "Failed to connect to %s\n", port_name); return 1; }
    int result = 0;

    if (set_freq_ghz >= 0) {
        printf("Setting frequency to %.4f GHz...\n", set_freq_ghz);
        if (budc_set_frequency_ghz(dev, set_freq_ghz) != 0) { fprintf(stderr, "Failed to set frequency.\n"); result = 1; }
    } else if (set_freq_mhz >= 0) {
        printf("Setting frequency to %.3f MHz...\n", set_freq_mhz);
        if (budc_set_frequency_mhz(dev, set_freq_mhz) != 0) { fprintf(stderr, "Failed to set frequency.\n"); result = 1; }
    } else if (set_freq_hz >= 0) {
        printf("Setting frequency to %.0f Hz...\n", set_freq_hz);
        if (budc_set_frequency_hz(dev, set_freq_hz) != 0) { fprintf(stderr, "Failed to set frequency.\n"); result = 1; }
    }

    if (set_power_level >= 0) {
        printf("Setting power to %d...\n", set_power_level);
        if (budc_set_power_level(dev, set_power_level) != 0) { fprintf(stderr, "Failed to set power.\n"); result = 1; }
    }

    if (wait_for_lock_after_set) {
        printf("Waiting for PLL to lock (5 second timeout)...\n");
        if (budc_wait_for_lock(dev, 5000) == 0) printf("PLL locked.\n");
        else { fprintf(stderr, "PLL lock timeout.\n"); result = 1; }
    }

    if (do_preset) {
        printf("Executing PRESET...\n");
        if (budc_preset(dev) != 0) { fprintf(stderr, "Failed to execute preset.\n"); result = 1; }
    }
    if (do_save) {
        printf("Executing SAVE...\n");
        if (budc_save_settings(dev) != 0) { fprintf(stderr, "Failed to save settings.\n"); result = 1; }
    }
    if (get_freq) {
        double freq;
        if (budc_get_frequency_ghz(dev, &freq) == 0) printf("Frequency: %.4f GHz\n", freq);
        else { fprintf(stderr, "Failed to get frequency.\n"); result = 1; }
    }
    if (get_power) {
        int power;
        if (budc_get_power_level(dev, &power) == 0) printf("Power Level: %d\n", power);
        else { fprintf(stderr, "Failed to get power level.\n"); result = 1; }
    }
    if (get_temp) {
        float temp;
        if (budc_get_temperature_c(dev, &temp) == 0) printf("Temperature: %.1f C\n", temp);
        else printf("Temperature: Not Supported or failed to read.\n");
    }
    if (get_lock) {
        bool locked;
        if (budc_get_lock_status(dev, &locked) == 0) printf("Lock Status: %s\n", locked ? "LOCKED" : "UNLOCKED");
        else { fprintf(stderr, "Failed to get lock status.\n"); result = 1; }
    }
    if (raw_command) {
        char response[512] = {0};
        printf("Sending raw command: %s\n", raw_command);
        if (budc_send_raw_command(dev, raw_command, response, sizeof(response)) == 0) {
            printf("Response: %s\n", strlen(response) > 0 ? response : "(no response)");
        } else { fprintf(stderr, "Failed to send raw command.\n"); result = 1; }
    }
    if (get_status) {
        char identity[256] = "N/A", serial[64] = "N/A", fw[64] = "N/A";
        double freq_ghz = 0.0; bool locked = false, temp_ok = false;
        float temp_c = 0.0; int power = 0;

        budc_get_identity(dev, identity, sizeof(identity));
        sscanf(identity, "%*[^,],%*[^,],%63[^,],%63s", serial, fw);
        budc_get_frequency_ghz(dev, &freq_ghz);
        budc_get_lock_status(dev, &locked);
        temp_ok = (budc_get_temperature_c(dev, &temp_c) == 0);
        budc_get_power_level(dev, &power);

        printf("\n--- BUDC Status Report ---\n");
        printf("  Identity:      %s\n", identity);
        printf("  Serial Number: %s\n", serial);
        printf("  Firmware:      %s\n", fw);
        printf("  Frequency:     %.4f GHz\n", freq_ghz);
        printf("  Lock Status:   %s\n", locked ? "LOCKED" : "UNLOCKED");
        if(temp_ok) printf("  Temperature:   %.1f C\n", temp_c); else printf("  Temperature:   Not Supported\n");
        printf("  Power Level:   %d\n", power);
        printf("--------------------------\n");
    }

    budc_disconnect(dev);
    return result;
}