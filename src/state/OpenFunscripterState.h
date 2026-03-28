#pragma once

#include <string>
#include <vector>
#include <array>

#include "OFS_Reflection.h"
#include "OFS_StateHandle.h"
#include "../src/UI/OFS_PatternPicker.h"

struct RecentFile 
{
	std::string name;
	std::string projectPath;
};

REFL_TYPE(RecentFile)
	REFL_FIELD(name)
	REFL_FIELD(projectPath)
REFL_END

struct OpenFunscripterState 
{
	static constexpr auto StateName = "OpenFunscripter";

	std::vector<RecentFile> recentFiles;
    std::string lastPath;

	struct HeatmapSettings {
		int32_t defaultWidth = 2000;
		int32_t defaultHeight = 50;
		std::string defaultPath = "./";
	} heatmapSettings;

	struct ActionEditorSettings {
		// 11 positions for the action editor buttons
		// Index 0 = top row (100), Index 10 = bottom row (0)
		std::array<int32_t, 11> positions = {100, 90, 80, 70, 60, 50, 40, 30, 20, 10, 0};
		// Min/max range for position mapping
		int32_t minPos = 0;
		int32_t maxPos = 100;
	} actionEditorPositions;

    bool showDebugLog = false;
    bool showVideo = true;

    bool showActionEditor = false;
    bool showStatistics = true;
    bool alwaysShowBookmarkLabels = false;
    bool showHistory = true;
    bool showSimulator = true;
    bool showSpecialFunctions = false;
    bool showWsApi = false;
    bool showChapterManager = false;
    bool showChapterBar = true;
    bool showPatternsWindow = false;
    bool showDevicePanel = false;
    bool showVoiceInputPanel = true;
    bool showWaveform = false;

    PatternLayout patternLayout;

    inline static OpenFunscripterState& State(uint32_t stateHandle) noexcept
    {
        return OFS_AppState<OpenFunscripterState>(stateHandle).Get();
    }

    void addRecentFile(const RecentFile& recentFile) noexcept;

    static void RegisterAll() noexcept;
};

REFL_TYPE(OpenFunscripterState::HeatmapSettings)
	REFL_FIELD(defaultWidth)
	REFL_FIELD(defaultHeight)
	REFL_FIELD(defaultPath)
REFL_END

REFL_TYPE(OpenFunscripterState::ActionEditorSettings)
	REFL_FIELD(positions)
	REFL_FIELD(minPos)
	REFL_FIELD(maxPos)
REFL_END

REFL_TYPE(OpenFunscripterState)
    REFL_FIELD(recentFiles)
    REFL_FIELD(lastPath)
    REFL_FIELD(showDebugLog)
    REFL_FIELD(showVideo)
    REFL_FIELD(showActionEditor)
    REFL_FIELD(showStatistics)
    REFL_FIELD(alwaysShowBookmarkLabels)
    REFL_FIELD(showHistory)
    REFL_FIELD(showSimulator)
    REFL_FIELD(showSpecialFunctions)
    REFL_FIELD(showWsApi)
    REFL_FIELD(showChapterManager)
    REFL_FIELD(showChapterBar)
    REFL_FIELD(showPatternsWindow)
    REFL_FIELD(showDevicePanel)
    REFL_FIELD(showVoiceInputPanel)
    REFL_FIELD(showWaveform)
    REFL_FIELD(patternLayout)
    REFL_FIELD(actionEditorPositions)
REFL_END