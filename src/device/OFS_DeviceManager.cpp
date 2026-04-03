#include "OFS_DeviceManager.h"

#include "../OpenFunscripter.h"
#include "../state/DeviceState.h"
#include "../../OFS-lib/Funscript/Funscript.h"
#include "FunscriptAction.h"
#include "OFS_VideoplayerEvents.h"
#include "OFS_EventSystem.h"
#include "OFS_FileLogging.h"

#include "adapters/OFS_HandyAdapter.h"
#include "adapters/OFS_LovenseAdapter.h"
#include "adapters/OFS_HismithAdapter.h"
#include "adapters/OFS_BluetoothLEAdapter.h"

#include "nlohmann/json.hpp"

#include <thread>
#include <chrono>
#include <mutex>
#include <atomic>
#include <iostream>
#include <sstream>
#include <algorithm>

#ifdef WIN32
#include <windows.h>
#include <xinput.h>
#pragma comment(lib, "xinput.lib")
#endif

using json = nlohmann::json;

OFS_DeviceManager::OFS_DeviceManager() noexcept
{
    stateHandle = OFS_AppState<DeviceState>::Register(DeviceState::StateName);
}

OFS_DeviceManager::~OFS_DeviceManager() noexcept
{
    Shutdown();
}

void OFS_DeviceManager::Init(OFS_Videoplayer* player) noexcept
{
    this->player = player;
    
    // Initialize device adapters
    handyAdapter = std::make_unique<OFS_HandyAdapter>();
    lovenseAdapter = std::make_unique<OFS_LovenseAdapter>();
    hismithAdapter = std::make_unique<OFS_HismithAdapter>();
    bluetoothAdapter = std::make_unique<OFS_BluetoothLEAdapter>();
    
    // Register callback for WebSocket Bluetooth connection
    if (bluetoothAdapter) {
        bluetoothAdapter->SetWebSocketConnectCallback([this](const std::string& deviceName) {
            // When WebSocket connects, also update DeviceManager state
            OnWebSocketDeviceConnected(deviceName);
        });
        
        // Register callback for device position changes (for reverse sync)
        bluetoothAdapter->SetPositionCallback([this](float position) {
            // Queue event for device position change
            EV::Enqueue<DevicePositionChangedEvent>(position);
        });
    }
    
    // Mark as initialized
    initialized.store(true);
    
    // Register event listeners
    playPauseListener = EV::MakeUnsubscibeFn(PlayPauseChangeEvent::EventType,
        EV::Queue().appendListener(PlayPauseChangeEvent::EventType,
            PlayPauseChangeEvent::HandleEvent(EVENT_SYSTEM_BIND(this, &OFS_DeviceManager::OnPlayPauseChanged))));
        
    timeChangeListener = EV::MakeUnsubscibeFn(TimeChangeEvent::EventType,
        EV::Queue().appendListener(TimeChangeEvent::EventType,
            TimeChangeEvent::HandleEvent(EVENT_SYSTEM_BIND(this, &OFS_DeviceManager::OnTimeChanged))));
        
    speedChangeListener = EV::MakeUnsubscibeFn(PlaybackSpeedChangeEvent::EventType,
        EV::Queue().appendListener(PlaybackSpeedChangeEvent::EventType,
            PlaybackSpeedChangeEvent::HandleEvent(EVENT_SYSTEM_BIND(this, &OFS_DeviceManager::OnSpeedChanged))));
            
    devicePosListener = EV::MakeUnsubscibeFn(DevicePositionChangedEvent::EventType,
        EV::Queue().appendListener(DevicePositionChangedEvent::EventType,
            DevicePositionChangedEvent::HandleEvent(EVENT_SYSTEM_BIND(this, &OFS_DeviceManager::OnDevicePositionChangedEvent))));
        
    LOG_INFO("DeviceManager initialized - Multi-Device Support");
}

void OFS_DeviceManager::Shutdown() noexcept
{
    // Remove event listeners
    if (playPauseListener) playPauseListener();
    if (timeChangeListener) timeChangeListener();
    if (speedChangeListener) speedChangeListener();
    
    Disconnect();
    
    // Cleanup adapters
    handyAdapter.reset();
    lovenseAdapter.reset();
    hismithAdapter.reset();
    bluetoothAdapter.reset();
    
    LOG_INFO("DeviceManager shutdown");
}

