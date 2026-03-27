#pragma once

#include <string>
#include <memory>
#include <atomic>
#include <vector>
#include <cstdint>
#include <functional>

#ifdef WIN32
#include <windows.h>
#include <bthdef.h>
#endif

enum class BLEDeviceType
{
    Unknown,
    Hismith,
    LovenseGen1,
    LovenseGen2
};

class OFS_BluetoothLEAdapter
{
public:
    OFS_BluetoothLEAdapter() noexcept;
    ~OFS_BluetoothLEAdapter() noexcept;

    bool Connect(const std::string& deviceName = "") noexcept;
    void Disconnect() noexcept;
    bool IsConnected() const noexcept;

    const char* GetDeviceName() const noexcept;
    const char* GetDeviceType() const noexcept { return "BluetoothLE"; }
    BLEDeviceType GetBLEDeviceType() const noexcept { return deviceType_; }

    bool SetSpeed(int speedPercent) noexcept;
    bool SetPosition(int positionPercent) noexcept;
    bool Stop() noexcept;

    bool SendPosition(float positionPercent) noexcept;

    // Set callback for when WebSocket Bluetooth connects - allows DeviceManager to be notified
    void SetWebSocketConnectCallback(std::function<void(const std::string&)> callback) noexcept;
    
    // Set callback for device position changes (for reverse sync)
    void SetPositionCallback(std::function<void(float)> callback) noexcept;
    
    // Send raw WebSocket message to browser
    bool SendWebSocketMessage(const std::string& message) noexcept;

    static std::vector<std::string> ScanForDevices() noexcept;
    static std::string GetDeviceNameFromAddress(const std::string& address) noexcept;
    static BLEDeviceType DetectDeviceType(const std::string& deviceName) noexcept;

private:
    bool SendCommand(uint8_t cmd, uint8_t param) noexcept;
    bool SendLovenseCommand(const std::string& cmd) noexcept;
    bool WriteToCharacteristic(const std::vector<uint8_t>& data) noexcept;

    std::string deviceName_;
    std::atomic<bool> connected_{false};
    BLEDeviceType deviceType_ = BLEDeviceType::Unknown;
    std::function<void(const std::string&)> wsConnectCallback_;
    std::function<void(float)> positionCallback_;

#ifdef WIN32
    BTH_ADDR deviceAddress_;
#endif
};

// Forward declare the Bluetooth WebSocket client
class OFS_BluetoothWebSocketClient;
extern OFS_BluetoothWebSocketClient* g_BluetoothClient;
