#pragma once

#include "../state/DeviceState.h"
#include <string>
#include <memory>
#include <atomic>
#include <vector>

// Lovense device adapter - connects via LAN API over WiFi
// Uses Lovense Connect app as a bridge (phone/tablet runs Lovense Connect, we connect via LAN)
class OFS_LovenseAdapter
{
public:
    OFS_LovenseAdapter() noexcept;
    ~OFS_LovenseAdapter() noexcept;
    
    // Connection - specify the IP:port of the Lovense Connect server
    bool Connect(const std::string& serverAddress, int port = 34567) noexcept;
    void Disconnect() noexcept;
    bool IsConnected() const noexcept { return connected_; }
    
    // Device discovery - scan for Lovense devices on the network
    static std::vector<std::string> DiscoverDevices(const std::string& serverAddress, int port = 34567) noexcept;
    
    // Device info
    const char* GetDeviceName() const noexcept { return deviceName_.c_str(); }
    const char* GetDeviceType() const noexcept { return deviceType_.c_str(); }
    const char* GetDeviceId() const noexcept { return deviceId_.c_str(); }
    
    // Send commands
    bool Vibrate(int level) noexcept;  // 0-20 (level)
    bool VibratePulse(int hz, int durationMs) noexcept;  // Custom pulse pattern
    bool Rotate(int speed) noexcept;  // 0-20 (for rotating toys)
    bool AirLevel(int level) noexcept;  // 0-5 (for Air toys like Max)
    bool Stop() noexcept;
    
    // Position command (maps funscript position to appropriate toy action)
    bool SendPosition(float positionPercent) noexcept;
    
    // Server configuration
    void SetServerAddress(const std::string& addr) noexcept { serverAddress_ = addr; }
    void SetServerPort(int port) noexcept { serverPort_ = port; }
    
private:
    std::string serverAddress_;
    int serverPort_ = 34567;
    std::atomic<bool> connected_{false};
    std::string deviceId_;
    std::string deviceName_;
    std::string deviceType_;
    
    // Internal helpers
    bool SendCommand(const std::string& command) noexcept;
};

// Device info structure
struct LovenseDeviceInfo
{
    std::string id;
    std::string name;
    std::string type;
};
