#pragma once

#include "../state/DeviceState.h"
#include <string>
#include <memory>
#include <atomic>

// Hismith device adapter - connects via WebSocket over WiFi
// Uses Hismith Remote Play feature - the machine connects to their cloud, 
// we connect via WebSocket to control it
class OFS_HismithAdapter
{
public:
    OFS_HismithAdapter() noexcept;
    ~OFS_HismithAdapter() noexcept;
    
    // Connection - specify game token from Hismith app remote play link
    // and the WebSocket server address
    bool Connect(const std::string& serverAddress = "ws://app.hismiths.com", 
                 int port = 8886,
                 const std::string& gameToken = "") noexcept;
    void Disconnect() noexcept;
    bool IsConnected() const noexcept { return connected_; }
    
    // Device info
    const char* GetDeviceName() const noexcept { return "Hismith"; }
    const char* GetDeviceType() const noexcept { return "hismith"; }
    
    // Send commands
    bool SetSpeed(int speedPercent) noexcept;      // 0-100 main motor
    bool SetVibration(int vibrationPercent) noexcept;  // 0-100 vibration motor
    bool SetPosition(int positionPercent) noexcept;  // 10-100 stroke position
    bool SetStrokeType(bool autoStroke) noexcept;    // true = auto, false = manual
    bool Stop() noexcept;
    
    // Position command (maps funscript position to stroke)
    bool SendPosition(float positionPercent) noexcept;
    
    // Configuration
    void SetServerAddress(const std::string& addr) noexcept { serverAddress_ = addr; }
    void SetPort(int port) noexcept { port_ = port; }
    void SetGameToken(const std::string& token) noexcept { gameToken_ = token; }
    
private:
    std::string serverAddress_;
    int port_ = 8886;
    std::string gameToken_;
    std::atomic<bool> connected_{false};
    
    // WebSocket message handling
    bool SendWsCommand(const std::string& action, const std::string& param) noexcept;
};
