#include <windows.h>
#include <vector>
#include <iostream>
#include <string.h>
#include <chrono>
#include <numeric>
#include <thread>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <conio.h>



BITMAPFILEHEADER            g_bfHeader;
BITMAPINFOHEADER            g_biHeader;
DWORD                       g_cbBits;
std::vector<BYTE *>         p_rgbs;
BOOL                        run = 1;


BYTE *YUVFrame(BYTE* rgb) {
    const size_t image_size = g_biHeader.biWidth * abs(g_biHeader.biHeight);
    size_t upos = image_size;
    size_t vpos = upos + upos / 4;
    BYTE* p_YUVFrame = NULL;
    p_YUVFrame = new BYTE[image_size*3/2];

    for (size_t i = 0; i < image_size; ++i) {
        BYTE r = rgb[3 * i + 2];
        BYTE g = rgb[3 * i + 1];
        BYTE b = rgb[3 * i];
        p_YUVFrame[i] = ((66 * r + 129 * g + 25 * b) >> 8) + 16;
        if (!((i / g_biHeader.biWidth) % 2) && !(i % 2)) {
            p_YUVFrame[upos++] = ((-38 * r + -74 * g + 112 * b) >> 8) + 128;
            p_YUVFrame[vpos++] = ((112 * r + -94 * g + -18 * b) >> 8) + 128;
        }
    }
    return p_YUVFrame;
}

int render_rgb_to_yuv() {
    BYTE* p_RGBData, * p_YUVFrame;
    DWORD frameSize;

    std::string out_file = "out_";
    auto now = std::chrono::system_clock::now();
    auto UTC = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream datetime;
    datetime << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %X");
    out_file += datetime.str();
    std::replace(out_file.begin(), out_file.end(), ':', '-');
    std::replace(out_file.begin(), out_file.end(), ':', '-');
    out_file += ".yuv";

    FILE* out; 
    if (!(out = fopen(out_file.c_str(), "wb"))){
        std::cerr << "\nError: Unable to open file " + out_file << std::endl;
        return 1;
    }
    std::cout << "Render " << std::endl;
    frameSize = (DWORD)(g_biHeader.biWidth * abs(g_biHeader.biHeight) * 1.5);
    for (;;) {
        if (p_rgbs.size() == 0 && !run)
            break;
        else if (p_rgbs.size() == 0 && run)
            continue;
        p_YUVFrame = YUVFrame(p_rgbs[0]);
        fwrite(p_YUVFrame, sizeof(BYTE), frameSize, out);
        delete(p_YUVFrame);
        delete(p_rgbs[0]);
        p_rgbs.erase(p_rgbs.begin());
        p_YUVFrame = NULL;
    }
    std::cout << "Render Finish" << std::endl;
    fclose(out);
    return 0;
}

void WINAPI SaveBitmap() {
    BITMAPFILEHEADER bfHeader;
    BITMAPINFOHEADER biHeader;
    BITMAPINFO bInfo;
    HGDIOBJ hTempBitmap;
    HBITMAP hBitmap = NULL;
    BITMAP bAllDesktops;
    HDC hDC, hMemDC = NULL;
    LONG lWidth, lHeight;
    BYTE* bBits = NULL;
    HANDLE hHeap = GetProcessHeap();
    DWORD cbBits, dwWritten = 0;
    HANDLE hFile;
    INT x = GetSystemMetrics(SM_XVIRTUALSCREEN);
    INT y = GetSystemMetrics(SM_YVIRTUALSCREEN);

    ZeroMemory(&bfHeader, sizeof(BITMAPFILEHEADER));
    ZeroMemory(&biHeader, sizeof(BITMAPINFOHEADER));
    ZeroMemory(&bInfo, sizeof(BITMAPINFO));
    ZeroMemory(&bAllDesktops, sizeof(BITMAP));

    hDC = GetDC(NULL);
    hTempBitmap = GetCurrentObject(hDC, OBJ_BITMAP);
    GetObjectW(hTempBitmap, sizeof(BITMAP), &bAllDesktops);

    lWidth = bAllDesktops.bmWidth;
    lHeight = bAllDesktops.bmHeight;

    DeleteObject(hTempBitmap);

    bfHeader.bfType = 0;
    bfHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    biHeader.biSize = sizeof(BITMAPINFOHEADER);
    biHeader.biBitCount = 24;
    biHeader.biCompression = BI_RGB;
    biHeader.biPlanes = 1;
    biHeader.biWidth = lWidth;
    biHeader.biHeight = -lHeight;

    bInfo.bmiHeader = biHeader;

    g_biHeader = biHeader;
    g_bfHeader = bfHeader;

    cbBits = (((24 * lWidth + 31) & ~31) / 8) * lHeight;
    g_cbBits = cbBits;

    std::thread thr2(render_rgb_to_yuv);
    for (;;) {
        if (!run)
            break;
        auto start = std::chrono::high_resolution_clock::now();
        hMemDC = CreateCompatibleDC(hDC);
        hBitmap = CreateDIBSection(hDC, &bInfo, DIB_RGB_COLORS, (VOID**)&bBits, NULL, 0);
        SelectObject(hMemDC, hBitmap);
        BitBlt(hMemDC, 0, 0, lWidth, lHeight, hDC, x, y, SRCCOPY);
        if (bBits != NULL) {
            BYTE* new_elm = new BYTE[g_cbBits];
            memcpy(new_elm, bBits, g_cbBits);
            p_rgbs.push_back(new_elm);
        }
        DeleteDC(hMemDC);
        DeleteObject(hBitmap);
        auto finish = std::chrono::high_resolution_clock::now();
        auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(finish - start).count();
        if (delta < 40) {
            Sleep(40 - delta);
        }
    }
    thr2.join();
    ReleaseDC(NULL, hDC);
}

void key_press() {
    char button;
    std::cout << "Record start. Press ESC to exit." << std::endl;
    for (;;)
    {
        if (_getch() == 27)
        {
            run = 0;
            std::cout << "Record stop. Wait rendering." << std::endl;
            break;
        }
    }
}

int main() {
    std::thread thr1(key_press);
    thr1.detach();
    SaveBitmap();

    std::cout << "size: " << g_biHeader.biWidth << "x" << abs(g_biHeader.biHeight) << std::endl;
    return 0;
}
