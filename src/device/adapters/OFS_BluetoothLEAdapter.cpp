#include "OFS_BluetoothLEAdapter.h"

#include "OFS_FileLogging.h"
#include "api/OFS_BluetoothWebSocketClient.h"

#include <iostream>
#include <sstream>
#include <algorithm>
#include <cstring>

#ifdef WIN32
#include <windows.h>
#include <setupapi.h>
#include <bluetoothapis.h>
#include <bthdef.h>
#include <initguid.h>
#include <bluetoothleapis.h>
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "bthprops.lib")

DEFINE_GUID(GUID_BTHLE_DEVICE, 0x0000ffe5, 0x0000, 0x1000, 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb);
DEFINE_GUID(GUID_BTHLE_LOVENSE, 0x0000ffb0, 0x0000, 0x1000, 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb);
DEFINE_GUID(GUID_BTHLE_CHAR, 0x0000ffe9, 0x0000, 0x1000, 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x9b, 0x34, 0xfb);

DEFINE_GUID(GUID_HISMITH_SERVICE, 0x0000ffe5, 0x0000, 0x1000, 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb);
DEFINE_GUID(GUID_HISMITH_CHARACTERISTIC, 0x0000ffe9, 0x0000, 0x1000, 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb);

DEFINE_GUID(GUID_LOVENSE_GEN1_SERVICE, 0x0000fff0, 0x0000, 0x1000, 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb);
DEFINE_GUID(GUID_LOVENSE_GEN1_TX, 0x0000fff2, 0x0000, 0x1000, 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb);

DEFINE_GUID(GUID_LOVENSE_NORDIC_SERVICE, 0x6e400001, 0xb5a3, 0xf393, 0xe0, 0xa9, 0xe5, 0x0e, 0x24, 0xdc, 0xca, 0x9e);
DEFINE_GUID(GUID_LOVENSE_NORDIC_TX, 0x6e400002, 0xb5a3, 0xf393, 0xe0, 0xa9, 0xe5, 0x0e, 0x24, 0xdc, 0xca, 0x9e);
#endif

OFS_BluetoothLEAdapter::OFS_BluetoothLEAdapter() noexcept
{
    // Set up callback for WebSocket Bluetooth connection status
    if (g_BluetoothClient) {
        g_BluetoothClient->SetConnectionCallback([this](bool connected, const std::string& name) {
            if (connected) {
                this->connected_ = true;
                if (!name.empty()) {
                    this->deviceName_ = name;
                }
                LOGF_INFO("Bluetooth LE: WebSocket connected callback, device=%s", this->deviceName_.c_str());
                
                // Call external callback if set
                if (wsConnectCallback_) {
                    wsConnectCallback_(this->deviceName_);
                }
            } else {
                this->connected_ = false;
                LOG_INFO("Bluetooth LE: WebSocket disconnected callback");
            }
        });
    }
}

void OFS_BluetoothLEAdapter::SetWebSocketConnectCallback(std::function<void(const std::string&)> callback) noexcept
{
    wsConnectCallback_ = callback;
}

void OFS_BluetoothLEAdapter::SetPositionCallback(std::function<void(float)> callback) noexcept
{
    positionCallback_ = callback;
    
    // Also register with the WebSocket client to receive position updates
    if (g_BluetoothClient) {
        g_BluetoothClient->SetPositionCallback([this](float position) {
            if (positionCallback_) {
                positionCallback_(position);
            }
        });
    }
}

OFS_BluetoothLEAdapter::~OFS_BluetoothLEAdapter() noexcept
{
}

