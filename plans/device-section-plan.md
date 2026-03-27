# Device Section Implementation Plan

## Current State Analysis

The existing implementation has:
- **OFS_DeviceManager** - Uses buttplug FFI library (buttplug_rs_ffi.dll) which user wants removed
- **OFS_DevicePanel** - Has a hidden title (uses `###DEVICE_PANEL` which hides the window title in ImGui)
- **Connection flow** - User clicks "Connect to Device" but scanning is separate

## Requirements

1. **Standalone Implementation**: Not using buttplug library, but implementing the buttplug protocol via WebSocket
2. **Visible Header**: Add proper title to device panel UI
3. **Auto-scan**: Connect button should trigger device scanning

## Implementation Plan

### Phase 1: Architecture Redesign

```
┌─────────────────────────────────────────────────────────────────┐
│                     OpenFunscripter                             │
├─────────────────────────────────────────────────────────────────┤
│  DevicePanel UI                                                 │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │ "Device Control" ◄── TITLE (currently hidden)           │   │
│  │                                                          │   │
│  │ [Connect to Device] ──────► Connect + AutoScan          │   │
│  │                                                          │   │
│  │ Status: Connected    [Disconnect]                       │   │
│  │                                                          │   │
│  │ [Scan for Devices] or "Scanning..." [Stop]              │   │
│  │                                                          │   │
│  │ Device: [Dropdown]                                       │   │
│  └─────────────────────────────────────────────────────────┘   │
├─────────────────────────────────────────────────────────────────┤
│  OFS_DeviceManager                                             │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │ WebSocket Client (civetweb)                             │   │
│  │   │                                                      │   │
│  │   ▼                                                      │   │
│  │ Buttplug Protocol Handler                                │   │
│  │   │                                                      │   │
│  │   ▼                                                      │   │
│  │ ws://localhost:12345 (Intiface Central)                 │   │
│  └─────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
```

### Phase 2: Key Changes

#### 2.1 Device Panel UI (OFS_DevicePanel.cpp)

**Current (line 24):**
```cpp
if (!ImGui::Begin(WindowId, &showWindow, ...))  // WindowId = "###DEVICE_PANEL"
```

**Fix:** Add visible title
```cpp
if (!ImGui::Begin("Device Control###DEVICE_PANEL", &showWindow, ...))
```

Or better, follow the pattern used by other panels:
```cpp
if (!ImGui::Begin(TR_ID(WindowId, Tr::DEVICE_CONTROL), &showWindow, ...))
```

#### 2.2 Connect + AutoScan

**Current flow:**
1. Click "Connect to Device" → EnsureConnected()
2. Click "Scan for Devices" → StartScanning()

**New flow:**
1. Click "Connect to Device" → EnsureConnected() + StartScanning()

#### 2.3 Device Manager (OFS_DeviceManager)

Replace FFI-based implementation with WebSocket client:

**Remove:**
- FFI function pointers
- LoadFFILibrary()
- InitEmbeddedServer() 
- All ffi.* function calls

**Add:**
- WebSocket connection using civetweb (`mg_connect_websocket_client`)
- JSON message handling for buttplug protocol
- Message queue for async communication

### Phase 3: Buttplug Protocol Implementation

Connect to Intiface Central via WebSocket at `ws://localhost:12345`

**Key Messages:**

| Message | Purpose |
|---------|---------|
| `{"Id":1,"Type":"RequestServerInfo"}` | Handshake |
| `{"Id":2,"Type":"StartScanning"}` | Start scanning |
| `{"Id":3,"Type":"StopScanning"}` | Stop scanning |
| `{"Id":4,"Type":"LinearCmd","DeviceIndex":0,"Cmd":{"Position":0.5,"Duration":100}}` | Send position |
| `{"Id":5,"Type":"StopDeviceCmd","DeviceIndex":0}` | Stop device |

**Incoming Events:**
- `ServerInfo` - Handshake response
- `DeviceAdded` - New device found
- `DeviceRemoved` - Device disconnected
- `ScanningFinished` - Scan complete

### Phase 4: File Changes Summary

| File | Changes |
|------|---------|
| `src/device/OFS_DevicePanel.cpp` | Add visible title, auto-scan on connect |
| `src/device/OFS_DevicePanel.h` | Possibly add WindowId constant |
| `src/device/OFS_DeviceManager.cpp` | Replace FFI with WebSocket client |
| `src/device/OFS_DeviceManager.h` | Remove FFI types, add WebSocket members |
| `src/state/DeviceState.h` | Add server address field |

### Phase 5: Implementation Order

1. **Fix Device Panel Title** - Quick fix, visible immediately
2. **Update Connect Button** - Auto-scan after connect
3. **Redesign DeviceManager** - Replace FFI with WebSocket
4. **Test with Intiface Central** - Verify device discovery and control

## Testing Checklist

- [ ] Device panel shows "Device Control" title
- [ ] Clicking "Connect to Device" connects AND starts scanning
- [ ] Devices appear in dropdown when found
- [ ] Can select device and send test commands
- [ ] Position changes during video playback work
- [ ] Disconnect properly cleans up WebSocket
