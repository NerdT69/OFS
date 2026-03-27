# Implementation Plan: Add Squirt Axis and Edit Min/Max Feature

## Overview
This plan outlines the implementation of:
1. Adding "squirt" as a new axis name to the Funscript system
2. Adding timeline right-click context menu for editing action min/max values
3. Ensuring squirt can use 0-1 (binary on/off) values

## Current State Analysis

### Axis Names (Funscript::AxisNames)
- **Location**: `OFS-lib/Funscript/Funscript.cpp`, lines 45-56
- **Current**: Array of 9 elements: surge, sway, suck, twist, roll, pitch, vib, pump, raw
- **Need**: Add "squirt" as 10th element

### Timeline Context Menu
- **Location**: `OFS-lib/UI/OFS_ScriptTimeline.cpp`, lines 432-516
- **Current**: Has context menu with Scripts, Rendering, Waveform submenus
- **Need**: Add "Edit Min/Max" submenu

### Device State for Min/Max
- **Location**: `src/state/DeviceState.h`, lines 29-31
- **Current**: Has `minPosition` and `maxPosition` fields (0-100 range)

## Implementation Steps

### Step 1: Add Squirt to AxisNames
**File**: `OFS-lib/Funscript/Funscript.cpp`
- Change array size from `std::array<const char*, 9>` to `std::array<const char*, 10>`
- Add "squirt" as the 10th element

**File**: `OFS-lib/Funscript/Funscript.h`
- Change declaration from `std::array<const char*, 9> AxisNames` to `std::array<const char*, 10> AxisNames`

### Step 2: Add Context Menu for Min/Max Editing
**File**: `OFS-lib/UI/OFS_ScriptTimeline.cpp`
- In the right-click context menu (around line 433), add:
  - New "Edit Min/Max" submenu
  - Input fields for minPosition and maxPosition
  - Use DeviceState to read/write values
  - Apply to selected actions or default range

### Step 3: Handle 0-1 Range for Squirt
- No special code changes needed - the system already supports 0-100
- For squirt (binary), users will manually set positions to 0 or 1
- The DeviceState minPosition/maxPosition controls device mapping

## Files to Modify
1. `OFS-lib/Funscript/Funscript.h` - Update array declaration
2. `OFS-lib/Funscript/Funscript.cpp` - Add "squirt" to AxisNames
3. `OFS-lib/UI/OFS_ScriptTimeline.cpp` - Add context menu for min/max