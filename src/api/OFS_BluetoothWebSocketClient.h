#pragma once

#include <string>
#include <atomic>
#include <functional>
#include <mutex>

struct mg_connection;

class OFS_BluetoothWebSocketClient
{
public:
    OFS_BluetoothWebSocketClient() noexcept;
    ~OFS_BluetoothWebSocketClient() noexcept;

    void InitializeConnection(struct mg_connection* conn) noexcept;
    void SendMessage(const std::string& msg) noexcept;
    void Close() noexcept;
    void HandleMessage(const std::string& msg) noexcept;

    bool IsConnected() const noexcept { return connected_; }
    const std::string& GetDeviceName() const noexcept { return deviceName_; }

    void SetConnectionCallback(std::function<void(bool connected, const std::string& name)> callback) noexcept;
    
    // For position updates from device
    void SetPositionCallback(std::function<void(float position)> callback) noexcept;

    // For sending commands to device
    bool SendSpeed(int speedPercent) noexcept;
    bool SendLovenseCommand(const std::string& cmd) noexcept;

private:
    struct mg_connection* connection_ = nullptr;
    std::atomic<bool> connected_{false};
    std::string deviceName_;
    std::function<void(bool, const std::string&)> connectionCallback_;
    std::function<void(float)> positionCallback_;
    std::mutex callbackMutex_;
};

extern OFS_BluetoothWebSocketClient* g_BluetoothClient;
