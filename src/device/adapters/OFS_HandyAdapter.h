#pragma once

#include "../state/DeviceState.h"
#include <string>
#include <memory>
#include <atomic>

// Handy device adapter - connects via REST API over WiFi
// The Handy connects to handyfeeling.com servers, we use their REST API
class OFS_HandyAdapter
{
public:
    OFS_HandyAdapter() noexcept;
    ~OFS_HandyAdapter() noexcept;
    
    // Connection
    bool Connect(const std::string& serverUrl = "https://www.handyfeeling.com/api/handy/v2") noexcept;
    void Disconnect() noexcept;
    bool IsConnected() const noexcept { return connected_; }
    
    // Device info
    const char* GetDeviceName() const noexcept { return "The Handy"; }
    const char* GetDeviceType() const noexcept { return "handy"; }
    
    // Send commands
    bool SetSpeed(int speedPercent) noexcept;  // 0-100
    bool SetPosition(int positionPercent) noexcept;  // 0-100 (stroke position)
    bool Stop() noexcept;
    
    // Query status
    bool IsDeviceOnline() noexcept;
    std::string GetFirmwareVersion() noexcept;
    
    // Server URL configuration
    void SetServerUrl(const std::string& url) noexcept { serverUrl_ = url; }
    std::string GetServerUrl() const noexcept { return serverUrl_; }
    
private:
    std::string serverUrl_;
    std::atomic<bool> connected_{false};
    std::string sessionKey_;
    
    // Internal helpers
    bool SendRequest(const std::string& endpoint, const std::string& body, std::string& response) noexcept;
};
