#include "OFS_HismithAdapter.h"

#include "../../util/OFS_HttpClient.h"
#include "OFS_FileLogging.h"

#include <iostream>
#include <sstream>
#include <algorithm>

OFS_HismithAdapter::OFS_HismithAdapter() noexcept
{
}

OFS_HismithAdapter::~OFS_HismithAdapter() noexcept
{
    Disconnect();
}

bool OFS_HismithAdapter::Connect(const std::string& serverAddress, int port, const std::string& gameToken) noexcept
{
    serverAddress_ = serverAddress;
    port_ = port;
    gameToken_ = gameToken;
    
    // For now, the Hismith WebSocket API requires authentication
    // We need the game token from the Hismith app
    // Without a valid token, we cannot connect
    
    if (gameToken_.empty()) {
        // Cannot connect without game token - user needs to provide one from Hismith app
        LOG_WARN("Hismith: No game token provided. Get token from Hismith app.");
        connected_ = false;
        return false;
    }
    
    // Note: Full WebSocket implementation would need to:
    // 1. Connect to ws://app.hismiths.com:8886
    // 2. Send connection request with game token
    // 3. Receive device status updates
    // 4. Send commands
    
    // For now, require a non-empty token to consider connected
    // The actual commands will be simulated
    if (!gameToken_.empty()) {
        connected_ = true;
        LOG_INFO("Hismith: Connected (demo mode)");
        return true;
    }
    
    connected_ = false;
    return false;
}

void OFS_HismithAdapter::Disconnect() noexcept
{
    Stop();
    connected_ = false;
}

bool OFS_HismithAdapter::SetSpeed(int speedPercent) noexcept
{
    if (!connected_) return false;
    
    speedPercent = std::max(0, std::min(100, speedPercent));
    
    return SendWsCommand("setSpeed", "S:" + std::to_string(speedPercent));
}

bool OFS_HismithAdapter::SetVibration(int vibrationPercent) noexcept
{
    if (!connected_) return false;
    
    vibrationPercent = std::max(0, std::min(100, vibrationPercent));
    
    return SendWsCommand("setSpeed", "V:" + std::to_string(vibrationPercent));
}

bool OFS_HismithAdapter::SetPosition(int positionPercent) noexcept
{
    if (!connected_) return false;
    
    positionPercent = std::max(10, std::min(100, positionPercent));
    
    // Set to manual mode first
    SendWsCommand("setMode", "0");
    
    // Then set position
    return SendWsCommand("setSpeed", "S:" + std::to_string(positionPercent));
}

bool OFS_HismithAdapter::SetStrokeType(bool autoStroke) noexcept
{
    if (!connected_) return false;
    
    // Mode: 1 = auto, 0 = manual
    std::string mode = autoStroke ? "1" : "0";
    return SendWsCommand("setMode", mode);
}

bool OFS_HismithAdapter::Stop() noexcept
{
    if (!connected_) return false;
    
    // Stop both motors
    SendWsCommand("setSpeed", "S:0");
    SendWsCommand("setSpeed", "V:0");
    
    return true;
}

bool OFS_HismithAdapter::SendPosition(float positionPercent) noexcept
{
    if (!connected_) return false;
    
    // Hismith uses 10-100 for position
    int position = static_cast<int>(positionPercent / 100.0f * 90.0f) + 10;
    position = std::max(10, std::min(100, position));
    
    // Use manual stroke mode with position
    return SetPosition(position);
}

bool OFS_HismithAdapter::SendWsCommand(const std::string& action, const std::string& param) noexcept
{
    // Build JSON message per Hismith protocol
    // {"action":"setSpeed","type":"S:50","success":true}
    
    std::stringstream json;
    json << "{\"action\":\"" << action << "\",\"type\":\"" << param << "\"}";
    
    // Note: Full implementation would send via WebSocket
    // For now, log the command that would be sent
    std::cerr << "Hismith command: " << json.str() << std::endl;
    
    // Return true to simulate successful command
    // In production, this would send via OFS_WebSocketClient
    return true;
}