bool OFS_BluetoothLEAdapter::Connect(const std::string& deviceName) noexcept
{
    LOG_INFO("Bluetooth LE: Using WebSocket connection to browser...");
    
    // The actual connection happens via the browser
    // Just mark as "connecting" - the user needs to use the browser connector
    deviceName_ = deviceName;
    deviceType_ = DetectDeviceType(deviceName);
    
    // Check if WebSocket client is connected
    if (g_BluetoothClient && g_BluetoothClient->IsConnected()) {
        connected_ = true;
        deviceName_ = g_BluetoothClient->GetDeviceName();
        LOGF_INFO("Bluetooth LE: Connected via WebSocket to %s", deviceName_.c_str());
        return true;
    }
    
    // Return true to indicate we're ready - connection happens via browser
    connected_ = true;
    deviceName_ = "Bluetooth Device (via Browser)";
    LOG_INFO("Bluetooth LE: Waiting for browser connection...");
    return true;
}

void OFS_BluetoothLEAdapter::Disconnect() noexcept
{
    connected_ = false;
    deviceName_.clear();
    deviceType_ = BLEDeviceType::Unknown;
#ifdef WIN32
    deviceAddress_ = 0;
#endif
    LOG_INFO("Bluetooth LE disconnected");
}

bool OFS_BluetoothLEAdapter::IsConnected() const noexcept
{
    if (g_BluetoothClient && g_BluetoothClient->IsConnected()) {
        return true;
    }
    return connected_;
}

const char* OFS_BluetoothLEAdapter::GetDeviceName() const noexcept
{
    if (g_BluetoothClient && g_BluetoothClient->IsConnected()) {
        return g_BluetoothClient->GetDeviceName().c_str();
    }
    return deviceName_.c_str();
}
        

bool OFS_BluetoothLEAdapter::SetSpeed(int speedPercent) noexcept
{
    if (!connected_) return false;
    speedPercent = std::max(0, std::min(100, speedPercent));

    // Check if using Web Bluetooth (via browser)
    if (g_BluetoothClient && g_BluetoothClient->IsConnected()) {
        return g_BluetoothClient->SendSpeed(speedPercent);
    }

    if (deviceType_ == BLEDeviceType::LovenseGen1 || deviceType_ == BLEDeviceType::LovenseGen2) {
        int vibrateLevel = (speedPercent * 20) / 100;
        return SendLovenseCommand("Vibrate:" + std::to_string(vibrateLevel));
    }
    
    return SendCommand(0x04, static_cast<uint8_t>(speedPercent));
}

bool OFS_BluetoothLEAdapter::SetPosition(int positionPercent) noexcept
{
    if (!connected_) return false;
    positionPercent = std::max(0, std::min(100, positionPercent));

    // Check if using Web Bluetooth (via browser)
    if (g_BluetoothClient && g_BluetoothClient->IsConnected()) {
        return g_BluetoothClient->SendSpeed(positionPercent);
    }

    if (deviceType_ == BLEDeviceType::LovenseGen1 || deviceType_ == BLEDeviceType::LovenseGen2) {
        return SendLovenseCommand("Vibrate:0");
    }
    
    return SendCommand(0x04, static_cast<uint8_t>(positionPercent));
}

bool OFS_BluetoothLEAdapter::Stop() noexcept
{
    if (!connected_) return false;

    if (deviceType_ == BLEDeviceType::LovenseGen1 || deviceType_ == BLEDeviceType::LovenseGen2) {
        return SendLovenseCommand("Vibrate:0");
    }

    return SendCommand(0x04, 0);
}

bool OFS_BluetoothLEAdapter::SendPosition(float positionPercent) noexcept
{
    if (!connected_) return false;

    int position = static_cast<int>(positionPercent / 100.0f * 100.0f);
    position = std::max(0, std::min(100, position));

    return SetSpeed(position);
}

bool OFS_BluetoothLEAdapter::SendWebSocketMessage(const std::string& message) noexcept
{
    if (g_BluetoothClient && g_BluetoothClient->IsConnected()) {
        g_BluetoothClient->SendMessage(message);
        LOGF_INFO("Sent WebSocket message: %s", message.c_str());
        return true;
    }
    LOGF_WARN("Cannot send WebSocket message - not connected");
    return false;
}

