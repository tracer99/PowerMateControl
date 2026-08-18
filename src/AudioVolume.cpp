/**
 * @file AudioVolume.cpp
 * @brief WASAPI endpoint activation, volume notify callback, and step/mute writes.
 */

#include "AudioVolume.h"
#include "LedController.h"

#include <windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <audioclient.h>
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
std::atomic<bool> g_needsReset{ false };           /**< Endpoint unusable; awaiting UI-thread Reset. */

/** @brief COM objects detached from the globals, owned by the caller. */
struct EndpointObjects {
    IMMDeviceEnumerator* enumerator = nullptr;
    IMMDevice* device = nullptr;
    IAudioEndpointVolume* endpointVolume = nullptr;
    VolumeNotifyCallback* callback = nullptr;
};

/**
 * @brief True for HRESULTs meaning the endpoint is dead and must be re-activated.
 * @note Typical after sleep/resume, a dock change, or a default-device switch.
 */
bool IsEndpointLost(HRESULT hr) {
    return hr == AUDCLNT_E_DEVICE_INVALIDATED ||
           hr == HRESULT_FROM_WIN32(ERROR_GEN_FAILURE) ||
           hr == HRESULT_FROM_WIN32(ERROR_DEVICE_NOT_CONNECTED) ||
           hr == RPC_E_DISCONNECTED ||
           hr == E_HANDLE;
}

/**
 * @brief Flags the endpoint for re-activation when a call failed because it died.
 * @return Always false, so callers can `return NoteFailure(hr);`.
 */
bool NoteFailure(HRESULT hr) {
    if (IsEndpointLost(hr)) {
        g_needsReset.store(true);
    }
    return false;
}

/** @brief Activates the endpoint and registers the notify sink; caller holds g_audioMutex. */
bool InitializeLocked() {
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

/**
 * @brief Clears the globals and hands ownership of the COM pointers to the caller.
 *
 * Releasing must happen outside g_audioMutex: UnregisterControlChangeNotify waits
 * for in-flight OnNotify calls, and OnNotify needs that same mutex to read volume.
 */
EndpointObjects DetachEndpoint() {
    std::lock_guard<std::mutex> lock(g_audioMutex);
    EndpointObjects owned{ g_enumerator, g_device, g_endpointVolume, g_callback };
    g_enumerator = nullptr;
    g_device = nullptr;
    g_endpointVolume = nullptr;
    g_callback = nullptr;
    g_initialized.store(false);
    return owned;
}

/** @brief Detaches then releases the endpoint objects. */
void ReleaseEndpoint() {
    EndpointObjects owned = DetachEndpoint();

    if (owned.endpointVolume && owned.callback) {
        owned.endpointVolume->UnregisterControlChangeNotify(owned.callback);
    }
    if (owned.callback) {
        owned.callback->Release();
    }
    if (owned.endpointVolume) {
        owned.endpointVolume->Release();
    }
    if (owned.device) {
        owned.device->Release();
    }
    if (owned.enumerator) {
        owned.enumerator->Release();
    }
}

}  // namespace

bool AudioVolume::Initialize() {
    std::lock_guard<std::mutex> lock(g_audioMutex);
    return InitializeLocked();
}

void AudioVolume::Shutdown() {
    ReleaseEndpoint();
    g_needsReset.store(false);
}

bool AudioVolume::Reset() {
    ReleaseEndpoint();

    std::lock_guard<std::mutex> lock(g_audioMutex);
    if (!InitializeLocked()) {
        return false;
    }

    g_needsReset.store(false);
    std::cerr << "[Debug] Audio endpoint re-activated\n";
    return true;
}

bool AudioVolume::NeedsReset() {
    return g_needsReset.load();
}

void AudioVolume::MarkForReset() {
    g_needsReset.store(true);
}

bool AudioVolume::GetState(float& scalar, bool& muted) {
    std::lock_guard<std::mutex> lock(g_audioMutex);
    if (!g_initialized.load() || !g_endpointVolume) {
        g_needsReset.store(true);
        return false;
    }

    BOOL muteFlag = FALSE;
    HRESULT hrMute = g_endpointVolume->GetMute(&muteFlag);
    if (FAILED(hrMute)) {
        return NoteFailure(hrMute);
    }

    HRESULT hrVol = g_endpointVolume->GetMasterVolumeLevelScalar(&scalar);
    if (FAILED(hrVol)) {
        return NoteFailure(hrVol);
    }

    muted = muteFlag != FALSE;
    return true;
}

bool AudioVolume::StepUp() {
    std::lock_guard<std::mutex> lock(g_audioMutex);
    if (!g_initialized.load() || !g_endpointVolume) {
        g_needsReset.store(true);
        return false;
    }

    HRESULT hr = g_endpointVolume->VolumeStepUp(nullptr);
    if (FAILED(hr)) {
        std::cerr << "[Error] VolumeStepUp failed: 0x" << std::hex << hr << std::dec << "\n";
        return NoteFailure(hr);
    }
    return true;
}

bool AudioVolume::StepDown() {
    std::lock_guard<std::mutex> lock(g_audioMutex);
    if (!g_initialized.load() || !g_endpointVolume) {
        g_needsReset.store(true);
        return false;
    }

    HRESULT hr = g_endpointVolume->VolumeStepDown(nullptr);
    if (FAILED(hr)) {
        std::cerr << "[Error] VolumeStepDown failed: 0x" << std::hex << hr << std::dec << "\n";
        return NoteFailure(hr);
    }
    return true;
}

bool AudioVolume::ToggleMute() {
    std::lock_guard<std::mutex> lock(g_audioMutex);
    if (!g_initialized.load() || !g_endpointVolume) {
        g_needsReset.store(true);
        return false;
    }

    BOOL muteFlag = FALSE;
    HRESULT hr = g_endpointVolume->GetMute(&muteFlag);
    if (FAILED(hr)) {
        std::cerr << "[Error] GetMute failed: 0x" << std::hex << hr << std::dec << "\n";
        return NoteFailure(hr);
    }

    hr = g_endpointVolume->SetMute(muteFlag ? FALSE : TRUE, nullptr);
    if (FAILED(hr)) {
        std::cerr << "[Error] SetMute failed: 0x" << std::hex << hr << std::dec << "\n";
        return NoteFailure(hr);
    }
    return true;
}
