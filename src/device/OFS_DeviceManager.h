#pragma once

#include "../state/DeviceState.h"
#include "OFS_Event.h"

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <atomic>
#include <thread>

// Forward declarations
class OFS_Videoplayer;
class Funscript;

// Device adapters
#ifdef WIN32
#include <xinput.h>
#endif

// Device connection events
class DeviceConnectedEvent : public OFS_Event<DeviceConnectedEvent>
{
public:
    DeviceConnectedEvent() noexcept {}
};

class DeviceDisconnectedEvent : public OFS_Event<DeviceDisconnectedEvent>
{
public:
    DeviceDisconnectedEvent() noexcept {}
};

class DeviceListUpdatedEvent : public OFS_Event<DeviceListUpdatedEvent>
{
public:
    DeviceListUpdatedEvent() noexcept {}
};

class DeviceErrorEvent : public OFS_Event<DeviceErrorEvent>
{
public:
    std::string errorMessage;
    DeviceErrorEvent() noexcept : errorMessage() {}
    DeviceErrorEvent(const std::string& msg) noexcept : errorMessage(msg) {}
};

class DevicePositionChangedEvent : public OFS_Event<DevicePositionChangedEvent>
{
public:
    float position; // 0-100 position from device
    DevicePositionChangedEvent() noexcept : position(0) {}
    DevicePositionChangedEvent(float pos) noexcept : position(pos) {}
};

// Forward declare adapters
class OFS_HandyAdapter;
class OFS_LovenseAdapter;
class OFS_HismithAdapter;
class OFS_BluetoothLEAdapter;

// Device types enum
enum class DeviceType
{
    None,
    XInput,       // Xbox/gamepad controllers
    Handy,        // The Handy
    Lovense,      // Lovense toys (via LAN)
    Hismith,      // Hismith machines (via WiFi)
    BluetoothLE   // Any Bluetooth LE device (Hismith, Lovense, etc.)
};

// OFS_DeviceManager - Multi-device control manager
class OFS_DeviceManager
{
private:
    uint32_t stateHandle = 0xFFFF'FFFF;
    
    // Connection state
    std::atomic<bool> connected{false};
    std::atomic<bool> initialized{false};
    
    // Current device type
    DeviceType currentDeviceType = DeviceType::None;
    
    // Device adapters
    std::unique_ptr<OFS_HandyAdapter> handyAdapter;
    std::unique_ptr<OFS_LovenseAdapter> lovenseAdapter;
    std::unique_ptr<OFS_HismithAdapter> hismithAdapter;
    std::unique_ptr<OFS_BluetoothLEAdapter> bluetoothAdapter;
    
    // Event listeners
    UnsubscribeFn playPauseListener;
    UnsubscribeFn timeChangeListener;
    UnsubscribeFn speedChangeListener;
    UnsubscribeFn devicePosListener;
    
    // Playback state
    bool isPlaying = false;
    float currentVideoTime = 0.f;
    float currentPlaybackSpeed = 1.f;
    
    // Video player reference
    OFS_Videoplayer* player = nullptr;
    
    // Device list (for UI)
    std::vector<DeviceInfo> devices;
    
    // Event handlers
    void OnPlayPauseChanged(const class PlayPauseChangeEvent* ev) noexcept;
    void OnTimeChanged(const class TimeChangeEvent* ev) noexcept;
    void OnSpeedChanged(const class PlaybackSpeedChangeEvent* ev) noexcept;
    void OnDevicePositionChangedEvent(const class DevicePositionChangedEvent* ev) noexcept;
    
public:
    OFS_DeviceManager() noexcept;
    ~OFS_DeviceManager() noexcept;
    
    // Initialize with video player reference
    void Init(OFS_Videoplayer* player) noexcept;
    
    // Shutdown
    void Shutdown() noexcept;
    
    // Update - called every frame
    void Update() noexcept;
    
    // Connection management
    bool EnsureConnected() noexcept;
    void Disconnect() noexcept;
    bool IsConnected() const noexcept { return connected.load(); }
    void SetConnected(bool connected) noexcept { this->connected.store(connected); }
    bool IsInitialized() const noexcept { return initialized.load(); }
    
    // Device selection
    void SetDeviceType(DeviceType type) noexcept;
    DeviceType GetDeviceType() const noexcept { return currentDeviceType; }
    
    // Device-specific configuration
    void SetHandyServerUrl(const std::string& url) noexcept;
    void SetLovenseServerAddress(const std::string& addr, int port) noexcept;
    void SetHismithServerAddress(const std::string& addr, int port, const std::string& token) noexcept;
    
    // Scanning - discover available devices
    void StartScanning() noexcept;
    void StopScanning() noexcept;
    
    // Device control
    void SendPosition(float position) noexcept;
    void StopDevice() noexcept;
    
    // Settings
    void SetEnabled(bool enabled) noexcept;
    void SetSelectedDevice(int deviceIndex) noexcept;
    void SetServerAddress(const std::string& address, int port) noexcept;
    
    // Device to script sync (reverse direction)
    void EnableDeviceToScriptSync(bool enabled) noexcept;
    void OnDevicePositionChanged(float position) noexcept;
    
    // WebSocket Bluetooth connection - called when browser connects via WebSocket
    void OnWebSocketDeviceConnected(const std::string& deviceName) noexcept;
    
    // UI
    void ShowWindow(bool* open) noexcept;
    
    // Get available devices
    int GetDeviceCount() const noexcept { return (int)devices.size(); }
    const char* GetDeviceName(int index) const noexcept;
    const char* GetDeviceTypeName() const noexcept;
    
private:
    // Internal connection
    bool ConnectDevice() noexcept;
    void DisconnectDevice() noexcept;
    
    // XInput specific
    bool ConnectXInput() noexcept;
    void DisconnectXInput() noexcept;
    void ScanXInputDevices() noexcept;
    void SendXInputVibration(int deviceIndex, float intensity) noexcept;
};
