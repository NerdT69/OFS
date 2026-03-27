#include "OFS_LovenseAdapter.h"

#include "../../util/OFS_HttpClient.h"
#include "OFS_FileLogging.h"

#include <iostream>
#include <sstream>
#include <algorithm>

OFS_LovenseAdapter::OFS_LovenseAdapter() noexcept
{
}

OFS_LovenseAdapter::~OFS_LovenseAdapter() noexcept
{
    Disconnect();
}

bool OFS_LovenseAdapter::Connect(const std::string& serverAddress, int port) noexcept
{
    serverAddress_ = serverAddress;
    serverPort_ = port;
    
    // Try to get device list
    auto devices = DiscoverDevices(serverAddress, port);
    
    if (devices.empty()) {
        // No devices found - not connected
        connected_ = false;
        deviceName_ = "Lovense (No devices found)";
        deviceId_ = "";
        LOG_WARN("Lovense: No devices found on network");
        return false;
    }
    
    // Use the first device found
    deviceId_ = devices[0];
    
    // Get device info
    std::string cmd = "/GetToyInfo?tid=" + deviceId_;
    std::string response;
    OFS_HttpClient client;
    client.SetTimeout(3000);
    
    // Build URL
    std::stringstream url;
    url << "http://" << serverAddress_ << ":" << serverPort_ << cmd;
    
    if (client.Get(url.str(), response)) {
        // Parse device name from response
        size_t namePos = response.find("\"nickName\"");
        if (namePos != std::string::npos) {
            size_t colonPos = response.find(":", namePos);
            if (colonPos != std::string::npos) {
                size_t startQuote = response.find("\"", colonPos + 1);
                size_t endQuote = response.find("\"", startQuote + 1);
                if (startQuote != std::string::npos && endQuote != std::string::npos) {
                    deviceName_ = response.substr(startQuote + 1, endQuote - startQuote - 1);
                }
            }
        }
        
        // Get device type
        size_t typePos = response.find("\"name\"");
        if (typePos != std::string::npos) {
            size_t colonPos = response.find(":", typePos);
            if (colonPos != std::string::npos) {
                size_t startQuote = response.find("\"", colonPos + 1);
                size_t endQuote = response.find("\"", startQuote + 1);
                if (startQuote != std::string::npos && endQuote != std::string::npos) {
                    deviceType_ = response.substr(startQuote + 1, endQuote - startQuote - 1);
                }
            }
        }
    }
    
    if (deviceName_.empty()) {
        deviceName_ = "Lovense " + deviceId_;
    }
    
    connected_ = true;
    return true;
}

void OFS_LovenseAdapter::Disconnect() noexcept
{
    // Send stop command before disconnecting
    Stop();
    connected_ = false;
    deviceId_.clear();
    deviceName_.clear();
    deviceType_.clear();
}

std::vector<std::string> OFS_LovenseAdapter::DiscoverDevices(const std::string& serverAddress, int port) noexcept
{
    std::vector<std::string> devices;
    
    OFS_HttpClient client;
    client.SetTimeout(3000);
    
    std::stringstream url;
    url << "http://" << serverAddress << ":" << port << "/GetToys";
    
    std::string response;
    if (!client.Get(url.str(), response)) {
        return devices;
    }
    
    // Parse JSON response to get toy IDs
    size_t pos = 0;
    while ((pos = response.find("\"", pos)) != std::string::npos) {
        size_t endPos = response.find("\":", pos);
        if (endPos != std::string::npos) {
            std::string id = response.substr(pos + 1, endPos - pos - 1);
            // Skip known keys
            if (id != "code" && id != "message" && id != "result" && id != "toys") {
                devices.push_back(id);
            }
            pos = endPos;
        } else {
            break;
        }
    }
    
    return devices;
}

bool OFS_LovenseAdapter::Vibrate(int level) noexcept
{
    if (!connected_) return false;
    
    // Clamp to 0-20
    level = std::max(0, std::min(20, level));
    
    std::stringstream cmd;
    cmd << "/Vibrate?tid=" << deviceId_ << "&v=" << level;
    std::string response;
    
    return SendCommand(cmd.str());
}

bool OFS_LovenseAdapter::VibratePulse(int hz, int durationMs) noexcept
{
    if (!connected_) return false;
    
    // Convert Hz to level approximation
    int level = std::max(0, std::min(20, hz / 2));
    return Vibrate(level);
}

bool OFS_LovenseAdapter::Rotate(int speed) noexcept
{
    if (!connected_) return false;
    
    // Clamp to 0-20
    speed = std::max(0, std::min(20, speed));
    
    std::stringstream cmd;
    cmd << "/Rotate?tid=" << deviceId_ << "&s=" << speed;
    std::string response;
    
    return SendCommand(cmd.str());
}

bool OFS_LovenseAdapter::AirLevel(int level) noexcept
{
    if (!connected_) return false;
    
    // Clamp to 0-5
    level = std::max(0, std::min(5, level));
    
    std::stringstream cmd;
    cmd << "/AirLevel?tid=" << deviceId_ << "&l=" << level;
    std::string response;
    
    return SendCommand(cmd.str());
}

bool OFS_LovenseAdapter::Stop() noexcept
{
    if (!connected_) return false;
    
    // Stop all
    std::string cmd = "/Stop?tid=" + deviceId_;
    std::string response;
    
    return SendCommand(cmd);
}

bool OFS_LovenseAdapter::SendPosition(float positionPercent) noexcept
{
    if (!connected_ || deviceId_.empty()) return false;
    
    // Map position 0-100 to appropriate toy command
    // Most Lovense toys respond to vibrate level
    
    // For rotating toys (Nora), use rotation
    if (deviceType_ == "nora" || deviceType_ == "max") {
        int level = static_cast<int>(positionPercent / 100.0f * 20.0f);
        return Rotate(level);
    }
    
    // For air toys (Max), use air level
    if (deviceType_ == "max") {
        int level = static_cast<int>(positionPercent / 100.0f * 5.0f);
        return AirLevel(level);
    }
    
    // Default: use vibration
    int level = static_cast<int>(positionPercent / 100.0f * 20.0f);
    return Vibrate(level);
}

bool OFS_LovenseAdapter::SendCommand(const std::string& command) noexcept
{
    OFS_HttpClient client;
    client.SetTimeout(2000);
    
    std::stringstream url;
    url << "http://" << serverAddress_ << ":" << serverPort_ << command;
    
    std::string response;
    return client.Get(url.str(), response);
}