bool OFS_DeviceManager::ConnectDevice() noexcept
{
    if (connected.load()) {
        return true;
    }
    
    // Connect based on device type
    switch (currentDeviceType) {
        case DeviceType::XInput:
            return ConnectXInput();
            
        case DeviceType::Handy:
            if (handyAdapter) {
                bool success = handyAdapter->Connect();
                if (success) {
                    connected.store(true);
                    DeviceInfo dev;
                    dev.deviceIndex = 0;
                    dev.deviceName = handyAdapter->GetDeviceName();
                    dev.supportedCommands = {"SetSpeed", "SetPosition"};
                    devices.push_back(dev);
                    SetSelectedDevice(0);  // Select the connected device
                    LOG_INFO("Connected to Handy");
                }
                return success;
            }
            return false;
            
        case DeviceType::Lovense:
            if (lovenseAdapter) {
                // Default: try localhost:34567 (Lovense Connect)
                bool success = lovenseAdapter->Connect("127.0.0.1", 34567);
                if (success) {
                    connected.store(true);
                    DeviceInfo dev;
                    dev.deviceIndex = 0;
                    dev.deviceName = lovenseAdapter->GetDeviceName();
                    dev.supportedCommands = {"Vibrate", "Rotate", "AirLevel"};
                    devices.push_back(dev);
                    SetSelectedDevice(0);  // Select the connected device
                    LOG_INFO("Connected to Lovense");
                }
                return success;
            }
            return false;
            
        case DeviceType::Hismith:
            if (hismithAdapter) {
                bool success = hismithAdapter->Connect();
                if (success) {
                    connected.store(true);
                    DeviceInfo dev;
                    dev.deviceIndex = 0;
                    dev.deviceName = hismithAdapter->GetDeviceName();
                    dev.supportedCommands = {"SetSpeed", "SetVibration", "SetPosition"};
                    devices.push_back(dev);
                    SetSelectedDevice(0);  // Select the connected device
                    LOG_INFO("Connected to Hismith");
                }
                return success;
            }
            return false;
            
        case DeviceType::BluetoothLE:
            if (bluetoothAdapter) {
                bool success = bluetoothAdapter->Connect();
                if (success) {
                    connected.store(true);
                    DeviceInfo dev;
                    dev.deviceIndex = 0;
                    dev.deviceName = bluetoothAdapter->GetDeviceName();
                    dev.supportedCommands = {"SetSpeed", "SetPosition", "SendPosition"};
                    devices.push_back(dev);
                    SetSelectedDevice(0);  // Select the connected device
                    LOGF_INFO("Connected to %s via Bluetooth LE", dev.deviceName.c_str());
                }
                return success;
            }
            return false;
            
        default:
            LOG_WARN("No device type selected");
            return false;
    }
}

void OFS_DeviceManager::Disconnect() noexcept
{
    connected.store(false);
    
    // Disconnect current device
    switch (currentDeviceType) {
        case DeviceType::Handy:
            if (handyAdapter) handyAdapter->Disconnect();
            break;
        case DeviceType::Lovense:
            if (lovenseAdapter) lovenseAdapter->Disconnect();
            break;
        case DeviceType::Hismith:
            if (hismithAdapter) hismithAdapter->Disconnect();
            break;
        case DeviceType::BluetoothLE:
            if (bluetoothAdapter) bluetoothAdapter->Disconnect();
            break;
        default:
            break;
    }
    
    auto& state = DeviceState::State(stateHandle);
    state.connected = false;
    devices.clear();
    state.selectedDeviceIndex = -1;
    
    LOG_INFO("Disconnected from device");
    EV::Enqueue<DeviceDisconnectedEvent>();
}

