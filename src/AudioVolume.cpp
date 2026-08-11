/**
 * @file AudioVolume.cpp
 * @brief WASAPI endpoint activation, volume notify callback, and step/mute writes.
 */

#include "AudioVolume.h"
#include "LedController.h"

#include <windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <iostream>
#include <atomic>
#include <mutex>

namespace {

/**
 * @brief IAudioEndpointVolumeCallback that refreshes the PowerMate LED on OS volume changes.
 * @note OnNotify must stay non-blocking; LED apply is queued/best-effort.
 */
class VolumeNotifyCallback : public IAudioEndpointVolumeCallback {
public:
    VolumeNotifyCallback() : refCount_(1) {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (ppv == nullptr) {
            return E_POINTER;
        }
        if (riid == IID_IUnknown || riid == __uuidof(IAudioEndpointVolumeCallback)) {
            *ppv = static_cast<IAudioEndpointVolumeCallback*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override {
        return InterlockedIncrement(&refCount_);
    }

    ULONG STDMETHODCALLTYPE Release() override {
        LONG count = InterlockedDecrement(&refCount_);
        if (count == 0) {
            delete this;
        }
        return static_cast<ULONG>(count);
    }

    HRESULT STDMETHODCALLTYPE OnNotify(PAUDIO_VOLUME_NOTIFICATION_DATA pNotify) override {
        UNREFERENCED_PARAMETER(pNotify);
        // Keep this non-blocking: LED refresh is best-effort.
        LedController::Refresh();
        return S_OK;
    }

private:
    LONG refCount_; /**< COM reference count. */
};

std::mutex g_audioMutex;                           /**< Guards endpoint COM pointers and init flag. */
IMMDeviceEnumerator* g_enumerator = nullptr;       /**< MMDevice enumerator. */
IMMDevice* g_device = nullptr;                     /**< Default multimedia render endpoint. */
IAudioEndpointVolume* g_endpointVolume = nullptr;  /**< Volume/mute interface for g_device. */
VolumeNotifyCallback* g_callback = nullptr;        /**< Registered control-change notify sink. */
std::atomic<bool> g_initialized{ false };          /**< True after successful Initialize. */

}  // namespace

bool AudioVolume::Initialize() {
    std::lock_guard<std::mutex> lock(g_audioMutex);
    if (g_initialized.load()) {
        return true;
    }

    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                  __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&g_enumerator));
    if (FAILED(hr) || !g_enumerator) {
        std::cerr << "[Error] MMDeviceEnumerator failed: 0x" << std::hex << hr << std::dec << "\n";
        return false;
    }

    hr = g_enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &g_device);
    if (FAILED(hr) || !g_device) {
        std::cerr << "[Error] GetDefaultAudioEndpoint failed: 0x" << std::hex << hr << std::dec << "\n";
        g_enumerator->Release();
        g_enumerator = nullptr;
        return false;
    }

    hr = g_device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr,
                           reinterpret_cast<void**>(&g_endpointVolume));
    if (FAILED(hr) || !g_endpointVolume) {
        std::cerr << "[Error] IAudioEndpointVolume activate failed: 0x" << std::hex << hr << std::dec << "\n";
        g_device->Release();
        g_device = nullptr;
        g_enumerator->Release();
        g_enumerator = nullptr;
        return false;
    }

    g_callback = new VolumeNotifyCallback();
    hr = g_endpointVolume->RegisterControlChangeNotify(g_callback);
    if (FAILED(hr)) {
        std::cerr << "[Error] RegisterControlChangeNotify failed: 0x" << std::hex << hr << std::dec << "\n";
        g_callback->Release();
        g_callback = nullptr;
        g_endpointVolume->Release();
        g_endpointVolume = nullptr;
        g_device->Release();
        g_device = nullptr;
        g_enumerator->Release();
        g_enumerator = nullptr;
        return false;
    }

    g_initialized.store(true);
    std::cerr << "[Debug] Audio volume monitoring initialized\n";
    return true;
}

void AudioVolume::Shutdown() {
    std::lock_guard<std::mutex> lock(g_audioMutex);
    if (!g_initialized.load()) {
        return;
    }

    if (g_endpointVolume && g_callback) {
        g_endpointVolume->UnregisterControlChangeNotify(g_callback);
    }
    if (g_callback) {
        g_callback->Release();
        g_callback = nullptr;
    }
    if (g_endpointVolume) {
        g_endpointVolume->Release();
        g_endpointVolume = nullptr;
    }
    if (g_device) {
        g_device->Release();
        g_device = nullptr;
    }
    if (g_enumerator) {
        g_enumerator->Release();
        g_enumerator = nullptr;
    }

    g_initialized.store(false);
}

bool AudioVolume::GetState(float& scalar, bool& muted) {
    std::lock_guard<std::mutex> lock(g_audioMutex);
    if (!g_initialized.load() || !g_endpointVolume) {
        return false;
    }

    BOOL muteFlag = FALSE;
    HRESULT hrMute = g_endpointVolume->GetMute(&muteFlag);
    HRESULT hrVol = g_endpointVolume->GetMasterVolumeLevelScalar(&scalar);
    if (FAILED(hrMute) || FAILED(hrVol)) {
        return false;
    }

    muted = muteFlag != FALSE;
    return true;
}

bool AudioVolume::StepUp() {
    std::lock_guard<std::mutex> lock(g_audioMutex);
    if (!g_initialized.load() || !g_endpointVolume) {
        return false;
    }

    HRESULT hr = g_endpointVolume->VolumeStepUp(nullptr);
    if (FAILED(hr)) {
        std::cerr << "[Error] VolumeStepUp failed: 0x" << std::hex << hr << std::dec << "\n";
        return false;
    }
    return true;
}

bool AudioVolume::StepDown() {
    std::lock_guard<std::mutex> lock(g_audioMutex);
    if (!g_initialized.load() || !g_endpointVolume) {
        return false;
    }

    HRESULT hr = g_endpointVolume->VolumeStepDown(nullptr);
    if (FAILED(hr)) {
        std::cerr << "[Error] VolumeStepDown failed: 0x" << std::hex << hr << std::dec << "\n";
        return false;
    }
    return true;
}

bool AudioVolume::ToggleMute() {
    std::lock_guard<std::mutex> lock(g_audioMutex);
    if (!g_initialized.load() || !g_endpointVolume) {
        return false;
    }

    BOOL muteFlag = FALSE;
    HRESULT hr = g_endpointVolume->GetMute(&muteFlag);
    if (FAILED(hr)) {
        std::cerr << "[Error] GetMute failed: 0x" << std::hex << hr << std::dec << "\n";
        return false;
    }

    hr = g_endpointVolume->SetMute(muteFlag ? FALSE : TRUE, nullptr);
    if (FAILED(hr)) {
        std::cerr << "[Error] SetMute failed: 0x" << std::hex << hr << std::dec << "\n";
        return false;
    }
    return true;
}
