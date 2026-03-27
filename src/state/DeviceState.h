#pragma once

#include <string>
#include <vector>
#include "OFS_StateHandle.h"

struct DeviceInfo
{
    int deviceIndex = -1;
    std::string deviceName;
    std::vector<std::string> supportedCommands;
};

struct DeviceState
{
    static constexpr auto StateName = "DeviceState";

    // Connection settings
    bool enabled = false;
    bool autoConnect = false;
    
    // Playback settings
    bool pauseWithVideo = true;
    bool syncPlaybackSpeed = false;
    
    // Device to script sync (reverse direction)
    bool syncDeviceToScript = false;
    
    // Position mapping (0-100 funscript to device range)
    int minPosition = 0;
    int maxPosition = 100;
    
    // Runtime state (not persisted)
    bool connected = false;
    bool scanning = false;
    std::vector<DeviceInfo> availableDevices;
    int selectedDeviceIndex = -1;
    
    // Last position sent to avoid duplicate commands
    float lastPosition = -1.f;
    
    // Last position synced to script (for debouncing device-to-script sync)
    float lastSyncedPosition = -1.f;

    static inline DeviceState& State(uint32_t stateHandle) noexcept {
        return OFS_AppState<DeviceState>(stateHandle).Get();
    }
};

REFL_TYPE(DeviceState)
    REFL_FIELD(enabled)
    REFL_FIELD(autoConnect)
    REFL_FIELD(pauseWithVideo)
    REFL_FIELD(syncPlaybackSpeed)
    REFL_FIELD(syncDeviceToScript)
    REFL_FIELD(minPosition)
    REFL_FIELD(maxPosition)
REFL_END