void OFS_DeviceManager::Update() noexcept
{
    // Poll device state if connected
    if (!connected.load() || !player) return;
    
    auto& state = DeviceState::State(stateHandle);
    if (state.selectedDeviceIndex < 0) return;
    
#ifdef WIN32
    if (currentDeviceType == DeviceType::XInput) {
        // Check if gamepad is still connected
        int deviceIndex = state.selectedDeviceIndex;
        
        XINPUT_STATE xinputState;
        ZeroMemory(&xinputState, sizeof(XINPUT_STATE));
        
        DWORD result = XInputGetState(deviceIndex, &xinputState);
        
        if (result != ERROR_SUCCESS) {
            LOG_WARN("Gamepad disconnected");
            Disconnect();
        }
    }
#endif
    
    // Check network device connection status periodically
    static int updateCounter = 0;
    updateCounter++;
    if (updateCounter >= 60) { // Check every ~1 second (60 frames)
        updateCounter = 0;
        
        switch (currentDeviceType) {
            case DeviceType::Handy:
                if (handyAdapter && !handyAdapter->IsDeviceOnline()) {
                    LOG_WARN("Handy disconnected");
                    Disconnect();
                }
                break;
                
            case DeviceType::Lovense:
                if (lovenseAdapter && !lovenseAdapter->IsConnected()) {
                    LOG_WARN("Lovense disconnected");
                    Disconnect();
                }
                break;
                
            case DeviceType::Hismith:
                if (hismithAdapter && !hismithAdapter->IsConnected()) {
                    LOG_WARN("Hismith disconnected");
                    Disconnect();
                }
                break;
                
            default:
                break;
        }
    }
}

bool OFS_DeviceManager::EnsureConnected() noexcept
{
    if (!initialized.load()) {
        LOG_ERROR("DeviceManager not initialized");
        return false;
    }
    
    if (connected.load()) {
        return true;
    }
    
    // Try to connect
    return ConnectDevice();
}

void OFS_DeviceManager::DisconnectDevice() noexcept
{
    Disconnect();
}

void OFS_DeviceManager::SetDeviceType(DeviceType type) noexcept
{
    // Disconnect from current device first
    if (connected.load()) {
        Disconnect();
    }
    
    currentDeviceType = type;
    LOGF_INFO("Device type changed to: %d", (int)type);
}

void OFS_DeviceManager::SetHandyServerUrl(const std::string& url) noexcept
{
    if (handyAdapter) {
        handyAdapter->SetServerUrl(url);
    }
}

void OFS_DeviceManager::SetLovenseServerAddress(const std::string& addr, int port) noexcept
{
    if (lovenseAdapter) {
        lovenseAdapter->SetServerAddress(addr);
        lovenseAdapter->SetServerPort(port);
    }
}

void OFS_DeviceManager::SetHismithServerAddress(const std::string& addr, int port, const std::string& token) noexcept
{
    if (hismithAdapter) {
        hismithAdapter->SetServerAddress(addr);
        hismithAdapter->SetPortNumber(port);
        hismithAdapter->SetGameToken(token);
    }
}

void OFS_DeviceManager::StartScanning() noexcept
{
    auto& state = DeviceState::State(stateHandle);
    state.scanning = true;
    
    devices.clear();
    
    switch (currentDeviceType) {
        case DeviceType::XInput:
            ScanXInputDevices();
            break;
            
        case DeviceType::Handy:
            // Handy is always available if connected
            if (handyAdapter && handyAdapter->IsConnected()) {
                DeviceInfo dev;
                dev.deviceIndex = 0;
                dev.deviceName = handyAdapter->GetDeviceName();
                devices.push_back(dev);
            }
            break;
            
        case DeviceType::Lovense:
            // Try to discover Lovense devices
            if (lovenseAdapter) {
                DeviceInfo dev;
                dev.deviceIndex = 0;
                dev.deviceName = "Lovense Toy";
                devices.push_back(dev);
            }
            break;
            
        case DeviceType::Hismith:
            if (hismithAdapter) {
                DeviceInfo dev;
                dev.deviceIndex = 0;
                dev.deviceName = "Hismith Machine";
                devices.push_back(dev);
            }
            break;
            
        case DeviceType::BluetoothLE:
            if (bluetoothAdapter) {
                auto deviceNames = OFS_BluetoothLEAdapter::ScanForDevices();
                if (!deviceNames.empty()) {
                    // Auto-connect to the first found device
                    bool success = bluetoothAdapter->Connect(deviceNames[0]);
                    if (success) {
                        connected.store(true);
                        DeviceInfo dev;
                        dev.deviceIndex = 0;
                        dev.deviceName = bluetoothAdapter->GetDeviceName();
                        dev.supportedCommands = {"SetSpeed", "SetPosition", "SendPosition"};
                        devices.push_back(dev);
                        SetSelectedDevice(0);  // Select the connected device
                        LOGF_INFO("Auto-connected to Bluetooth LE device: %s", dev.deviceName.c_str());
                        state.scanning = false;
                        EV::Enqueue<DeviceConnectedEvent>();
                        EV::Enqueue<DeviceListUpdatedEvent>();
                        return;
                    }
                }
                for (size_t i = 0; i < deviceNames.size(); i++) {
                    DeviceInfo dev;
                    dev.deviceIndex = (int)i;
                    dev.deviceName = deviceNames[i];
                    dev.supportedCommands = {"SetSpeed", "SetPosition"};
                    devices.push_back(dev);
                }
            }
            break;
            
        default:
            break;
    }
    
    state.scanning = false;
    EV::Enqueue<DeviceListUpdatedEvent>();
    
    LOGF_INFO("Found %d device(s)", (int)devices.size());
}

