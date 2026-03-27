#include "OFS_VoiceInput.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <filesystem>
#include <chrono>

#include "imgui.h"

#include "OFS_Util.h"
#include "OpenFunscripter.h"
#include "state/PreferenceState.h"

OFS_VoiceInput::OFS_VoiceInput() noexcept
{
}

OFS_VoiceInput::~OFS_VoiceInput() noexcept
{
    Shutdown();
}

bool OFS_VoiceInput::Init() noexcept
{
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
        return false;
    }

    auto modelPath = std::filesystem::path("data") / "ggml-tiny.en.bin";
    if (!std::filesystem::exists(modelPath)) {
        modelPath = std::filesystem::path("data") / "whisper" / "ggml-tiny.en.bin";
    }
    
    if (std::filesystem::exists(modelPath)) {
        ctx = whisper_init_from_file_with_params(modelPath.string().c_str(), whisper_context_default_params());
        if (ctx) {
            wparams = new whisper_full_params(whisper_full_default_params(WHISPER_SAMPLING_GREEDY));
        wparams->n_threads = 4;
        wparams->max_len = 40;
        wparams->n_max_text_ctx = 0;
        wparams->max_tokens = 32;
            wparams->language = "en";
            wparams->temperature = 0.0f;
            wparams->initial_prompt = "The user will say numbers from zero to one hundred. Numbers: zero one two three four five six seven eight nine ten eleven twelve thirteen fourteen fifteen sixteen seventeen eighteen nineteen twenty thirty forty fifty sixty seventy eighty ninety hundred. Also: oh as zero, fifty as 50, sixty as 60, seventy as 70, eighty as 80, ninety as 90.";
            
            modelLoaded.store(true);
            return true;
        }
    }

    LOG_ERROR("VoiceInput: Failed to load whisper model");
    return true;
}

void OFS_VoiceInput::Shutdown() noexcept
{
    StopCapture();

    if (recognitionThread.joinable()) {
        recognitionRunning.store(false);
        recognitionThread.join();
    }

    if (ctx) {
        whisper_free(ctx);
        ctx = nullptr;
    }

    if (wparams) {
        delete wparams;
        wparams = nullptr;
    }

    modelLoaded.store(false);
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

bool OFS_VoiceInput::StartCapture(int frequency) noexcept
{
    if (capturing.load()) {
        return true;
    }

    int numDevices = SDL_GetNumAudioDevices(1);
    if (numDevices == 0) {
        LOG_ERROR("VoiceInput: No capture devices found");
        return false;
    }

    LOGF_INFO("VoiceInput: Found %d capture devices", numDevices);

    SDL_AudioSpec desired;
    SDL_AudioSpec obtained;
    SDL_memset(&desired, 0, sizeof(desired));
    desired.freq = 16000;
    desired.format = AUDIO_S16LSB;
    desired.channels = 1;
    desired.samples = 4096;
    desired.callback = AudioCallback;
    desired.userdata = this;

    const char* deviceName = selectedDevice.empty() ? nullptr : selectedDevice.c_str();
    
    if (!deviceName || strlen(deviceName) == 0) {
        for (int i = 0; i < numDevices; i++) {
            const char* name = SDL_GetAudioDeviceName(i, 1);
            if (name && strstr(name, "Yeti")) {
                deviceName = name;
                LOGF_INFO("VoiceInput: Auto-selecting Yeti: %s", name);
                break;
            }
        }
    }
    
    captureDevice = SDL_OpenAudioDevice(deviceName, 1, &desired, &obtained, 0);

    if (captureDevice == 0) {
        LOGF_ERROR("VoiceInput: Failed to open audio device: %s", SDL_GetError());
        return false;
    }

    LOGF_INFO("VoiceInput: Using format: %d channels: %d samples: %d freq: %d", 
        obtained.format, obtained.channels, obtained.samples, obtained.freq);

    capturing.store(true);
    SDL_PauseAudioDevice(captureDevice, 0);

    if (!recognitionThread.joinable()) {
        recognitionRunning.store(true);
        recognitionThread = std::thread(&OFS_VoiceInput::RecognitionWorker, this);
    }

    return true;
}

void OFS_VoiceInput::StopCapture() noexcept
{
    if (!capturing.load()) {
        return;
    }

    capturing.store(false);

    if (captureDevice != 0) {
        SDL_CloseAudioDevice(captureDevice);
        captureDevice = 0;
    }
}

std::vector<std::string> OFS_VoiceInput::GetAvailableDevices() noexcept
{
    std::vector<std::string> devices;
    int numDevices = SDL_GetNumAudioDevices(1);
    
    for (int i = 0; i < numDevices; i++) {
        const char* name = SDL_GetAudioDeviceName(i, 1);
        if (name) {
            devices.push_back(name);
        }
    }
    
    return devices;
}

void OFS_VoiceInput::SetDevice(const std::string& deviceName) noexcept
{
    bool wasCapturing = capturing.load();
    if (wasCapturing) {
        StopCapture();
    }

    selectedDevice = deviceName;

    if (wasCapturing) {
        StartCapture();
    }
}

void OFS_VoiceInput::Update(float currentTime) noexcept
{
    lastProcessedTime = currentTime;
}

void OFS_VoiceInput::Show(bool* open) noexcept
{
    if (!ImGui::Begin("Voice Input", open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return;
    }

    // Get reference to preferences state for saving
    auto app = OpenFunscripter::ptr;
    if (app) {
        auto& prefState = PreferenceState::State(app->preferences->StateHandle());
        
        bool enabled = prefState.voiceInputEnabled;
        if (ImGui::Checkbox("Enable Voice Input", &enabled)) {
            prefState.voiceInputEnabled = enabled;
            SetEnabled(enabled);
        }
        OFS::Tooltip("Enable microphone input for voice commands");

        ImGui::Separator();

        // Use slider for delay control
        ImGui::Text("Delay (seconds)");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150);
        if (ImGui::SliderFloat("##delay", &prefState.voiceInputDelay, 0.0f, 10.0f, "%.1f")) {
            prefState.voiceInputDelay = Util::Clamp(prefState.voiceInputDelay, 0.0f, 10.0f);
            SetDelay(prefState.voiceInputDelay);
        }
        OFS::Tooltip("Time offset for when the action is placed");

        ImGui::Separator();

        // Status with color indicator
        bool isCapturing = capturing.load();
        bool isModelLoaded = modelLoaded.load();
        
        ImGui::Text("Status: ");
        ImGui::SameLine();
        if (isCapturing) {
            if (isModelLoaded) {
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Listening");
            } else {
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Loading model...");
            }
        } else {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Stopped");
        }

        ImGui::Separator();

        // Better formatted help text
        ImGui::Text("Voice Commands:");
        ImGui::BulletText("Say a number (0-100) to add a point");
        ImGui::BulletText("'fifty' = 50, 'seventy five' = 75");
        ImGui::BulletText("'twenty three' = 23, 'one hundred' = 100");
    }
    else {
        // Fallback if app not available
        bool enabled = IsEnabled();
        if (ImGui::Checkbox("Enable Voice Input", &enabled)) {
            SetEnabled(enabled);
        }

        ImGui::Separator();

        static float delay = GetDelay();
        ImGui::Text("Delay (seconds)");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150);
        ImGui::SliderFloat("##delay", &delay, 0.0f, 10.0f, "%.1f");
        SetDelay(delay);

        ImGui::Separator();

        ImGui::Text("Status: %s", capturing.load() ? (modelLoaded.load() ? "Listening" : "Loading model...") : "Stopped");

        ImGui::Separator();

        ImGui::Text("Say a number (0-100) to add a point at that position.");
        ImGui::Text("Examples: 'fifty', 'seventy five', 'twenty three'");
    }

    ImGui::End();
}

