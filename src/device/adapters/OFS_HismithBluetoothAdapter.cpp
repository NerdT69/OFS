#include "OFS_HismithBluetoothAdapter.h"

#include "OFS_FileLogging.h"

#include <iostream>
#include <sstream>
#include <algorithm>
#include <cstring>

#ifdef WIN32
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "bthprops.lib")
#endif

OFS_HismithBluetoothAdapter::OFS_HismithBluetoothAdapter() noexcept
#ifdef WIN32
    : bleDeviceHandle_(INVALID_HANDLE_VALUE)
#endif
{
}

OFS_HismithBluetoothAdapter::~OFS_HismithBluetoothAdapter() noexcept
{
    Disconnect();
}

bool OFS_HismithBluetoothAdapter::Connect(const std::string& deviceName) noexcept
{
#ifdef WIN32
    return FindAndConnectDevice(deviceName);
#else
    LOG_WARN("Bluetooth LE not supported on this platform");
    return false;
#endif
}

#ifdef WIN32
bool OFS_HismithBluetoothAdapter::FindAndConnectDevice(const std::string& targetName) noexcept
{
    LOG_INFO("Starting Bluetooth LE device scan for Hismith devices...");

    HDEVINFO deviceInfoSet = SetupDiGetClassDevs(&GUID_HISMITH_BLE_SERVICE, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (deviceInfoSet == INVALID_HANDLE_VALUE) {
        LOG_ERROR("Failed to get Bluetooth device info set");
        return false;
    }

    SP_DEVICE_INTERFACE_DATA deviceInterfaceData;
    deviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    bool found = false;
    DWORD deviceIndex = 0;
    std::string foundDeviceName;
    BTH_ADDR foundAddress = 0;
    std::wstring foundDevicePath;

    while (SetupDiEnumDeviceInterfaces(deviceInfoSet, NULL, &GUID_HISMITH_BLE_SERVICE, deviceIndex, &deviceInterfaceData)) {
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

                LOGF_INFO("Found Bluetooth device: %s", name.c_str());

                bool isHismith = (name.find("HISMITH") != std::string::npos) ||
                                 (name.find("Hismith") != std::string::npos) ||
                                 (name.find("hismith") != std::string::npos);

                if (isHismith || targetName.empty() || name == targetName) {
                    foundDeviceName = name;
                    foundAddress = addr;
                    foundDevicePath = detailData->DevicePath;
                    found = true;
                    delete[] detailData;
                    break;
                }
            }
        }

        delete[] detailData;
        deviceIndex++;
    }

    SetupDiDestroyDeviceInfoList(deviceInfoSet);

    if (!found) {
        LOG_WARN("No Hismith Bluetooth device found");
        return false;
    }

    deviceName_ = foundDeviceName;
    deviceAddress_ = foundAddress;

    bleDeviceHandle_ = CreateFile(foundDevicePath.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (bleDeviceHandle_ == INVALID_HANDLE_VALUE) {
        LOG_ERROR("Failed to open BLE device");
        return false;
    }

    connected_ = true;
    LOGF_INFO("Connected to %s via Bluetooth LE", deviceName_.c_str());
    return true;
}
#endif

void OFS_HismithBluetoothAdapter::Disconnect() noexcept
{
#ifdef WIN32
    if (bleDeviceHandle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(bleDeviceHandle_);
        bleDeviceHandle_ = INVALID_HANDLE_VALUE;
    }
#endif
    connected_ = false;
    deviceName_.clear();
#ifdef WIN32
    deviceAddress_ = 0;
#endif
    LOG_INFO("Hismith Bluetooth disconnected");
}

bool OFS_HismithBluetoothAdapter::SetSpeed(int speedPercent) noexcept
{
    if (!connected_) return false;
    speedPercent = std::max(0, std::min(100, speedPercent));
    return SendHismithCommand(0x04, static_cast<uint8_t>(speedPercent));
}

bool OFS_HismithBluetoothAdapter::SetVibration(int vibrationPercent) noexcept
{
    if (!connected_) return false;
    vibrationPercent = std::max(0, std::min(100, vibrationPercent));
    return SendHismithCommand(0x04, static_cast<uint8_t>(vibrationPercent));
}

bool OFS_HismithBluetoothAdapter::SetPosition(int positionPercent) noexcept
{
    if (!connected_) return false;
    positionPercent = std::max(10, std::min(100, positionPercent));
    return SendHismithCommand(0x04, static_cast<uint8_t>(positionPercent));
}

bool OFS_HismithBluetoothAdapter::SetStrokeType(bool autoStroke) noexcept
{
    if (!connected_) return false;
    uint8_t mode = autoStroke ? 0x01 : 0x00;
    return SendHismithCommand(0x05, mode);
}

