#pragma once

#include <string>
#include <vector>
#include <cstdint>

#include "Funscript.h"
#include "OFS_ScriptTimeline.h"

// Forward declarations
class OpenFunscripter;

// Pattern button structure
struct PatternButton {
    std::string name;
    std::string funscriptPath;
    
    // For patterns created from selection (stored inline, not from file)
    bool isInlinePattern = false;
    std::vector<FunscriptAction> inlineActions;
};

REFL_TYPE(PatternButton)
    REFL_FIELD(name)
    REFL_FIELD(funscriptPath)
    REFL_FIELD(isInlinePattern)
    REFL_FIELD(inlineActions)
REFL_END

// Pattern layout structure - simplified (fixed 3 columns)
struct PatternLayout {
    std::vector<PatternButton> buttons;
};

REFL_TYPE(PatternLayout)
    REFL_FIELD(buttons)
REFL_END

class PatternPicker {
private:
    PatternLayout layout;
    int hoveredButtonIndex = -1;
    int selectedButtonIndex = -1;
    
    // Temporary storage for dialogs
    std::string newButtonName;
    std::string newButtonPath;
    
    // For renaming
    int renamingIndex = -1;
    char renameBuffer[256];
    
public:
    static constexpr const char* WindowId = "###PATTERNS";
    static constexpr int FIXED_COLUMNS = 3;
    
    PatternPicker() noexcept;
    void ShowPatternsWindow(bool* open) noexcept;
    
    // Load a funscript file and return its actions
    static bool LoadFunscriptActions(const std::string& path, FunscriptArray& actions) noexcept;
    
    // Insert pattern at current video time
    void InsertPattern(const PatternButton& button) noexcept;
    
    // Insert inline pattern (from selection)
    void InsertInlinePattern(const PatternButton& button) noexcept;
    
    // Get pattern by index
    const PatternButton* GetPattern(int index) const noexcept;
    int GetPatternCount() const noexcept;
    
    // Get/set layout for persistence
    const PatternLayout& GetLayout() const noexcept { return layout; }
    void SetLayout(const PatternLayout& newLayout) noexcept { layout = newLayout; }
};