bool OFS_BluetoothLEAdapter::SendCommand(uint8_t cmd, uint8_t param) noexcept
{
#ifdef WIN32
    if (!connected_) {
        return false;
    }

    uint8_t checksum = (cmd + param) & 0xFF;
    uint8_t data[] = {0xAA, cmd, param, checksum};

    LOGF_INFO("Bluetooth command prepared: %02X %02X %02X %02X", data[0], data[1], data[2], data[3]);

    HDEVINFO deviceInfoSet = SetupDiGetClassDevs(&GUID_BTHLE_DEVICE, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (deviceInfoSet == INVALID_HANDLE_VALUE) {
        LOG_ERROR("Failed to get Bluetooth device info set");
        return false;
    }

    SP_DEVICE_INTERFACE_DATA deviceInterfaceData;
    deviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    bool foundDevice = false;
    DWORD deviceIndex = 0;
    std::wstring devicePath;

    while (SetupDiEnumDeviceInterfaces(deviceInfoSet, NULL, &GUID_BTHLE_DEVICE, deviceIndex, &deviceInterfaceData)) {
        DWORD detailSize = 0;
        SetupDiGetDeviceInterfaceDetail(deviceInfoSet, &deviceInterfaceData, NULL, 0, &detailSize, NULL);

        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
            deviceIndex++;
            continue;
        }

        PSP_DEVICE_INTERFACE_DETAIL_DATA detailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)new char[detailSize];
        detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

        if (!SetupDiGetDeviceInterfaceDetail(deviceInfoSet, &deviceInterfaceData, detailData, detailSize, &detailSize, NULL)) {
            delete[] detailData;
            deviceIndex++;
            continue;
        }

        BTH_ADDR addr = 0;
        if (swscanf_s(detailData->DevicePath, L"%*[^#]#%llx", &addr) == 1) {
            if (addr == deviceAddress_) {
                devicePath = detailData->DevicePath;
                foundDevice = true;
                delete[] detailData;
                break;
            }
        }

        delete[] detailData;
        deviceIndex++;
    }

    SetupDiDestroyDeviceInfoList(deviceInfoSet);

    if (!foundDevice || devicePath.empty()) {
        LOG_ERROR("Failed to find BLE device path");
        return false;
    }

    HANDLE bleDevice = CreateFile(devicePath.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (bleDevice == INVALID_HANDLE_VALUE) {
        LOG_ERROR("Failed to open BLE device");
        return false;
    }

    USHORT serviceCount = 0;
    BluetoothGATTGetServices(bleDevice, 0, NULL, &serviceCount, BLUETOOTH_GATT_FLAG_NONE);
    if (serviceCount == 0) {
        CloseHandle(bleDevice);
        LOG_ERROR("No services found on BLE device");
        return false;
    }

    std::vector<BLUETOOTH_GATT_SERVICE> services(serviceCount);
    BluetoothGATTGetServices(bleDevice, serviceCount, services.data(), &serviceCount, BLUETOOTH_GATT_FLAG_NONE);

    BLUETOOTH_GATT_SERVICE* hismithService = nullptr;
    for (auto& svc : services) {
        if (svc.ServiceUuid.Type == BluetoothUuidType::L2CAP || svc.ServiceUuid.Value.Long == 0xffe5 || svc.ServiceUuid.Value.ShortRegion == 0xffe5) {
            hismithService = &svc;
            break;
        }
    }

    if (!hismithService) {
        CloseHandle(bleDevice);
        LOG_ERROR("Hismith service not found");
        return false;
    }

    USHORT charCount = 0;
    BluetoothGATTGetCharacteristics(bleDevice, hismithService, 0, NULL, &charCount, BLUETOOTH_GATT_FLAG_NONE);
    if (charCount == 0) {
        CloseHandle(bleDevice);
        LOG_ERROR("No characteristics found");
        return false;
    }

    std::vector<BLUETOOTH_GATT_CHARACTERISTIC> characteristics(charCount);
    BluetoothGATTGetCharacteristics(bleDevice, hismithService, charCount, characteristics.data(), &charCount, BLUETOOTH_GATT_FLAG_NONE);

    BLUETOOTH_GATT_CHARACTERISTIC* writeChar = nullptr;
    for (auto& ch : characteristics) {
        if (ch.CharacteristicUuid.Value.Long == 0xffe9 || ch.CharacteristicUuid.Value.ShortRegion == 0xffe9) {
            writeChar = &ch;
            break;
        }
    }

    if (!writeChar) {
        for (auto& ch : characteristics) {
            if (ch.Properties & BLUETOOTH_GATT_CHARACTERISTIC_PROPERTIES_WRITE) {
                writeChar = &ch;
                break;
            }
        }
    }

    if (!writeChar) {
        CloseHandle(bleDevice);
        LOG_ERROR("No writable characteristic found");
        return false;
    }

    BLUETOOTH_GATT_CHARACTERISTIC_VALUE writeValue = {};
    writeValue.DataSize = sizeof(data);
    memcpy(writeValue.Data, data, sizeof(data));

    DWORD result = BluetoothGATTSetCharacteristicValue(bleDevice, writeChar, &writeValue, NULL, BLUETOOTH_GATT_FLAG_NONE);
    
    CloseHandle(bleDevice);

    if (result != ERROR_SUCCESS) {
        LOGF_ERROR("Failed to write characteristic: %lu", result);
        return false;
    }

    LOGF_INFO("Bluetooth command sent: %02X %02X %02X %02X", data[0], data[1], data[2], data[3]);
    return true;
#else
    return false;
#endif
}

