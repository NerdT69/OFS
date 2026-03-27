#include "OpenFunscripter.h"
#include "OFS_PatternPicker.h"
#include "OFS_Util.h"
#include "OFS_ImGui.h"
#include "imgui.h"
#include "imgui_stdlib.h"
#include "nlohmann/json.hpp"
#include <fstream>

PatternPicker::PatternPicker() noexcept
{
    // Default constructor - layout initialized with defaults
}

bool PatternPicker::LoadFunscriptActions(const std::string& path, FunscriptArray& actions) noexcept
{
    std::ifstream file(path);
    if (!file.is_open()) {
        LOGF_ERROR("Failed to open funscript file: %s", path.c_str());
        return false;
    }

    try {
        nlohmann::json json;
        file >> json;
        
        Funscript::Metadata metadata;
        Funscript tempScript;
        
        if (tempScript.Deserialize(json, &metadata, false)) {
            actions = tempScript.Data().Actions;
            return true;
        }
        else {
            LOG_ERROR("Failed to deserialize funscript");
            return false;
        }
    }
    catch (const nlohmann::json::parse_error& e) {
        LOGF_ERROR("JSON parse error: %s", e.what());
        return false;
    }
}

void PatternPicker::InsertPattern(const PatternButton& button) noexcept
{
    auto app = OpenFunscripter::ptr;
    if (!app || !app->LoadedProject->IsValid()) {
        return;
    }

    FunscriptArray actions;
    if (!LoadFunscriptActions(button.funscriptPath, actions)) {
        LOGF_ERROR("Failed to load pattern: %s", button.name.c_str());
        return;
    }

    if (actions.empty()) {
        LOGF_WARN("Pattern has no actions: %s", button.name.c_str());
        return;
    }

    float currentTime = app->player->CurrentTime();
    float offsetTime = currentTime - actions.begin()->atS;

    app->undoSystem->Snapshot(StateType::ADD_EDIT_ACTIONS, app->ActiveFunscript());

    for (const auto& action : actions) {
        FunscriptAction newAction(action.atS + offsetTime, action.pos);
        app->ActiveFunscript()->AddAction(newAction);
    }

    // Jump to end of pattern
    float lastActionTime = actions.rbegin()->atS + offsetTime;
    app->player->SetPositionExact(lastActionTime);

    LOGF_INFO("Inserted pattern '%s' at time %.3f", button.name.c_str(), currentTime);
}

void PatternPicker::InsertInlinePattern(const PatternButton& button) noexcept
{
    auto app = OpenFunscripter::ptr;
    if (!app || !app->LoadedProject->IsValid()) {
        return;
    }

    if (button.inlineActions.empty()) {
        LOGF_WARN("Pattern has no actions: %s", button.name.c_str());
        return;
    }

    float currentTime = app->player->CurrentTime();
    float offsetTime = currentTime - button.inlineActions.begin()->atS;

    app->undoSystem->Snapshot(StateType::ADD_EDIT_ACTIONS, app->ActiveFunscript());

    for (const auto& action : button.inlineActions) {
        FunscriptAction newAction(action.atS + offsetTime, action.pos);
        app->ActiveFunscript()->AddAction(newAction);
    }

    // Jump to end of pattern
    float lastActionTime = button.inlineActions.rbegin()->atS + offsetTime;
    app->player->SetPositionExact(lastActionTime);

    LOGF_INFO("Inserted inline pattern '%s' at time %.3f", button.name.c_str(), currentTime);
}

const PatternButton* PatternPicker::GetPattern(int index) const noexcept
{
    if (index >= 0 && index < (int)layout.buttons.size()) {
        return &layout.buttons[index];
    }
    return nullptr;
}

int PatternPicker::GetPatternCount() const noexcept
{
    return (int)layout.buttons.size();
}

