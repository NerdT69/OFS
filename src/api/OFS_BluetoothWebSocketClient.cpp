#include "OFS_BluetoothWebSocketClient.h"

#include "OFS_FileLogging.h"

#include <civetweb.h>
#include <sstream>
#include <algorithm>

OFS_BluetoothWebSocketClient* g_BluetoothClient = nullptr;

OFS_BluetoothWebSocketClient::OFS_BluetoothWebSocketClient() noexcept
{
}

OFS_BluetoothWebSocketClient::~OFS_BluetoothWebSocketClient() noexcept
{
    Close();
}

void OFS_BluetoothWebSocketClient::InitializeConnection(struct mg_connection* conn) noexcept
{
    connection_ = conn;
    connected_ = true;
    LOG_INFO("Bluetooth WebSocket client initialized");
}

void OFS_BluetoothWebSocketClient::SendMessage(const std::string& msg) noexcept
{
    if (!connection_) return;
    mg_websocket_write(connection_, MG_WEBSOCKET_OPCODE_TEXT, msg.data(), msg.size());
}

void OFS_BluetoothWebSocketClient::Close() noexcept
{
    if (connected_) {
        connected_ = false;
        deviceName_.clear();
        LOG_INFO("Bluetooth WebSocket client closed");
        
        std::lock_guard<std::mutex> lock(callbackMutex_);
        if (connectionCallback_) {
            connectionCallback_(false, "");
        }
    }
    connection_ = nullptr;
}

void OFS_BluetoothWebSocketClient::SetConnectionCallback(std::function<void(bool, const std::string&)> callback) noexcept
{
    std::lock_guard<std::mutex> lock(callbackMutex_);
    connectionCallback_ = callback;
}

void OFS_BluetoothWebSocketClient::SetPositionCallback(std::function<void(float)> callback) noexcept
{
    std::lock_guard<std::mutex> lock(callbackMutex_);
    positionCallback_ = callback;
}

void OFS_BluetoothWebSocketClient::HandleMessage(const std::string& msg) noexcept
{
    LOGF_INFO("Bluetooth WebSocket received: %s", msg.c_str());
    
    // Handle PING - must respond with PONG
    if (msg == "PING") {
        SendMessage("PONG");
        return;
    }
    
    if (msg.rfind("DEVICE:", 0) == 0) {
        deviceName_ = msg.substr(7);
        LOGF_INFO("Bluetooth device name: %s", deviceName_.c_str());
    }
    else if (msg == "GATT_CONNECTED") {
        LOG_INFO("Bluetooth GATT connected");
    }
    else if (msg.rfind("READY:", 0) == 0) {
        LOG_INFO("Bluetooth ready - calling callback");
        connected_ = true;
        std::lock_guard<std::mutex> lock(callbackMutex_);
        if (connectionCallback_) {
            LOG_INFO("Bluetooth calling connection callback with connected=true");
            connectionCallback_(true, deviceName_);
        } else {
            LOG_INFO("Bluetooth NO callback set!");
        }
    }
    else if (msg == "CONNECTED") {
        LOG_INFO("Bluetooth CONNECTED - calling callback");
        connected_ = true;
        std::lock_guard<std::mutex> lock(callbackMutex_);
        if (connectionCallback_) {
            LOG_INFO("Bluetooth calling connection callback with connected=true");
            connectionCallback_(true, deviceName_);
        } else {
            LOG_INFO("Bluetooth NO callback set!");
        }
    }
    else if (msg.rfind("ERROR:", 0) == 0) {
        LOGF_INFO("Bluetooth error: %s", msg.substr(6).c_str());
    }
    else if (msg.rfind("POSITION:", 0) == 0) {
        // Handle position updates from device
        // Position can be in 0-100 range (already percentage) or 0-255 range (raw byte)
        // If value > 100, assume it's a raw byte and convert to percentage
        std::string posStr = msg.substr(9);
        try {
            float position = std::stof(posStr);
            if (position > 100.0f) {
                // Convert from 0-255 range to 0-100 range
                position = (position / 255.0f) * 100.0f;
            }
            position = std::max(0.0f, std::min(100.0f, position));
            LOGF_INFO("Device position changed: %.1f (from raw: %s)", position, posStr.c_str());
            std::lock_guard<std::mutex> lock(callbackMutex_);
            if (positionCallback_) {
                positionCallback_(position);
            }
        } catch (...) {
            LOGF_WARN("Invalid position value: %s", posStr.c_str());
        }
    }
    // Also support "Position" (capital P) format
    else if (msg.rfind("Position:", 0) == 0) {
        std::string posStr = msg.substr(9);
        try {
            float position = std::stof(posStr);
            if (position > 100.0f) {
                // Convert from 0-255 range to 0-100 range
                position = (position / 255.0f) * 100.0f;
            }
            position = std::max(0.0f, std::min(100.0f, position));
            LOGF_INFO("Device position changed (Position): %.1f (from raw: %s)", position, posStr.c_str());
            std::lock_guard<std::mutex> lock(callbackMutex_);
            if (positionCallback_) {
                positionCallback_(position);
            }
        } catch (...) {
            LOGF_WARN("Invalid position value: %s", posStr.c_str());
        }
    }
}

bool OFS_BluetoothWebSocketClient::SendSpeed(int speedPercent) noexcept
{
    if (!connected_) return false;
    
    speedPercent = std::max(0, std::min(100, speedPercent));
    
    // Detect Lovense by name
    bool isLovense = deviceName_.find("LVS-") != std::string::npos ||
                     deviceName_.find("Lovense") != std::string::npos ||
                     deviceName_.find("Max") != std::string::npos ||
                     deviceName_.find("Nora") != std::string::npos ||
                     deviceName_.find("Lush") != std::string::npos ||
                     deviceName_.find("Edge") != std::string::npos;
    
    if (isLovense) {
        int vibrateLevel = (speedPercent * 20) / 100;
        return SendLovenseCommand("Vibrate:" + std::to_string(vibrateLevel));
    }
    
    // Hismith protocol: 0xAA, cmd, param, checksum
    uint8_t checksum = (0x04 + speedPercent) & 0xFF;
    uint8_t data[4] = {0xAA, 0x04, (uint8_t)speedPercent, checksum};
    
    char hexData[32];
    snprintf(hexData, sizeof(hexData), "AA,%02X,%02X,%02X", data[1], data[2], data[3]);
    
    SendMessage("SEND:" + std::string(hexData));
    return true;
}

bool OFS_BluetoothWebSocketClient::SendLovenseCommand(const std::string& cmd) noexcept
{
    if (!connected_) return false;
    SendMessage("LOVENSE:" + cmd);
    return true;
}