bool OFS_BluetoothLEAdapter::WriteToCharacteristic(const std::vector<uint8_t>& data) noexcept
{
    return SendCommand(data.size() > 1 ? data[1] : 0, data.size() > 2 ? data[2] : 0);
}

std::vector<std::string> OFS_BluetoothLEAdapter::ScanForDevices() noexcept
{
    std::vector<std::string> deviceNames;

#ifdef WIN32
    LOG_INFO("Scanning for Bluetooth LE devices...");

    HDEVINFO deviceInfoSet = SetupDiGetClassDevs(NULL, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (deviceInfoSet == INVALID_HANDLE_VALUE) {
        LOG_ERROR("Failed to get Bluetooth device info set");
        return deviceNames;
    }

    SP_DEVICE_INTERFACE_DATA deviceInterfaceData;
    deviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    DWORD deviceIndex = 0;

    while (SetupDiEnumDeviceInterfaces(deviceInfoSet, NULL, NULL, deviceIndex, &deviceInterfaceData)) {
        DWORD detailSize = 0;
        SetupDiGetDeviceInterfaceDetail(deviceInfoSet, &deviceInterfaceData, NULL, 0, &detailSize, NULL);

        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
            deviceIndex++;
            continue;
        }

        PSP_DEVICE_INTERFACE_DETAIL_DATA detailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)new char[detailSize];
        detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

        if (!SetupDiGetDeviceInterfaceDetail(deviceInfoSet, &deviceInterfaceData, detailData, detailSize, &detailSize, NULL)) {
            delete[] detailData;
            deviceIndex++;
            continue;
        }

        BLUETOOTH_DEVICE_INFO bthDeviceInfo = {};
        bthDeviceInfo.dwSize = sizeof(BLUETOOTH_DEVICE_INFO);

        BTH_ADDR addr = 0;
        if (swscanf_s(detailData->DevicePath, L"%*[^#]#%llx", &addr) == 1) {
            bthDeviceInfo.Address.ullLong = addr;

            if (BluetoothGetDeviceInfo(NULL, &bthDeviceInfo) == ERROR_SUCCESS) {
                std::wstring nameW(bthDeviceInfo.szName);
                std::string name(nameW.begin(), nameW.end());

                if (!name.empty()) {
                    deviceNames.push_back(name);
                    LOGF_INFO("Found: %s", name.c_str());
                }
            }
        }

        delete[] detailData;
        deviceIndex++;
    }

    SetupDiDestroyDeviceInfoList(deviceInfoSet);

    LOGF_INFO("Found %d Bluetooth LE devices", (int)deviceNames.size());
