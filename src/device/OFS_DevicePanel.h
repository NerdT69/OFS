#pragma once

#include "imgui.h"
#include "OFS_Reflection.h"

#include <memory>

class OFS_DeviceManager;

class DevicePanel
{
private:
    uint32_t stateHandle = 0xFFFF'FFFF;
    bool showWindow = false;
    
public:
    static constexpr const char* WindowId = "###DEVICE_PANEL";
    
    DevicePanel() noexcept;
    
    void Init(OFS_DeviceManager* mgr) noexcept;
    void Toggle() noexcept;
    void Show(bool* open) noexcept;
    
private:
    void ShowWindow() noexcept;
};