void OFS_DeviceManager::StopScanning() noexcept
{
    auto& state = DeviceState::State(stateHandle);
    state.scanning = false;
    LOG_INFO("Stopped scanning");
}

void OFS_DeviceManager::SendPosition(float position) noexcept
{
    auto& state = DeviceState::State(stateHandle);
    
    // Only log on state changes or issues
    FILE* f = nullptr;
    
    // Check if we should send this position
    if (!state.enabled || !connected.load() || state.selectedDeviceIndex < 0) {
        return;
    }
    
    // Skip only if exactly the same position
    if (std::abs(state.lastPosition - position) < 0.001f) {
        return;
    }
    state.lastPosition = position;
    
    // Send to appropriate device
    switch (currentDeviceType) {
        case DeviceType::Handy:
            if (handyAdapter) {
                handyAdapter->SetPosition(static_cast<int>(position));
            }
            break;
            
        case DeviceType::Lovense:
            if (lovenseAdapter) {
                lovenseAdapter->SendPosition(position);
            }
            break;
            
        case DeviceType::Hismith:
            if (hismithAdapter) {
                hismithAdapter->SendPosition(position);
            }
            break;
            
        case DeviceType::BluetoothLE:
            if (bluetoothAdapter) {
                bluetoothAdapter->SendPosition(position);
            }
            break;
            
        default:
            break;
    }
}

void OFS_DeviceManager::StopDevice() noexcept
{
    auto& state = DeviceState::State(stateHandle);
    
    if (state.selectedDeviceIndex < 0) return;
    
    switch (currentDeviceType) {
        case DeviceType::Handy:
            if (handyAdapter) handyAdapter->Stop();
            break;
        case DeviceType::Lovense:
            if (lovenseAdapter) lovenseAdapter->Stop();
            break;
        case DeviceType::Hismith:
            if (hismithAdapter) hismithAdapter->Stop();
            break;
        case DeviceType::BluetoothLE:
            if (bluetoothAdapter) bluetoothAdapter->Stop();
            break;
        default:
            // XInput handled elsewhere
            break;
    }
    
    state.lastPosition = -1.f;
    LOG_DEBUG("DeviceManager: Stopped device");
}

void OFS_DeviceManager::SetEnabled(bool enabled) noexcept
{
    auto& state = DeviceState::State(stateHandle);
    state.enabled = enabled;
    
    if (!enabled) {
        StopDevice();
    }
}

void OFS_DeviceManager::SetSelectedDevice(int deviceIndex) noexcept
{
    auto& state = DeviceState::State(stateHandle);
    state.selectedDeviceIndex = deviceIndex;
    state.lastPosition = -1.f;
    
    if (deviceIndex >= 0 && deviceIndex < (int)devices.size()) {
        LOGF_INFO("Selected device: %s", devices[deviceIndex].deviceName.c_str());
    }
}