void SDLCALL OFS_VoiceInput::AudioCallback(void* userdata, Uint8* stream, int len) noexcept
{
    auto* self = static_cast<OFS_VoiceInput*>(userdata);
    int16_t* samples = reinterpret_cast<int16_t*>(stream);
    int sampleCount = len / sizeof(int16_t);
    
    if (sampleCount == 0) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(self->audioBufferMutex);
    
    for (int i = 0; i < sampleCount; i++) {
        self->audioBuffer.push_back(static_cast<float>(samples[i]) / 32768.0f);
    }
}

void OFS_VoiceInput::RecognitionWorker() noexcept
{
    const int minSamples = 16000; // Process 1 second of audio for better recognition
    
    while (recognitionRunning.load()) {
        if (!capturing.load() || !ctx || !wparams) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        std::vector<float> localBuffer;
        {
            std::lock_guard<std::mutex> lock(audioBufferMutex);
            if (audioBuffer.size() >= minSamples) {
            // Keep 50% overlap - balanced between response time and reducing duplicates
                int keepSamples = minSamples / 2;
                localBuffer.assign(audioBuffer.end() - minSamples, audioBuffer.end());
                if (audioBuffer.size() > minSamples) {
                    audioBuffer.erase(audioBuffer.begin(), audioBuffer.end() - keepSamples);
                } else {
                    audioBuffer.clear();
                }
            }
        }

        if (localBuffer.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        // Process with whisper - no additional delay for faster response
        if (whisper_full(ctx, *wparams, localBuffer.data(), localBuffer.size()) != 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        const int n_segments = whisper_full_n_segments(ctx);
        for (int i = 0; i < n_segments; ++i) {
            const char* text = whisper_full_get_segment_text(ctx, i);
            if (text && *text) {
                // Skip logging and processing for BLANK_AUDIO - it's just noise from Whisper
                bool isBlankAudio = (strstr(text, "BLANK") != nullptr || strstr(text, "blank") != nullptr);
                
                int32_t number = ParseNumberFromText(text);
                if (number >= 0 && number <= 100 && numberCallback) {
                    // Dedup: ignore if same number was recognized within last 1 second
                    auto now = std::chrono::steady_clock::now();
                    auto timeSinceLast = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastRecognitionTime).count();
                    if (lastRecognizedNumber == number && timeSinceLast < 1000) {
                        // Skip duplicate within 1 second - but still log it
                        if (!isBlankAudio) {
                            LOGF_INFO("VoiceInput: Transcribed: \"%s\", recognized: %d (deduped)", text, number);
                        }
                    } else {
                        lastRecognizedNumber = number;
                        lastRecognitionTime = now;
                        
                        if (!isBlankAudio) {
                            LOGF_INFO("VoiceInput: Transcribed: \"%s\", recognized: %d", text, number);
                        }
                        LOGF_INFO("VoiceInput: Recognized number: %d", number);
                        numberCallback(number, lastProcessedTime);
                    }
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

int32_t OFS_VoiceInput::ParseNumberFromText(const std::string& text) noexcept
{
    std::string lower = text;
    std::transform(lower.begin(), lower.end(), lower.begin(), 
        [](unsigned char c) { return std::tolower(c); });

    // Trim whitespace
    auto start = lower.find_first_not_of(" ");
    if (start == std::string::npos) return -1;
    auto end = lower.find_last_not_of(" ");
    lower = lower.substr(start, end - start + 1);

    // Also filter out BLANK_AUDIO which is common noise - check BEFORE other filters
    // The transcribed text is often " [BLANK_AUDIO]" so we need to check for it
    if (lower.find("blank") != std::string::npos || lower.find("speak") != std::string::npos) {
        return -1;
    }

    // Filter out noise/garbage - ignore text that is just punctuation, brackets, or very short noise
    if (lower.length() <= 1 && (lower == "[" || lower == "]" || lower == "-" || lower == "")) {
        return -1;
    }
    
    // Remove common noise characters and check if we still have meaningful content
    std::string filtered;
    for (char c : lower) {
        if (c == '[' || c == ']' || c == '-' || c == '.' || c == ',' || c == ' ') continue;
        filtered += c;
    }
    
    // If no meaningful characters left, ignore this input
    if (filtered.empty()) {
        return -1;
    }

    // First: Check if it's a direct number like "10", "50", "18"
    try {
        size_t pos;
        int num = std::stoi(filtered, &pos);
        if (pos == filtered.length() && num >= 0 && num <= 100) {
            return num;
        }
    } catch (...) {
        // Not a direct number, continue with word parsing
    }

    std::istringstream iss(lower);
    std::string word;
    std::vector<std::string> words;
    while (iss >> word) {
        words.push_back(word);
    }
    
    if (words.empty()) return -1;
    
    // Handle special case: "one hundred" or "a hundred"
    for (size_t i = 0; i < words.size(); i++) {
        if (i + 1 < words.size() && words[i] == "one" && words[i+1] == "hundred") {
            return 100;
        }
        if (words[i] == "hundred") {
            return 100;
        }
    }
    
    // First pass: look for teens (13-19)
    for (size_t i = 0; i < words.size(); i++) {
        const std::string& w = words[i];
        if (w == "thirteen") return 13;
        if (w == "fourteen") return 14;
        if (w == "fifteen") return 15;
        if (w == "sixteen") return 16;
        if (w == "seventeen") return 17;
        if (w == "eighteen") return 18;
        if (w == "nineteen") return 19;
    }
    
    // Second pass: look for tens (20, 30, 40, etc.)
    for (size_t i = 0; i < words.size(); i++) {
        const std::string& w = words[i];
        int tens = -1;
        
        if (w == "twenty") tens = 20;
        else if (w == "thirty") tens = 30;
        else if (w == "forty") tens = 40;
        else if (w == "fifty") tens = 50;
        else if (w == "sixty") tens = 60;
        else if (w == "seventy") tens = 70;
        else if (w == "eighty") tens = 80;
        else if (w == "ninety") tens = 90;
        
        if (tens > 0) {
            // Check if there's a ones digit after this
            if (i + 1 < words.size()) {
                const std::string& next = words[i + 1];
                int ones = -1;
                if (next == "one" || next == "a") ones = 1;
                else if (next == "two" || next == "to" || next == "too") ones = 2;
                else if (next == "three") ones = 3;
                else if (next == "four" || next == "for") ones = 4;
                else if (next == "five") ones = 5;
                else if (next == "six") ones = 6;
                else if (next == "seven") ones = 7;
                else if (next == "eight") ones = 8;
                else if (next == "nine") ones = 9;
                
                if (ones > 0) {
                    return tens + ones;
                }
            }
            return tens;
        }
    }
    
    // Third pass: look for single digits and special cases
    for (size_t i = 0; i < words.size(); i++) {
        const std::string& w = words[i];
        
        // Handle zero/oh
        if (w == "zero" || w == "oh") {
            return 0;
        }
        // Handle single digits
        if (w == "one" || w == "a") return 1;
        if (w == "two" || w == "to" || w == "too") return 2;
        if (w == "three") return 3;
        if (w == "four" || w == "for") return 4;
        if (w == "five") return 5;
        if (w == "six") return 6;
        if (w == "seven") return 7;
        if (w == "eight") return 8;
        if (w == "nine") return 9;
        // Handle ten
        if (w == "ten") return 10;
    }
    
    return -1;
}
