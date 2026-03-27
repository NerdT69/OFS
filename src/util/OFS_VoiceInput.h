#pragma once

#include <string>
#include <functional>
#include <vector>
#include <atomic>
#include <memory>
#include <thread>
#include <mutex>

#include "SDL.h"
#include "whisper.h"

class OFS_VoiceInput
{
public:
    using NumberCallback = std::function<void(int32_t position, float atTime)>;

    OFS_VoiceInput() noexcept;
    ~OFS_VoiceInput() noexcept;

    bool Init() noexcept;
    void Shutdown() noexcept;

    bool StartCapture(int frequency = 16000) noexcept;
    void StopCapture() noexcept;

    void SetNumberCallback(NumberCallback callback) noexcept { numberCallback = callback; }

    bool IsCapturing() const noexcept { return capturing.load(); }

    std::vector<std::string> GetAvailableDevices() noexcept;

    void SetDevice(const std::string& deviceName) noexcept;
    void SetEnabled(bool enabled) noexcept { isEnabled.store(enabled); }
    bool IsEnabled() const noexcept { return isEnabled.load(); }

    void SetDelay(float delay) noexcept { delaySeconds.store(delay); }
    float GetDelay() const noexcept { return delaySeconds.load(); }

    void Update(float currentTime) noexcept;
    void Show(bool* open) noexcept;

    bool IsModelLoaded() const noexcept { return modelLoaded.load(); }

private:
    static void SDLCALL AudioCallback(void* userdata, Uint8* stream, int len) noexcept;

    void ProcessAudioData(const uint8_t* data, int len) noexcept;
    int32_t ParseNumberFromText(const std::string& text) noexcept;
    void RecognitionWorker() noexcept;

    std::atomic<bool> capturing{false};
    std::atomic<bool> isEnabled{false};
    std::atomic<float> delaySeconds{2.0f};
    std::atomic<bool> modelLoaded{false};

    SDL_AudioDeviceID captureDevice = 0;
    std::string selectedDevice;

    NumberCallback numberCallback;

    std::vector<float> audioBuffer;
    std::mutex audioBufferMutex;

    std::thread recognitionThread;
    std::atomic<bool> recognitionRunning{false};

    struct whisper_context* ctx = nullptr;
    struct whisper_full_params* wparams = nullptr;

    float lastProcessedTime = 0.0f;
    int logCounter = 0;
    
    // Deduplication for duplicate number recognition
    int32_t lastRecognizedNumber = -1;
    std::chrono::steady_clock::time_point lastRecognitionTime;
};