#endif

    return deviceNames;
}

std::string OFS_BluetoothLEAdapter::GetDeviceNameFromAddress(const std::string& address) noexcept
{
#ifdef WIN32
    try {
        BTH_ADDR addr = 0;
        if (sscanf_s(address.c_str(), "%llx", &addr) != 1) {
            return "";
        }

        BLUETOOTH_DEVICE_INFO deviceInfo = {};
        deviceInfo.dwSize = sizeof(BLUETOOTH_DEVICE_INFO);
        deviceInfo.Address.ullLong = addr;

        DWORD result = BluetoothGetDeviceInfo(NULL, &deviceInfo);
        if (result == ERROR_SUCCESS) {
            std::wstring nameW(deviceInfo.szName);
            return std::string(nameW.begin(), nameW.end());
        }
    }
    catch (...) {
    }
#endif
    return "";
}

BLEDeviceType OFS_BluetoothLEAdapter::DetectDeviceType(const std::string& deviceName) noexcept
{
    if (deviceName.find("HISMITH") != std::string::npos || 
        deviceName.find("Hismith") != std::string::npos) {
        return BLEDeviceType::Hismith;
    }
    if (deviceName.find("LVS-") != std::string::npos || 
        deviceName.find("Lovense") != std::string::npos) {
        return BLEDeviceType::LovenseGen2;
    }
    if (deviceName.find("Max") != std::string::npos ||
        deviceName.find("Nora") != std::string::npos ||
        deviceName.find("Lush") != std::string::npos ||
        deviceName.find("Edge") != std::string::npos ||
        deviceName.find("Domi") != std::string::npos ||
        deviceName.find("Ambi") != std::string::npos) {
        return BLEDeviceType::LovenseGen2;
    }
    return BLEDeviceType::Unknown;
}

