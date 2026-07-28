#include "AboutDialog.h"
#include "resource.h"
#include "version.h"

#include <shellapi.h>
#include <wincodec.h>
#include <cstdio>
#include <cstring>
#include <new>

#pragma comment(lib, "windowscodecs.lib")

namespace {

constexpr const wchar_t* kRepoUrl = L"https://github.com/tracer99/PowerMateControl";
constexpr const wchar_t* kIssuesUrl = L"https://github.com/tracer99/PowerMateControl/issues";
constexpr const wchar_t* kMaintainer = L"Maintainer: tracer99";

struct AboutState {
    HBITMAP logo = nullptr;
};

HBITMAP LoadPngResourceScaled(HINSTANCE hInst, int resourceId, UINT maxWidth, UINT maxHeight) {
    HRSRC hrsrc = FindResourceW(hInst, MAKEINTRESOURCEW(resourceId), RT_RCDATA);
    if (!hrsrc) {
        return nullptr;
    }

    HGLOBAL hglob = LoadResource(hInst, hrsrc);
    if (!hglob) {
        return nullptr;
    }

    void* data = LockResource(hglob);
    DWORD size = SizeofResource(hInst, hrsrc);
    if (!data || size == 0) {
        return nullptr;
    }

    HGLOBAL heap = GlobalAlloc(GMEM_MOVEABLE, size);
    if (!heap) {
        return nullptr;
    }

    void* heapPtr = GlobalLock(heap);
    if (!heapPtr) {
        GlobalFree(heap);
        return nullptr;
    }
    memcpy(heapPtr, data, size);
    GlobalUnlock(heap);

    IStream* stream = nullptr;
    if (FAILED(CreateStreamOnHGlobal(heap, TRUE, &stream)) || !stream) {
        GlobalFree(heap);
        return nullptr;
    }

    IWICImagingFactory* factory = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&factory));
    if (FAILED(hr) || !factory) {
        stream->Release();
        return nullptr;
    }

    IWICBitmapDecoder* decoder = nullptr;
    hr = factory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnLoad, &decoder);
    stream->Release();
    if (FAILED(hr) || !decoder) {
        factory->Release();
        return nullptr;
    }

    IWICBitmapFrameDecode* frame = nullptr;
    hr = decoder->GetFrame(0, &frame);
    decoder->Release();
    if (FAILED(hr) || !frame) {
        factory->Release();
        return nullptr;
    }

    UINT width = 0;
    UINT height = 0;
    frame->GetSize(&width, &height);

    UINT destW = width;
    UINT destH = height;
    if (width > maxWidth || height > maxHeight) {
        const double scaleW = static_cast<double>(maxWidth) / static_cast<double>(width);
        const double scaleH = static_cast<double>(maxHeight) / static_cast<double>(height);
        const double scale = scaleW < scaleH ? scaleW : scaleH;
        destW = static_cast<UINT>(width * scale);
        destH = static_cast<UINT>(height * scale);
        if (destW == 0) destW = 1;
        if (destH == 0) destH = 1;
    }

    IWICBitmapScaler* scaler = nullptr;
    hr = factory->CreateBitmapScaler(&scaler);
    if (FAILED(hr) || !scaler) {
        frame->Release();
        factory->Release();
        return nullptr;
    }

    hr = scaler->Initialize(frame, destW, destH, WICBitmapInterpolationModeFant);
    frame->Release();
    if (FAILED(hr)) {
        scaler->Release();
        factory->Release();
        return nullptr;
    }

    IWICFormatConverter* converter = nullptr;
    hr = factory->CreateFormatConverter(&converter);
    if (FAILED(hr) || !converter) {
        scaler->Release();
        factory->Release();
        return nullptr;
    }

    hr = converter->Initialize(scaler, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone,
                               nullptr, 0.0, WICBitmapPaletteTypeCustom);
    scaler->Release();
    if (FAILED(hr)) {
        converter->Release();
        factory->Release();
        return nullptr;
    }

    const UINT stride = destW * 4;
    const UINT bufferSize = stride * destH;
    BYTE* pixels = new (std::nothrow) BYTE[bufferSize];
    if (!pixels) {
        converter->Release();
        factory->Release();
        return nullptr;
    }

    hr = converter->CopyPixels(nullptr, stride, bufferSize, pixels);
    converter->Release();
    factory->Release();
    if (FAILED(hr)) {
        delete[] pixels;
        return nullptr;
    }

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = static_cast<LONG>(destW);
    bmi.bmiHeader.biHeight = -static_cast<LONG>(destH); // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HDC hdc = GetDC(nullptr);
    HBITMAP hbmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    ReleaseDC(nullptr, hdc);

    if (hbmp && bits) {
        memcpy(bits, pixels, bufferSize);
    } else if (hbmp) {
        DeleteObject(hbmp);
        hbmp = nullptr;
    }

    delete[] pixels;
    return hbmp;
}

void OpenUrl(const wchar_t* url) {
    ShellExecuteW(nullptr, L"open", url, nullptr, nullptr, SW_SHOWNORMAL);
}

INT_PTR CALLBACK AboutDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG: {
            auto* state = new AboutState();
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));

            HINSTANCE hInst = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(hwnd, GWLP_HINSTANCE));
            state->logo = LoadPngResourceScaled(hInst, IDB_LOGO, 96, 96);
            if (state->logo) {
                SendDlgItemMessageW(hwnd, IDC_ABOUT_LOGO, STM_SETIMAGE,
                                    IMAGE_BITMAP, reinterpret_cast<LPARAM>(state->logo));
            }

            wchar_t version[64] = {};
            swprintf_s(version, L"Version %hs", PMC_VERSION_STRING);
            SetDlgItemTextW(hwnd, IDC_ABOUT_VERSION, version);
            SetDlgItemTextW(hwnd, IDC_ABOUT_MAINTAINER, kMaintainer);
            return TRUE;
        }

        case WM_COMMAND: {
            switch (LOWORD(wParam)) {
                case IDC_ABOUT_GITHUB:
                    OpenUrl(kRepoUrl);
                    return TRUE;
                case IDC_ABOUT_ISSUES:
                    OpenUrl(kIssuesUrl);
                    return TRUE;
                case IDOK:
                case IDCANCEL:
                    EndDialog(hwnd, LOWORD(wParam));
                    return TRUE;
            }
            break;
        }

        case WM_DESTROY: {
            auto* state = reinterpret_cast<AboutState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            if (state) {
                if (state->logo) {
                    DeleteObject(state->logo);
                }
                delete state;
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
            }
            break;
        }
    }
    return FALSE;
}

}  // namespace

void AboutDialog::Show(HWND owner) {
    HINSTANCE hInst = GetModuleHandleW(nullptr);
    DialogBoxParamW(hInst, MAKEINTRESOURCEW(IDD_ABOUT), owner, AboutDlgProc, 0);
}