bool OFS_HismithBluetoothAdapter::Stop() noexcept
{
    if (!connected_) return false;
    SendHismithCommand(0x04, 0x00);
    return true;
}

bool OFS_HismithBluetoothAdapter::SendPosition(float positionPercent) noexcept
{
    if (!connected_) return false;
    int position = static_cast<int>(positionPercent / 100.0f * 90.0f) + 10;
    position = std::max(10, std::min(100, position));
    return SetPosition(position);
}

bool OFS_HismithBluetoothAdapter::SendHismithCommand(uint8_t cmd, uint8_t param) noexcept
{
#ifdef WIN32
    if (!connected_ || bleDeviceHandle_ == INVALID_HANDLE_VALUE) {
        return false;
    }

    uint8_t checksum = (cmd + param) & 0xFF;
    uint8_t data[] = {0xAA, cmd, param, checksum};

    USHORT serviceCount = 0;
    BluetoothGATTGetServices(bleDeviceHandle_, 0, NULL, &serviceCount, BLUETOOTH_GATT_FLAG_NONE);
    if (serviceCount == 0) {
        LOG_ERROR("No services found on BLE device");
        return false;
    }

    std::vector<BLUETOOTH_GATT_SERVICE> services(serviceCount);
    BluetoothGATTGetServices(bleDeviceHandle_, serviceCount, services.data(), &serviceCount, BLUETOOTH_GATT_FLAG_NONE);

    BLUETOOTH_GATT_SERVICE* hismithService = nullptr;
    for (auto& svc : services) {
        if (svc.ServiceUuid.Value.Long == 0xffe5 || svc.ServiceUuid.Value.ShortRegion == 0xffe5) {
            hismithService = &svc;
            break;
        }
    }

    if (!hismithService) {
        LOG_ERROR("Hismith service not found");
        return false;
    }

    USHORT charCount = 0;
    BluetoothGATTGetCharacteristics(bleDeviceHandle_, hismithService, 0, NULL, &charCount, BLUETOOTH_GATT_FLAG_NONE);
    if (charCount == 0) {
        LOG_ERROR("No characteristics found");
        return false;
    }

    std::vector<BLUETOOTH_GATT_CHARACTERISTIC> characteristics(charCount);
    BluetoothGATTGetCharacteristics(bleDeviceHandle_, hismithService, charCount, characteristics.data(), &charCount, BLUETOOTH_GATT_FLAG_NONE);

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
        LOG_ERROR("No writable characteristic found");
        return false;
    }

    BLUETOOTH_GATT_CHARACTERISTIC_VALUE writeValue = {};
    writeValue.DataSize = sizeof(data);
    memcpy(writeValue.Data, data, sizeof(data));

    DWORD result = BluetoothGATTSetCharacteristicValue(bleDeviceHandle_, writeChar, &writeValue, NULL, BLUETOOTH_GATT_FLAG_NONE);

    if (result != ERROR_SUCCESS) {
        LOGF_ERROR("Failed to write characteristic: %lu", result);
        return false;
    }

    LOGF_INFO("Hismith BT command sent: %02X %02X %02X %02X", data[0], data[1], data[2], data[3]);
    return true;
#else
    return false;
#endif
}

std::vector<std::string> OFS_HismithBluetoothAdapter::ScanForDevices() noexcept
{
    std::vector<std::string> deviceNames;

#ifdef WIN32
    LOG_INFO("Scanning for Hismith Bluetooth LE devices...");

    HDEVINFO deviceInfoSet = SetupDiGetClassDevs(&GUID_HISMITH_BLE_SERVICE, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (deviceInfoSet == INVALID_HANDLE_VALUE) {
        LOG_ERROR("Failed to get Bluetooth device info set");
        return deviceNames;
    }

    SP_DEVICE_INTERFACE_DATA deviceInterfaceData;
    deviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    DWORD deviceIndex = 0;

    while (SetupDiEnumDeviceInterfaces(deviceInfoSet, NULL, &GUID_HISMITH_BLE_SERVICE, deviceIndex, &deviceInterfaceData)) {
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

                bool isHismith = (name.find("HISMITH") != std::string::npos) ||
                                 (name.find("Hismith") != std::string::npos) ||
                                 (name.find("hismith") != std::string::npos);

                if (isHismith && !name.empty()) {
                    deviceNames.push_back(name);
                    LOGF_INFO("Found Hismith device: %s", name.c_str());
                }
            }
        }

        delete[] detailData;
        deviceIndex++;
    }

    SetupDiDestroyDeviceInfoList(deviceInfoSet);

    LOGF_INFO("Found %d Hismith devices", (int)deviceNames.size());
#endif

    return deviceNames;
}