bool OFS_BluetoothLEAdapter::SendLovenseCommand(const std::string& cmd) noexcept
{
#ifdef WIN32
    if (!connected_) {
        return false;
    }

    std::string fullCmd = cmd + ";";
    std::vector<uint8_t> data(fullCmd.begin(), fullCmd.end());

    LOGF_INFO("Lovense command: %s", fullCmd.c_str());

    HDEVINFO deviceInfoSet = SetupDiGetClassDevs(&GUID_BTHLE_DEVICE, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (deviceInfoSet == INVALID_HANDLE_VALUE) {
        LOG_ERROR("Failed to get Bluetooth device info set");
        return false;
    }

    SP_DEVICE_INTERFACE_DATA deviceInterfaceData;
    deviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    bool foundDevice = false;
    DWORD deviceIndex = 0;
    std::wstring devicePath;

    while (SetupDiEnumDeviceInterfaces(deviceInfoSet, NULL, &GUID_BTHLE_DEVICE, deviceIndex, &deviceInterfaceData)) {
        DWORD detailSize = 0;
        SetupDiGetDeviceInterfaceDetail(deviceInfoSet, &deviceInterfaceData, NULL, 0, &detailSize, NULL);

        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
            deviceIndex++;
            continue;
        }

        PSP_DEVICE_INTERFACE_DETAIL_DATA detailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)new char[detailSize];
        detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

        if (!SetupDiGetDeviceInterfaceDetail(deviceInfoSet, &deviceInterfaceData, detailData, detailSize, &detailSize, NULL)) {
            delete[] detailData;
            deviceIndex++;
            continue;
        }

        BTH_ADDR addr = 0;
        if (swscanf_s(detailData->DevicePath, L"%*[^#]#%llx", &addr) == 1) {
            if (addr == deviceAddress_) {
                devicePath = detailData->DevicePath;
                foundDevice = true;
                delete[] detailData;
                break;
            }
        }

        delete[] detailData;
        deviceIndex++;
    }

    SetupDiDestroyDeviceInfoList(deviceInfoSet);

    if (!foundDevice || devicePath.empty()) {
        LOG_ERROR("Failed to find BLE device path");
        return false;
    }

    HANDLE bleDevice = CreateFile(devicePath.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (bleDevice == INVALID_HANDLE_VALUE) {
        LOG_ERROR("Failed to open BLE device");
        return false;
    }

    USHORT serviceCount = 0;
    BluetoothGATTGetServices(bleDevice, 0, NULL, &serviceCount, BLUETOOTH_GATT_FLAG_NONE);
    if (serviceCount == 0) {
        CloseHandle(bleDevice);
        LOG_ERROR("No services found on BLE device");
        return false;
    }

    std::vector<BLUETOOTH_GATT_SERVICE> services(serviceCount);
    BluetoothGATTGetServices(bleDevice, serviceCount, services.data(), &serviceCount, BLUETOOTH_GATT_FLAG_NONE);

    BLUETOOTH_GATT_SERVICE* lovenseService = nullptr;
    GUID targetService = (deviceType_ == BLEDeviceType::LovenseGen1) ? GUID_LOVENSE_GEN1_SERVICE : GUID_LOVENSE_NORDIC_SERVICE;
    
    for (auto& svc : services) {
        if (svc.ServiceUuid.Value.Long == targetService.Data1 ||
            svc.ServiceUuid.Value.ShortRegion == 0xFFF0 ||
            svc.ServiceUuid.Value.ShortRegion == 0x6E40) {
            lovenseService = &svc;
            break;
        }
    }

    if (!lovenseService) {
        CloseHandle(bleDevice);
        LOG_ERROR("Lovense service not found");
        return false;
    }

    USHORT charCount = 0;
    BluetoothGATTGetCharacteristics(bleDevice, lovenseService, 0, NULL, &charCount, BLUETOOTH_GATT_FLAG_NONE);
    if (charCount == 0) {
        CloseHandle(bleDevice);
        LOG_ERROR("No characteristics found");
        return false;
    }

    std::vector<BLUETOOTH_GATT_CHARACTERISTIC> characteristics(charCount);
    BluetoothGATTGetCharacteristics(bleDevice, lovenseService, charCount, characteristics.data(), &charCount, BLUETOOTH_GATT_FLAG_NONE);

    BLUETOOTH_GATT_CHARACTERISTIC* writeChar = nullptr;
    for (auto& ch : characteristics) {
        if (ch.Properties & BLUETOOTH_GATT_CHARACTERISTIC_PROPERTIES_WRITE ||
            ch.Properties & BLUETOOTH_GATT_CHARACTERISTIC_PROPERTIES_WRITE_WITHOUT_RESPONSE) {
            writeChar = &ch;
            break;
        }
    }

    if (!writeChar) {
        CloseHandle(bleDevice);
        LOG_ERROR("No writable characteristic found");
        return false;
    }

    BLUETOOTH_GATT_CHARACTERISTIC_VALUE writeValue = {};
    writeValue.DataSize = (USHORT)data.size();
    memcpy(writeValue.Data, data.data(), data.size());

    DWORD result = BluetoothGATTSetCharacteristicValue(bleDevice, writeChar, &writeValue, NULL, BLUETOOTH_GATT_FLAG_NONE);
    
    CloseHandle(bleDevice);

    if (result != ERROR_SUCCESS) {
        LOGF_ERROR("Failed to write characteristic: %lu", result);
        return false;
    }

    LOGF_INFO("Lovense command sent successfully");
    return true;
#else
    return false;
#endif
}