void OFS_DeviceManager::OnWebSocketDeviceConnected(const std::string& deviceName) noexcept
{
    // Mark as connected
    connected.store(true);
    
    // Clear existing devices and add the new one
    devices.clear();
    DeviceInfo dev;
    dev.deviceIndex = 0;
    dev.deviceName = deviceName.empty() ? "Bluetooth Device (via Browser)" : deviceName;
    dev.supportedCommands = {"SetSpeed", "SetPosition", "SendPosition"};
    devices.push_back(dev);
    
    // Select the device
    SetSelectedDevice(0);
    
    LOGF_INFO("WebSocket device connected and selected: %s", dev.deviceName.c_str());
}

void OFS_DeviceManager::SetServerAddress(const std::string& address, int port) noexcept
{
    // Legacy - now handled by device-specific methods
    SetLovenseServerAddress(address, port);
}

void OFS_DeviceManager::EnableDeviceToScriptSync(bool enabled) noexcept
{
    auto& state = DeviceState::State(stateHandle);
    state.syncDeviceToScript = enabled;
    LOGF_INFO("Device to script sync: %s", enabled ? "enabled" : "disabled");
    
    if (!bluetoothAdapter) return;
    
    if (enabled) {
        // Reset lastSyncedPosition to allow recording from the start
        state.lastSyncedPosition = -1.f;
        LOG_INFO("Reset lastSyncedPosition to allow recording");
        
        // Use SEND commands to query device position - send position 0 as a query
        // The device will respond with its current position
        // We'll alternate positions to detect actual device position vs echoed command
        LOG_INFO("Starting position query using SEND commands at 200ms interval");
        bluetoothAdapter->SendWebSocketMessage("SEND:AA,04,00,00");
    } else {
        // Send STOP_QUERY to stop position queries
        LOG_INFO("Sending STOP_QUERY to stop position queries");
        bluetoothAdapter->SendWebSocketMessage("STOP_QUERY");
    }
}

void OFS_DeviceManager::OnDevicePositionChanged(float position) noexcept
{
    auto& state = DeviceState::State(stateHandle);
    
    // Check if sync is enabled
    if (!state.syncDeviceToScript || !connected.load()) return;
    
    if (!player) return;
    float currentTime = player->CurrentTime();
    if (currentTime < 0) return;
    
    // Get active funscript
    auto app = OpenFunscripter::ptr;
    if (!app) return;
    
    auto activeScript = app->ActiveFunscript();
    if (!activeScript) return;
    
    // Map device position (0-100) to funscript position using min/max range
    float deviceRange = static_cast<float>(state.maxPosition - state.minPosition);
    float funscriptPos = state.minPosition + (position / 100.0f) * deviceRange;
    funscriptPos = std::max(0.0f, std::min(100.0f, funscriptPos));
    
    // Ignore the very first position received (initialize debounce baseline)
    if (state.lastSyncedPosition < 0) {
        state.lastSyncedPosition = funscriptPos;
        return;
    }
    
    // Debounce: skip if the position hasn't changed by at least 2%
    if (std::abs(state.lastSyncedPosition - funscriptPos) < 2.0f) return;
    state.lastSyncedPosition = funscriptPos;
    
    // Add a new point at the current playback time with the device position.
    // Use a dedup window (30ms) to reduce point density during recording.
    FunscriptAction newAction(currentTime, static_cast<int32_t>(funscriptPos));
    activeScript->AddEditAction(newAction, 0.030f); // 30ms
    
    LOGF_DEBUG("Device sync: position %.1f -> funscript %d at time %.3fs", 
               position, static_cast<int32_t>(funscriptPos), currentTime);
}

void OFS_DeviceManager::OnDevicePositionChangedEvent(const DevicePositionChangedEvent* ev) noexcept
{
    OnDevicePositionChanged(ev->position);
}

// Event handlers
void OFS_DeviceManager::OnPlayPauseChanged(const PlayPauseChangeEvent* ev) noexcept
{
    if (ev->playerType != VideoplayerType::Main) return;
    
    auto& state = DeviceState::State(stateHandle);
    if (!state.enabled || !connected.load()) return;
    
    isPlaying = !ev->paused;
    
    if (ev->paused && state.pauseWithVideo) {
        // Send position 0 to ensure device stops immediately
        SendPosition(0.0f);
        LOG_DEBUG("DeviceManager: Video paused, stopping device");
    }
    else if (!ev->paused) {
        LOG_DEBUG("DeviceManager: Video playing");
    }
}