void PatternPicker::ShowPatternsWindow(bool* open) noexcept
{
    if (open != nullptr && !(*open)) { return; }
    
    auto app = OpenFunscripter::ptr;
    
    ImGui::Begin(TR_ID(WindowId, Tr::PATTERNS), open, ImGuiWindowFlags_None);
    
    // Add button row with two buttons
    float buttonWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) / 2;
    if (ImGui::Button(TR(ADD_PATTERN), ImVec2(buttonWidth, 0.f))) {
        ImGui::OpenPopup("add_pattern_popup");
        newButtonName = "";
        newButtonPath = "";
    }
    ImGui::SameLine();
    if (ImGui::Button(TR(CREATE_FROM_SELECTION), ImVec2(buttonWidth, 0.f))) {
        // Create pattern from timeline selection or funscript selection
        bool hasTimelineSelection = app && app->scriptTimeline.selectionStart() >= 0.f;
        bool hasScriptSelection = app && app->ActiveFunscript() && app->ActiveFunscript()->HasSelection();
        
        if (hasTimelineSelection) {
            // Get selection from timeline
            float startTime = app->scriptTimeline.selectionStart();
            float endTime = app->player->CurrentTime();
            if (endTime < startTime) std::swap(startTime, endTime);
            
            auto selection = app->ActiveFunscript()->GetSelection(startTime, endTime);
            if (!selection.empty()) {
                PatternButton newButton;
                newButton.name = "Selection Pattern";
                newButton.isInlinePattern = true;
                newButton.inlineActions = selection;
                layout.buttons.push_back(newButton);
            }
        } else if (hasScriptSelection) {
            // Get selection from funscript
            auto selection = app->ActiveFunscript()->Selection();
            if (!selection.empty()) {
                PatternButton newButton;
                newButton.name = "Selection Pattern";
                newButton.isInlinePattern = true;
                newButton.inlineActions = selection;
                layout.buttons.push_back(newButton);
            }
        } else {
            ImGui::OpenPopup("no_selection_popup");
        }
    }
    
    // Handle no selection popup
    if (ImGui::BeginPopupModal("no_selection_popup", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text(TR(SELECTION_EMPTY));
        if (ImGui::Button(TR(DONE), ImVec2(120.f, 0.f))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    
    ImGui::Separator();
    
    // Pattern grid - fixed 3 columns
    int columns = FIXED_COLUMNS;
    ImGui::Columns(columns, "pattern_grid", false);
    
    for (int i = 0; i < GetPatternCount(); i++) {
        auto* pattern = GetPattern(i);
        if (!pattern) continue;
        
        bool fileExists = pattern->isInlinePattern || Util::FileExists(pattern->funscriptPath);
        
        // Button color based on file existence
        if (!fileExists) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.2f, 0.2f, 1.0f));
        }
        
        // Build button text
        std::string buttonText = pattern->name;
        
        if (ImGui::Button(buttonText.c_str(), ImVec2(-1.f, 0.f))) {
            if (pattern->isInlinePattern) {
                InsertInlinePattern(*pattern);
            } else {
                InsertPattern(*pattern);
            }
        }
        
        if (!fileExists) {
            ImGui::PopStyleColor();
        }
        
        // Right-click context menu
        if (ImGui::IsItemClicked(1)) {
            selectedButtonIndex = i;
            ImGui::OpenPopup("pattern_context_menu");
        }
        
        ImGui::NextColumn();
    }
    
    ImGui::Columns(1);
    
    ImGui::Separator();
    
    // Add Pattern Popup
    if (ImGui::BeginPopupModal("add_pattern_popup", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        static char nameBuffer[256] = "";
        
        ImGui::Text("Pattern Name:");
        ImGui::InputText("##pattern_name", nameBuffer, sizeof(nameBuffer));
        
        ImGui::Spacing();
        
        if (ImGui::Button(TR(BROWSE), ImVec2(120.f, 0.f))) {
            Util::OpenFileDialog(
                TR(SELECT_FUNSCRIPT), 
                "",
                [this](auto& result) {
                    if (!result.files.empty()) {
                        newButtonPath = result.files[0];
                        // Auto-fill name from filename if empty
                        if (newButtonName.empty()) {
                            auto filename = Util::Filename(newButtonPath);
                            auto lastDot = filename.rfind('.');
                            if (lastDot != std::string::npos) {
                                filename = filename.substr(0, lastDot);
                            }
                            newButtonName = filename;
                            strncpy(nameBuffer, filename.c_str(), sizeof(nameBuffer) - 1);
                        }
                    }
                },
                false,
                { "Funscript", "*.funscript" }
            );
        }
        
        ImGui::SameLine();
        
        if (!newButtonPath.empty()) {
            ImGui::Text("%s", Util::Filename(newButtonPath).c_str());
        }
        
        ImGui::Spacing();
        
        if (ImGui::Button(TR(SAVE), ImVec2(120.f, 0.f)) && !newButtonPath.empty()) {
            PatternButton newButton;
            newButton.name = nameBuffer;
            newButton.funscriptPath = newButtonPath;
            newButton.isInlinePattern = false;
            
            layout.buttons.push_back(newButton);
            
            newButtonName = "";
            newButtonPath = "";
            nameBuffer[0] = '\0';
            
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::SameLine();
        
        if (ImGui::Button(TR(CANCEL), ImVec2(120.f, 0.f))) {
            newButtonName = "";
            newButtonPath = "";
            nameBuffer[0] = '\0';
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
    
    // Context Menu Popup
    if (ImGui::BeginPopup("pattern_context_menu")) {
        if (selectedButtonIndex >= 0 && selectedButtonIndex < GetPatternCount()) {
            auto* pattern = GetPattern(selectedButtonIndex);
            if (pattern) {
                // Rename - use inline input
                static char renameBuffer[256] = "";
                strncpy(renameBuffer, pattern->name.c_str(), sizeof(renameBuffer) - 1);
                ImGui::Text("Rename:");
                if (ImGui::InputText("##rename", renameBuffer, sizeof(renameBuffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
                    if (strlen(renameBuffer) > 0) {
                        layout.buttons[selectedButtonIndex].name = renameBuffer;
                    }
                }
                ImGui::Separator();
                
                // Change funscript (only for file-based patterns)
                if (!pattern->isInlinePattern) {
                    if (ImGui::MenuItem(TR(CHANGE))) {
                        // Change funscript file
                        Util::OpenFileDialog(
                            TR(SELECT_FUNSCRIPT),
                            pattern->funscriptPath,
                            [this, idx = selectedButtonIndex](auto& result) {
                                if (!result.files.empty()) {
                                    layout.buttons[idx].funscriptPath = result.files[0];
                                }
                            },
                            false,
                            { "Funscript", "*.funscript" }
                        );
                    }
                }
                
                if (ImGui::MenuItem(TR(DELETE))) {
                    layout.buttons.erase(layout.buttons.begin() + selectedButtonIndex);
                }
            }
        }
        ImGui::EndPopup();
    }
    

    ImGui::End();
}
