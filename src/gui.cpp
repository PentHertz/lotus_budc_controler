/* BUDC Controller - A cross-platform controller for BUC/BUDC devices.
 *
 * Copyright (C) 2025 Penthertz
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

#include <glad/glad.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <float.h> 

// Wrap C header for C++
extern "C" {
    #include "budc_scpi.h"
}

#ifdef _WIN32
    #include <windows.h>
#else
    #include <unistd.h>
#endif

// AppState remains the same
typedef struct {
    budc_device* dev;
    serial_port_info* port_list;
    int port_count;
    int selected_port_idx;
    bool is_connected;
    char identity[256];
    char serial_number[64];
    char fw_version[64];
    double current_freq_ghz;
    bool is_locked;
    float temperature_c;
    int power_level;
    bool temp_supported;
    double target_freq_ghz;
    int target_power_level;
    char scpi_command[256];
    char scpi_log[4096];
    time_t last_update_time;
    bool auto_refresh_enabled;
} AppState;

void safe_delay(int milliseconds) {
    #ifdef _WIN32
        Sleep(milliseconds);
    #else
        usleep(milliseconds * 1000);
    #endif
}

void refresh_port_list(AppState* state) {
    // Free existing port list
    if (state->port_list) {
        free(state->port_list);
        state->port_list = NULL;
    }
    
    // Reset selection
    state->selected_port_idx = -1;
    
    // Get new port list
    state->port_count = budc_find_ports(&state->port_list);
    
    printf("Refreshed port list: found %d ports\n", state->port_count);
}

void update_frequency_only(AppState* state) {
    if (!state->is_connected) return;
    if (budc_get_frequency_ghz(state->dev, &state->current_freq_ghz) == 0) {
        state->target_freq_ghz = state->current_freq_ghz;
    }
}

void update_power_only(AppState* state) {
    if (!state->is_connected) return;
    if (budc_get_power_level(state->dev, &state->power_level) == 0) {
        state->target_power_level = state->power_level;
    }
}

void update_device_status(AppState* state) {
    if (!state->is_connected) return;
    update_frequency_only(state);
    safe_delay(50);
    budc_get_lock_status(state->dev, &state->is_locked);
    safe_delay(50);
    if (budc_get_temperature_c(state->dev, &state->temperature_c) == 0) {
        state->temp_supported = true;
    } else {
        state->temp_supported = false;
        state->temperature_c = -999.0f;
    }
    safe_delay(50);
    update_power_only(state);
    state->last_update_time = time(NULL);
}

void update_all_values(AppState* state) {
    if (!state->is_connected) return;
    if (budc_get_identity(state->dev, state->identity, sizeof(state->identity)) == 0) {
        // Parse the identity string: Company,Product,Serial,Firmware
        char company[64] = "";
        char product[64] = "";
        
        sscanf(state->identity, "%63[^,],%63[^,],%63[^,],%63s", 
               company, product, state->serial_number, state->fw_version);
               
        // Store company and product for display
        snprintf(state->identity, sizeof(state->identity), "%s %s", company, product);
    } else {
        strcpy(state->identity, "Error: Failed to read IDN");
        state->serial_number[0] = '\0';
        state->fw_version[0] = '\0';
    }
    safe_delay(100);
    update_device_status(state);
}

void init_app_state(AppState* state) {
    memset(state, 0, sizeof(AppState));
    state->selected_port_idx = -1;
    state->port_list = NULL;
    state->port_count = budc_find_ports(&state->port_list);
    state->auto_refresh_enabled = false;
}

void cleanup_app_state(AppState* state) {
    if (state->is_connected) budc_disconnect(state->dev);
    if (state->port_list) free(state->port_list);
}

void render_gui(AppState* state);

int main(int argc, char* argv[]) {
    glfwInit();
    const char* glsl_version = "#version 330";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* window = glfwCreateWindow(1024, 768, "BUDC Controller", NULL, NULL);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    
    // Initialize GLAD - using the loader function
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        printf("Failed to initialize GLAD\n");
        return -1;
    }

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    AppState state;
    init_app_state(&state);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        render_gui(&state);
        ImGui::Render();

        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        
        glfwSwapBuffers(window);
    }

    cleanup_app_state(&state);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

void render_gui(AppState* state) {
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_Always);
    ImGui::Begin("BUDC Control Panel", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

    if (ImGui::CollapsingHeader("Connection", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (state->is_connected) {
            ImGui::Text("Connected to: %s", state->port_list[state->selected_port_idx].name);
            ImGui::SameLine(0, 20);
            if (ImGui::Button("Disconnect")) {
                budc_disconnect(state->dev);
                state->is_connected = false;
                state->dev = NULL;
            }
        } else {
            if (state->port_count > 0) {
                const char** items = (const char**)malloc(state->port_count * sizeof(char*));
                for(int i = 0; i < state->port_count; i++) items[i] = state->port_list[i].name;
                ImGui::Combo("Serial Port", &state->selected_port_idx, items, state->port_count, 4);
                free(items);
            } else {
                ImGui::Text("No serial ports found.");
            }
            ImGui::SameLine(0, 20);
            if (ImGui::Button("Connect")) {
                if (state->selected_port_idx >= 0) {
                    state->dev = budc_connect(state->port_list[state->selected_port_idx].name);
                    if (state->dev) {
                        state->is_connected = true;
                        safe_delay(500);
                        update_all_values(state);
                    }
                }
            }
            ImGui::SameLine(0, 10);
            if (ImGui::Button("Refresh Ports")) {
                refresh_port_list(state);
            }
        }
    }
    
    if (!state->is_connected) {
        ImGui::End();
        return;
    }
    
    if (state->auto_refresh_enabled && difftime(time(NULL), state->last_update_time) > 10.0) {
        update_device_status(state);
    }
    
    if (ImGui::CollapsingHeader("Device Information", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Company & Product: %s", state->identity);
        ImGui::Text("Serial Number: %s", state->serial_number);
        ImGui::Text("Firmware Version: %s", state->fw_version);
        ImGui::Separator();
        ImGui::Text("Current LO Freq: %.4f GHz", state->current_freq_ghz);
        ImGui::Text("PLL Lock Status: "); ImGui::SameLine();
        ImGui::TextColored(state->is_locked ? ImVec4(0,1,0,1) : ImVec4(1,0,0,1), state->is_locked ? "LOCKED" : "UNLOCKED");
        if (state->temp_supported) ImGui::Text("Temperature: %.1f C", state->temperature_c);
        else ImGui::Text("Temperature: Not Supported");
        ImGui::Text("Power Level: %d", state->power_level);
        ImGui::Separator();
        ImGui::Checkbox("Auto-refresh (10s)", &state->auto_refresh_enabled);
    }
    
    if (ImGui::CollapsingHeader("Controls", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::InputDouble("Target Freq (GHz)", &state->target_freq_ghz, 0.1, 1.0, "%.4f");
        ImGui::SameLine();
        if (ImGui::Button("Set Freq")) {
            if (budc_set_frequency_ghz(state->dev, state->target_freq_ghz) == 0) {
                safe_delay(250); // Give device time to process
                update_frequency_only(state);
            }
        }
        
        ImGui::InputInt("Target Power Level", &state->target_power_level, 1, 5);
        ImGui::SameLine();
        if (ImGui::Button("Set Power")) {
            if (budc_set_power_level(state->dev, state->target_power_level) == 0) {
                safe_delay(250); // Give device time to process
                update_power_only(state);
            }
        }
        
        ImGui::Separator();
        if (ImGui::Button("PRESET")) { budc_preset(state->dev); safe_delay(200); update_all_values(state); }
        ImGui::SameLine();
        if (ImGui::Button("SAVE")) { budc_save_settings(state->dev); }
        ImGui::SameLine();
        if (ImGui::Button("Refresh All")) { update_all_values(state); }
    }
    
    if (ImGui::CollapsingHeader("Direct SCPI Command")) {
        bool enter_pressed = ImGui::InputText("Command", state->scpi_command, sizeof(state->scpi_command), ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        if (ImGui::Button("Send") || enter_pressed) {
            char response[512] = {0};
            char log_entry[1024];
            budc_send_raw_command(state->dev, state->scpi_command, response, sizeof(response));
            snprintf(log_entry, sizeof(log_entry), ">> %s\n<< %s\n\n", state->scpi_command, strlen(response) > 0 ? response : "(no response)");
            strncat(state->scpi_log, log_entry, sizeof(state->scpi_log) - strlen(state->scpi_log) - 1);
            state->scpi_command[0] = '\0';
            safe_delay(100);
            update_device_status(state); // Update everything after a manual command
        }
        ImGui::InputTextMultiline("Log", state->scpi_log, sizeof(state->scpi_log), ImVec2(-FLT_MIN, 150), ImGuiInputTextFlags_ReadOnly);
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.0f);
    }
    ImGui::End();
}