void OFS_DeviceManager::OnTimeChanged(const TimeChangeEvent* ev) noexcept
{
    if (ev->playerType != VideoplayerType::Main) return;
    
    currentVideoTime = ev->time;
    
    // Don't send positions when video is paused
    if (!isPlaying) return;
    
    // Only sync when device control is enabled
    auto& state = DeviceState::State(stateHandle);
    if (!state.enabled || !connected.load() || state.selectedDeviceIndex < 0) return;
    
    // Get active funscript and send position
    auto app = OpenFunscripter::ptr;
    if (!app) return;
    
    auto activeScript = app->ActiveFunscript();
    if (!activeScript) return;
    
    // Apply sync delay as lookahead to compensate for device latency
    float lookAheadTime = currentVideoTime + state.syncDelay;
    float position = activeScript->GetPositionAtTime(lookAheadTime);
    
    SendPosition(position);
}

void OFS_DeviceManager::OnSpeedChanged(const PlaybackSpeedChangeEvent* ev) noexcept
{
    if (ev->playerType != VideoplayerType::Main) return;
    
    currentPlaybackSpeed = ev->playbackSpeed;
}

const char* OFS_DeviceManager::GetDeviceName(int index) const noexcept
{
    if (index >= 0 && index < (int)devices.size()) {
        return devices[index].deviceName.c_str();
    }
    return "Unknown Device";
}

const char* OFS_DeviceManager::GetDeviceTypeName() const noexcept
{
    switch (currentDeviceType) {
        case DeviceType::XInput: return "Gamepad (XInput)";
        case DeviceType::Handy: return "The Handy";
        case DeviceType::Lovense: return "Lovense";
        case DeviceType::Hismith: return "Hismith";
        case DeviceType::BluetoothLE: return "Bluetooth LE";
        default: return "None";
    }
}

void OFS_DeviceManager::ShowWindow(bool* open) noexcept
{
    // This is handled by OFS_DevicePanel
}

// XInput specific implementations
bool OFS_DeviceManager::ConnectXInput() noexcept
{
#ifdef WIN32
    ScanXInputDevices();
    
    if (devices.empty()) {
        LOG_WARN("No gamepads found");
        EV::Enqueue<DeviceErrorEvent>("No gamepads found. Connect an Xbox controller or compatible gamepad.");
        return false;
    }
    
    connected.store(true);
    LOGF_INFO("Connected to %d gamepad(s)", (int)devices.size());
    EV::Enqueue<DeviceConnectedEvent>();
    return true;
#else
    LOG_WARN("XInput not supported on this platform");
    return false;
#endif
}

void OFS_DeviceManager::DisconnectXInput() noexcept
{
    // Already handled by Disconnect()
}

void OFS_DeviceManager::ScanXInputDevices() noexcept
{
#ifdef WIN32
    devices.clear();
    
    for (DWORD i = 0; i < XUSER_MAX_COUNT; i++) {
        XINPUT_STATE state;
        ZeroMemory(&state, sizeof(XINPUT_STATE));
        
        DWORD result = XInputGetState(i, &state);
        
        if (result == ERROR_SUCCESS) {
            DeviceInfo dev;
            dev.deviceIndex = (int)i;
            dev.deviceName = "Xbox Controller (Slot " + std::to_string(i) + ")";
            dev.supportedCommands = {"Vibrate"};
            devices.push_back(dev);
            LOGF_INFO("Found gamepad at slot %d", i);
        }
    }
#endif
}

void OFS_DeviceManager::SendXInputVibration(int deviceIndex, float intensity) noexcept
{
#ifdef WIN32
    intensity = std::max(0.0f, std::min(1.0f, intensity));
    
    WORD leftMotor = static_cast<WORD>(intensity * 65535.0f);
    WORD rightMotor = static_cast<WORD>(intensity * 65535.0f);
    
    XINPUT_VIBRATION vibration;
    vibration.wLeftMotorSpeed = leftMotor;
    vibration.wRightMotorSpeed = rightMotor;
    
    XInputSetState(deviceIndex, &vibration);
#endif
}
