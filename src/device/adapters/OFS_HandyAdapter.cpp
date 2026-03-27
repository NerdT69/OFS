#include "OFS_HandyAdapter.h"

#include "../../util/OFS_HttpClient.h"

#include <iostream>
#include <sstream>
#include <algorithm>

OFS_HandyAdapter::OFS_HandyAdapter() noexcept
{
}

OFS_HandyAdapter::~OFS_HandyAdapter() noexcept
{
    Disconnect();
}

bool OFS_HandyAdapter::Connect(const std::string& serverUrl) noexcept
{
    serverUrl_ = serverUrl;
    
    // Check if device is online
    if (!IsDeviceOnline()) {
        return false;
    }
    
    connected_ = true;
    return true;
}

void OFS_HandyAdapter::Disconnect() noexcept
{
    connected_ = false;
    sessionKey_.clear();
}

bool OFS_HandyAdapter::SetSpeed(int speedPercent) noexcept
{
    if (!connected_) return false;
    
    // Clamp to valid range
    speedPercent = std::max(0, std::min(100, speedPercent));
    
    std::string body = "{\"speed\":" + std::to_string(speedPercent) + "}";
    std::string response;
    
    return SendRequest("/setSpeed", body, response);
}

bool OFS_HandyAdapter::SetPosition(int positionPercent) noexcept
{
    if (!connected_) return false;
    
    // Clamp to valid range
    positionPercent = std::max(0, std::min(100, positionPercent));
    
    // Handy uses stroke position 0-100
    // API endpoint to set stroke position directly
    std::string body = "{\"position\":" + std::to_string(positionPercent) + "}";
    std::string response;
    
    // Try the direct position API first
    if (SendRequest("/stroke", body, response)) {
        return true;
    }
    
    // Fall back to speed-based movement if direct position not available
    return SetSpeed(positionPercent);
}

bool OFS_HandyAdapter::Stop() noexcept
{
    if (!connected_) return false;
    
    // Set speed to 0
    std::string response;
    return SendRequest("/setSpeed", "{\"speed\":0}", response);
}

bool OFS_HandyAdapter::IsDeviceOnline() noexcept
{
    std::string response;
    
    if (!SendRequest("/connected", "", response)) {
        return false;
    }
    
    // Response should contain online status
    return response.find("true") != std::string::npos || response.find("1") != std::string::npos;
}

std::string OFS_HandyAdapter::GetFirmwareVersion() noexcept
{
    std::string response;
    
    if (!SendRequest("/info", "", response)) {
        return "Unknown";
    }
    
    // Parse firmware info from JSON response
    size_t pos = response.find("fwVersion");
    if (pos != std::string::npos) {
        size_t colonPos = response.find(":", pos);
        if (colonPos != std::string::npos) {
            size_t endPos = response.find(",", colonPos);
            if (endPos == std::string::npos) {
                endPos = response.find("}", colonPos);
            }
            if (endPos != std::string::npos) {
                return response.substr(colonPos + 1, endPos - colonPos - 1);
            }
        }
    }
    
    return "Unknown";
}

bool OFS_HandyAdapter::SendRequest(const std::string& endpoint, const std::string& body, std::string& response) noexcept
{
    OFS_HttpClient client;
    client.SetTimeout(5000);
    
    // Build URL string
    std::string url = serverUrl_ + endpoint;
    
    bool success;
    if (body.empty()) {
        success = client.Get(url, response);
    } else {
        success = client.Post(url, body, response);
    }
    
    if (!success) {
        std::cerr << "Handy API error: " << client.GetLastError() << std::endl;
        return false;
    }
    
    // Check for error response
    if (response.find("\"code\":0") != std::string::npos || 
        response.find("\"success\":true") != std::string::npos) {
        return true;
    }
    
    // Device not connected error
    if (response.find("Machine not connected") != std::string::npos) {
        connected_ = false;
        return false;
    }
    
    return true;
}
