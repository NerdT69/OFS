#pragma once

#include "../state/DeviceState.h"
#include <string>
#include <memory>
#include <atomic>
#include <vector>
#include <cstdint>

#ifdef WIN32
#include <windows.h>
#include <bthdef.h>
#include <setupapi.h>
#include <bluetoothapis.h>
#include <bluetoothleapis.h>
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "bthprops.lib")

DEFINE_GUID(GUID_HISMITH_BLE_SERVICE, 0x0000ffe5, 0x0000, 0x1000, 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb);
DEFINE_GUID(GUID_HISMITH_BLE_CHAR, 0x0000ffe9, 0x0000, 0x1000, 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb);
#endif

class OFS_HismithBluetoothAdapter
{
public:
    OFS_HismithBluetoothAdapter() noexcept;
    ~OFS_HismithBluetoothAdapter() noexcept;
    
    bool Connect(const std::string& deviceName = "") noexcept;
    void Disconnect() noexcept;
    bool IsConnected() const noexcept { return connected_; }
    
    const char* GetDeviceName() const noexcept { return deviceName_.c_str(); }
    const char* GetDeviceType() const noexcept { return "hismith_bt"; }
    
    bool SetSpeed(int speedPercent) noexcept;
    bool SetVibration(int vibrationPercent) noexcept;
    bool SetPosition(int positionPercent) noexcept;
    bool SetStrokeType(bool autoStroke) noexcept;
    bool Stop() noexcept;
    
    bool SendPosition(float positionPercent) noexcept;
    
    static std::vector<std::string> ScanForDevices() noexcept;
    
private:
    bool SendHismithCommand(uint8_t cmd, uint8_t param) noexcept;
    bool FindAndConnectDevice(const std::string& deviceName) noexcept;
    
    std::string deviceName_;
    std::atomic<bool> connected_{false};
    
#ifdef WIN32
    BTH_ADDR deviceAddress_;
    HANDLE bleDeviceHandle_;
#endif
};
