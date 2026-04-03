#include "OFS_DevicePanel.h"

#include "../OpenFunscripter.h"
#include "../state/DeviceState.h"
#include "OFS_DeviceManager.h"
#include "OFS_EventSystem.h"
#include "OFS_FileLogging.h"
#include "OFS_Util.h"

#include <imgui.h>
#include <imgui_stdlib.h>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

// Device Panel UI
void DevicePanel::Show(bool* open) noexcept
{
    auto& state = DeviceState::State(stateHandle);
    auto* deviceMgr = OpenFunscripter::ptr->deviceMgr.get();
    
    showWindow = *open;
    if (!showWindow) {
        return;
    }
    
    if (!ImGui::Begin("Device Control###DEVICE_PANEL", &showWindow, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        *open = showWindow;
        return;
    }
    
    // Device type selection - Simplified to Bluetooth LE only
    ImGui::Text("Device: Bluetooth LE");
    ImGui::SameLine();
    
    // Set device type to Bluetooth LE
    if (deviceMgr && deviceMgr->GetDeviceType() != DeviceType::BluetoothLE) {
        deviceMgr->SetDeviceType(DeviceType::BluetoothLE);
    }
    
    // Auto-enable WebSocket API for Bluetooth LE
    auto* app = OpenFunscripter::ptr;
    
    if (app && app->webApi) {
        // Check if the server is actually running by trying to start it
        // StartServer() returns true if already running or successfully started
        app->webApi->StartServer();
    }
    
    OFS::Tooltip("Uses Web Bluetooth to connect to sex toys without Windows pairing");
    
    // Connection status
    ImGui::Text("Connection Status: ");
    ImGui::SameLine();
    if (deviceMgr && deviceMgr->IsConnected()) {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Connected");
    } else if (deviceMgr && deviceMgr->IsInitialized()) {
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Searching...");
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Not Connected");
    }
    
    ImGui::Separator();
    
    // Device Enable/Disable
    bool enabled = state.enabled;
    if (ImGui::Checkbox("Enable Device Control", &enabled)) {
        deviceMgr->SetEnabled(enabled);
    }
    ImGui::SameLine();
    OFS::Tooltip("When enabled, the device will respond to the funscript during playback");
    
    // Connection buttons
    // Always show Search & Connect for Bluetooth LE (even if connected, to allow re-pairing)
    if (deviceMgr && deviceMgr->GetDeviceType() == DeviceType::BluetoothLE) {
        if (ImGui::Button("Search & Connect##btn")) {
            // Launch browser with Web Bluetooth connector
#ifdef WIN32
            // First ensure the server is running
            if (app && app->webApi) {
                app->webApi->StartServer();
            }
            
            ShellExecuteA(NULL, "open", "http://localhost:8080/bluetooth-connector", NULL, NULL, SW_SHOWNORMAL);
#endif
            // Try to set connected state since WebSocket may already be connected
            if (deviceMgr) {
                deviceMgr->SetConnected(true);
            }
        }
        ImGui::SameLine();
        OFS::Tooltip("Opens browser to select Bluetooth device without Windows pairing");
        
        if (deviceMgr->IsConnected()) {
            ImGui::SameLine();
            if (ImGui::Button("Disconnect##btn")) {
                deviceMgr->Disconnect();
            }
        }
    } else if (!deviceMgr || !deviceMgr->IsConnected()) {
        if (ImGui::Button("Connect to Device")) {
            if (deviceMgr) {
                bool success = deviceMgr->EnsureConnected();
                if (success) {
                    deviceMgr->StartScanning();
                } else {
                    LOG_ERROR("Failed to connect to device");
                }
            }
        }
        ImGui::SameLine();
        OFS::Tooltip("Connect to the selected device type");
    } else {
        if (ImGui::Button("Disconnect")) {
            if (deviceMgr) {
                deviceMgr->Disconnect();
            }
        }
    }
    
    ImGui::Separator();
    
    // Device scanning
    if (deviceMgr && deviceMgr->IsConnected()) {
        if (!state.scanning) {
            if (ImGui::Button("Scan for Devices")) {
                deviceMgr->StartScanning();
            }
        } else {
            ImGui::Text("Scanning for devices...");
            ImGui::SameLine();
            if (ImGui::Button("Stop")) {
                deviceMgr->StopScanning();
            }
        }
        
        ImGui::Separator();
        
        // Device selection
        int deviceCount = deviceMgr->GetDeviceCount();
        if (deviceCount > 0) {
            ImGui::Text("Available Devices:");
            ImGui::PushItemWidth(200.f);
            
            const char* preview = state.selectedDeviceIndex >= 0 && state.selectedDeviceIndex < deviceCount 
                ? deviceMgr->GetDeviceName(state.selectedDeviceIndex) 
                : "Select a device...";
            
            if (ImGui::BeginCombo("##DeviceSelect", preview)) {
                for (int i = 0; i < deviceCount; i++) {
                    const char* deviceName = deviceMgr->GetDeviceName(i);
                    bool isSelected = (state.selectedDeviceIndex == i);
                    if (ImGui::Selectable(deviceName, isSelected)) {
                        deviceMgr->SetSelectedDevice(i);
                        state.selectedDeviceIndex = i;
                    }
                    if (isSelected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::PopItemWidth();
        } else {
            ImGui::TextDisabled("No devices found.");
        }
        
        ImGui::Separator();
        
        // Device Settings
        ImGui::Text("Device Settings:");
        
        // Pause with video
        bool pauseWithVideo = state.pauseWithVideo;
        if (ImGui::Checkbox("Stop device when video pauses", &pauseWithVideo)) {
            state.pauseWithVideo = pauseWithVideo;
        }
        
        // Position range
        ImGui::SliderInt("Min Position", &state.minPosition, 0, 100, "%d%%");
        ImGui::SliderInt("Max Position", &state.maxPosition, 0, 100, "%d%%");
        
        // Ensure min <= max
        if (state.minPosition > state.maxPosition) {
            state.maxPosition = state.minPosition;
        }
        
        // Sync delay (lookahead to compensate for device latency)
        ImGui::SliderFloat("Sync Delay (s)", &state.syncDelay, 0.0f, 2.0f, "%.2f");
        ImGui::SameLine();
        OFS::Tooltip("Adds a lookahead delay to compensate for device playing late.\nIncrease if your device lags behind the video.");
        
        // Sync with playback speed
        bool syncSpeed = state.syncPlaybackSpeed;
        if (ImGui::Checkbox("Sync with playback speed", &syncSpeed)) {
            state.syncPlaybackSpeed = syncSpeed;
        }
        
        // Device to script sync (reverse direction)
        ImGui::Separator();
        ImGui::Text("Device to Script Sync:");
        bool syncDeviceToScript = state.syncDeviceToScript;
        if (ImGui::Checkbox("Update funscript from device##syncToggle", &syncDeviceToScript)) {
            state.syncDeviceToScript = syncDeviceToScript;
            if (deviceMgr) {
                deviceMgr->EnableDeviceToScriptSync(syncDeviceToScript);
            }
        }
        ImGui::SameLine();
        OFS::Tooltip("When enabled, moving your device while the video plays adds new funscript points\nat the current playback time. Toggle off to stop recording.");
        
        // Show current status
        if (syncDeviceToScript && deviceMgr && deviceMgr->IsConnected()) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "[Active]");
        } else if (!syncDeviceToScript) {
            ImGui::SameLine();
            ImGui::TextDisabled("[Off]");
        }
        
    } else {
        ImGui::TextDisabled("Connect to enable device control");
        
        // Show help text based on device type
        if (deviceMgr) {
            ImGui::Separator();
            ImGui::TextWrapped("Help:");
            switch (deviceMgr->GetDeviceType()) {
                case DeviceType::XInput:
                    ImGui::TextWrapped("Connect an Xbox or compatible gamepad via USB or Bluetooth.");
                    break;
                case DeviceType::Handy:
                    ImGui::TextWrapped("The Handy must be connected to the internet via WiFi. "
                        "It connects to handyfeeling.com servers. Make sure your Handy is online.");
                    break;
                case DeviceType::Lovense:
                    ImGui::TextWrapped("Use the Lovense Connect app on your phone/tablet. "
                        "Run the app and connect to the same network as this PC. "
                        "The app should show the PC IP on port 34567.");
                    break;
                case DeviceType::Hismith:
                    ImGui::TextWrapped("Hismith machines with WiFi can be controlled via Remote Play. "
                        "Get the game token from the Hismith app.");
                    break;
                case DeviceType::BluetoothLE:
                    ImGui::TextWrapped("For direct Bluetooth without Windows pairing:");
                    ImGui::TextColored(ImVec4(0.3f, 0.7f, 1.0f, 1.0f), "Click Search & Connect to open browser selector");
                    ImGui::TextWrapped("Or use Fun Sync Player for Bluetooth, then use WiFi/LAN here.");
                default:
                    ImGui::TextWrapped("Select a device type above and click Connect.");
                    break;
            }
        }
    }
    
    ImGui::Separator();
    
    // Manual controls (for testing)
    if (deviceMgr && deviceMgr->IsConnected() && state.selectedDeviceIndex >= 0) {
        ImGui::Text("Manual Test Controls:");
        
        if (ImGui::Button("Test: Move to 50%")) {
            deviceMgr->SendPosition(50.0f);
        }
        ImGui::SameLine();
        if (ImGui::Button("Test: Stop")) {
            deviceMgr->StopDevice();
        }
    }
    
    ImGui::End();
    *open = showWindow;
}

// Device panel constructor
DevicePanel::DevicePanel() noexcept
{
    stateHandle = OFS_AppState<DeviceState>::Register(DeviceState::StateName);
    showWindow = false;
}

// Initialize the device panel
void DevicePanel::Init(OFS_DeviceManager* mgr) noexcept
{
    // Initialization logic if needed
    // For now, the panel is ready to be shown/toggled
    LOG_INFO("Device panel initialized");
}

// Toggle window visibility
void DevicePanel::Toggle() noexcept
{
    showWindow = !showWindow;
}

// Show window (private, called by Show)
void DevicePanel::ShowWindow() noexcept
{
    // Internal implementation
}
