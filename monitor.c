#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0601
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <pdh.h>
#include <strsafe.h>
#include <stdint.h>

#define WIDTH      252
#define HEIGHT     84
#define PAD_X      8
#define PAD_Y      5
#define LINE_H     23
#define BAR_H      9
#define BAR_X      76
#define BAR_W      158
#define LABEL_W    44
#define FONT_SZ    11
#define TIMER_ID   1
#define REFRESH_MS 1000
#define PDH_CSTATUS_VALID_DATA 0L
#define PDH_CSTATUS_NEW_DATA   1L
#define WM_TASKBAR (WM_APP + 1)
#define WM_TRAY    (WM_APP + 2)
#define ID_LOCK    100
#define ID_EXIT    101

static HINSTANCE      g_hInst;
static HWND           g_hWnd;
static int            g_locked;
static int            g_running = 1;
static NOTIFYICONDATAW g_nid;
static UINT           g_taskbarCreated;
static float          g_cpu;
static float          g_ramPct;
static DWORD          g_ramUsed, g_ramTotal;
static float          g_gpu;
static HICON          g_hIcon;
static int            g_iconCustom;

static HQUERY   g_hCPUQ;
static HCOUNTER g_hCPUctr;
static int      g_cpuOK;
static int      g_cpuBaseline;

static void CPUInit(void)
{
    g_cpuOK = g_cpuBaseline = 0;
    if (PdhOpenQueryW(NULL, 0, &g_hCPUQ) != ERROR_SUCCESS) return;
    if (PdhAddEnglishCounterW(g_hCPUQ,
            L"\\Processor Information(_Total)\\% Processor Utility",
            0, &g_hCPUctr) != ERROR_SUCCESS)
        { PdhCloseQuery(g_hCPUQ); return; }
    if (PdhCollectQueryData(g_hCPUQ) != ERROR_SUCCESS)
        { PdhCloseQuery(g_hCPUQ); return; }
    g_cpuOK = 1;
}

static float GetCPU(void)
{
    if (!g_cpuOK) return 0;
    if (PdhCollectQueryData(g_hCPUQ) != ERROR_SUCCESS) return g_cpu;
    if (!g_cpuBaseline) { g_cpuBaseline = 1; return g_cpu; }
    PDH_FMT_COUNTERVALUE v;
    if (PdhGetFormattedCounterValue(g_hCPUctr, PDH_FMT_DOUBLE | PDH_FMT_NOCAP100,
            NULL, &v) != ERROR_SUCCESS) return g_cpu;
    if (v.CStatus != PDH_CSTATUS_VALID_DATA && v.CStatus != PDH_CSTATUS_NEW_DATA) return g_cpu;
    g_cpu = (float)v.doubleValue;
    if (g_cpu > 100.0f) g_cpu = 100.0f;
    return g_cpu;
}

static void CPUUninit(void)
{
    if (g_cpuOK) { PdhCloseQuery(g_hCPUQ); g_cpuOK = 0; }
}

static float GetRAM(void)
{
    MEMORYSTATUSEX m = { sizeof(m) };
    GlobalMemoryStatusEx(&m);
    g_ramTotal = (DWORD)(m.ullTotalPhys >> 20);
    g_ramUsed  = (DWORD)((m.ullTotalPhys - m.ullAvailPhys) >> 20);
    g_ramPct = (float)((double)(m.ullTotalPhys - m.ullAvailPhys) * 100.0 / (double)m.ullTotalPhys);
    return g_ramPct;
}

static HQUERY   g_hGPUQ;
static HCOUNTER *g_hGPUctrs;
static LPWSTR   *g_gpuPaths;
static ULONGLONG *g_gpuPrev;
static DWORD    g_gpuCount;
static int      g_gpuOK;
static ULONGLONG g_gpuLastTick;
static ULONGLONG g_gpuReenumTick;

#ifdef _DEBUG
static void GPUWriteDebug(const WCHAR *msg)
{
    HANDLE h = CreateFileW(L"gpu_debug.txt", GENERIC_WRITE, 0, NULL,
                           OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h != INVALID_HANDLE_VALUE) {
        SetFilePointer(h, 0, NULL, FILE_END);
        DWORD n;
        WriteFile(h, msg, (DWORD)(lstrlenW(msg) * sizeof(WCHAR)), &n, NULL);
        CloseHandle(h);
    }
}
#endif

static int GPUReenum(void)
{
    // save old prev values keyed by path
#ifdef _DEBUG
    GPUWriteDebug(L"  reenum\n");
#endif
    DWORD oldCount = g_gpuCount;
    LPWSTR *oldPaths = g_gpuPaths;
    ULONGLONG *oldPrev = g_gpuPrev;
    HCOUNTER *oldCtrs = g_hGPUctrs;
    HQUERY oldQ = g_hGPUQ;
    g_gpuCount = 0; g_gpuPaths = NULL; g_gpuPrev = NULL; g_hGPUctrs = NULL; g_hGPUQ = NULL;

    if (PdhOpenQueryW(NULL, 0, &g_hGPUQ) != ERROR_SUCCESS) goto fail_reenum;

    DWORD cchPaths = 0;
    PdhExpandWildCardPathW(NULL, L"\\GPU Engine(*)\\Running Time", NULL, &cchPaths, 0);
    if (cchPaths == 0) { PdhCloseQuery(g_hGPUQ); g_hGPUQ = NULL; goto fail_reenum; }

    LPWSTR msz = (LPWSTR)HeapAlloc(GetProcessHeap(), 0, cchPaths * sizeof(WCHAR));
    if (!msz) { PdhCloseQuery(g_hGPUQ); g_hGPUQ = NULL; goto fail_reenum; }
    if (PdhExpandWildCardPathW(NULL, L"\\GPU Engine(*)\\Running Time", msz, &cchPaths, 0) != ERROR_SUCCESS) {
        HeapFree(GetProcessHeap(), 0, msz); PdhCloseQuery(g_hGPUQ); g_hGPUQ = NULL; goto fail_reenum;
    }

    DWORD cnt = 0;
    LPWSTR pp = msz;
    while (*pp) { cnt++; pp += lstrlenW(pp) + 1; }
    if (cnt == 0) { HeapFree(GetProcessHeap(), 0, msz); PdhCloseQuery(g_hGPUQ); g_hGPUQ = NULL; goto fail_reenum; }

    g_gpuPaths = (LPWSTR*)HeapAlloc(GetProcessHeap(), 0, cnt * sizeof(LPWSTR));
    g_hGPUctrs = (HCOUNTER*)HeapAlloc(GetProcessHeap(), 0, cnt * sizeof(HCOUNTER));
    g_gpuPrev = (ULONGLONG*)HeapAlloc(GetProcessHeap(), 0, cnt * sizeof(ULONGLONG));
    if (!g_gpuPaths || !g_hGPUctrs || !g_gpuPrev) {
        if (g_gpuPaths) HeapFree(GetProcessHeap(), 0, g_gpuPaths);
        if (g_hGPUctrs) HeapFree(GetProcessHeap(), 0, g_hGPUctrs);
        if (g_gpuPrev) HeapFree(GetProcessHeap(), 0, g_gpuPrev);
        g_gpuPaths = NULL; g_hGPUctrs = NULL; g_gpuPrev = NULL;
        HeapFree(GetProcessHeap(), 0, msz); PdhCloseQuery(g_hGPUQ); g_hGPUQ = NULL; goto fail_reenum;
    }

    DWORD idx = 0;
    pp = msz;
    while (*pp && idx < cnt) {
        g_gpuPaths[idx] = pp;
        g_gpuPrev[idx] = 0;
        // try to carry over old prev value
        for (DWORD oi = 0; oi < oldCount; oi++)
            if (oldPaths && oldPaths[oi] && wcscmp(pp, oldPaths[oi]) == 0) {
                g_gpuPrev[idx] = oldPrev[oi]; break;
            }
        if (PdhAddEnglishCounterW(g_hGPUQ, pp, 0, &g_hGPUctrs[idx]) != ERROR_SUCCESS)
            g_hGPUctrs[idx] = NULL;
        idx++; pp += lstrlenW(pp) + 1;
    }
    g_gpuCount = idx;

    // clean old query
    if (oldQ) { PdhCloseQuery(oldQ); }
    if (oldCtrs) HeapFree(GetProcessHeap(), 0, oldCtrs);
    if (oldPaths) HeapFree(GetProcessHeap(), 0, oldPaths);
    if (oldPrev) HeapFree(GetProcessHeap(), 0, oldPrev);

    PdhCollectQueryData(g_hGPUQ);
    return 1;

fail_reenum:
    // restore old state
    if (g_hGPUQ) { PdhCloseQuery(g_hGPUQ); g_hGPUQ = NULL; }
    g_hGPUQ = oldQ; g_hGPUctrs = oldCtrs; g_gpuPaths = oldPaths; g_gpuPrev = oldPrev; g_gpuCount = oldCount;
    return 0;
}

static void GPUInit(void)
{
    g_gpuOK = 0; g_hGPUQ = NULL; g_hGPUctrs = NULL; g_gpuPaths = NULL; g_gpuPrev = NULL;
    g_gpuCount = 0; g_gpuLastTick = 0; g_gpuReenumTick = GetTickCount64();
#ifdef _DEBUG
    DeleteFileW(L"gpu_debug.txt");
    GPUWriteDebug(L"GPUInit start\n");
#endif
    if (GPUReenum()) g_gpuOK = 1;
}

static void GPUUninit(void)
{
    if (g_hGPUQ) { PdhCloseQuery(g_hGPUQ); g_hGPUQ = NULL; }
    if (g_hGPUctrs) { HeapFree(GetProcessHeap(), 0, g_hGPUctrs); g_hGPUctrs = NULL; }
    if (g_gpuPaths) { HeapFree(GetProcessHeap(), 0, g_gpuPaths); g_gpuPaths = NULL; }
    if (g_gpuPrev) { HeapFree(GetProcessHeap(), 0, g_gpuPrev); g_gpuPrev = NULL; }
    g_gpuCount = 0; g_gpuOK = 0;
}

static float GetGPU(void)
{
    ULONGLONG now = GetTickCount64();

    // re-enum every 5s to pick up new processes (Furmark lanzado después)
    if (g_gpuReenumTick == 0 || now - g_gpuReenumTick > 5000) {
        g_gpuReenumTick = now;
        if (GPUReenum()) g_gpuOK = 1;
    }

    if (!g_gpuOK || g_gpuCount == 0) return -1;
    if (PdhCollectQueryData(g_hGPUQ) != ERROR_SUCCESS) return g_gpu;

    if (g_gpuLastTick == 0) { g_gpuLastTick = now; return g_gpu; }
    ULONGLONG elapsed = now - g_gpuLastTick;
    g_gpuLastTick = now;
    if (elapsed == 0) return g_gpu;

    float max3D = 0, maxAll = 0;
#ifdef _DEBUG
    static int dbgOnce = 0;
    WCHAR gdbg[512]; int gpos = 0;
#endif

    for (DWORD i = 0; i < g_gpuCount; i++) {
        if (!g_hGPUctrs[i]) continue;
        PDH_FMT_COUNTERVALUE v;
        if (PdhGetFormattedCounterValue(g_hGPUctrs[i], PDH_FMT_LARGE | PDH_FMT_NOSCALE, NULL, &v) != ERROR_SUCCESS) continue;
        if (v.CStatus != PDH_CSTATUS_VALID_DATA && v.CStatus != PDH_CSTATUS_NEW_DATA) continue;
        ULONGLONG val = v.largeValue;
        if (g_gpuPrev[i] != 0 && val >= g_gpuPrev[i]) {
            ULONGLONG delta = val - g_gpuPrev[i];
            float util = (float)((double)delta / (double)(elapsed * 100));
            if (util > maxAll) maxAll = util;
            if (wcsstr(g_gpuPaths[i], L"engtype_3D") && util > max3D) max3D = util;
        }
        g_gpuPrev[i] = val;
    }

#ifdef _DEBUG
    if (!dbgOnce) {
        gpos = StringCchPrintfW(gdbg, 512, L"GetGPU: %lu engs max3D=%.0f%% maxAll=%.0f%% elapsed=%llu\n",
            g_gpuCount, max3D, maxAll, elapsed);
        for (DWORD i = 0; i < g_gpuCount && i < 3; i++)
            gpos += StringCchPrintfW(gdbg + gpos, 512 - gpos, L"  [%lu] %llu\n", i, g_gpuPrev[i]);
        GPUWriteDebug(gdbg);
        dbgOnce = 1;
    }
#endif

    float r = max3D > 0 ? max3D : maxAll;
    if (r > 100) r = 100;
    return r;
}

static COLORREF ColorPct(float p)
{
    if (p < 50) return RGB(0, 210, 80);
    if (p < 80) return RGB(210, 210, 0);
    return RGB(220, 40, 40);
}

static void DrawBar(HDC dc, int x, int y, int w, int h, float p)
{
    RECT r = { x, y, x + w, y + h };
    HBRUSH bg = CreateSolidBrush(RGB(50, 50, 65));
    FillRect(dc, &r, bg);
    DeleteObject(bg);
    if (p > 0.5f) {
        int fw = (int)(w * p / 100.0f);
        if (fw < 2) fw = 2;
        HBRUSH fg = CreateSolidBrush(ColorPct(p));
        RECT fr = { x + 1, y + 1, x + fw, y + h - 1 };
        FillRect(dc, &fr, fg);
        DeleteObject(fg);
    }
}

static void UpdateTrayTip(void)
{
    WCHAR tip[128];
    if (g_gpuOK && g_gpu >= 0)
        StringCchPrintfW(tip, 128, L"CPU %3.0f%% | RAM %lu/%lu GB (%3.0f%%) | GPU %3.0f%%",
                         g_cpu, g_ramUsed, g_ramTotal, g_ramPct, g_gpu);
    else
        StringCchPrintfW(tip, 128, L"CPU %3.0f%% | RAM %lu/%lu GB (%3.0f%%) | GPU N/A",
                         g_cpu, g_ramUsed, g_ramTotal, g_ramPct);
    StringCchCopyW(g_nid.szTip, 128, tip);
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

static void UpdateData(void)
{
    g_cpu = GetCPU();
    GetRAM();
    if (g_gpuOK) g_gpu = GetGPU();
    if (g_nid.hWnd) UpdateTrayTip();
    InvalidateRect(g_hWnd, NULL, FALSE);
}

static void Render(HWND hWnd)
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hWnd, &ps);
    RECT cr; GetClientRect(hWnd, &cr);
    HDC mdc = CreateCompatibleDC(hdc);
    HBITMAP bmp = CreateCompatibleBitmap(hdc, cr.right, cr.bottom);
    HGDIOBJ ob = SelectObject(mdc, bmp);

    HBRUSH bg = CreateSolidBrush(RGB(20, 20, 30));
    FillRect(mdc, &cr, bg);
    DeleteObject(bg);

    HFONT fnt = CreateFontW(-FONT_SZ, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                            PROOF_QUALITY, FIXED_PITCH | FF_DONTCARE, L"Consolas");
    HGDIOBJ of = SelectObject(mdc, fnt);
    SetBkMode(mdc, TRANSPARENT);

    HBRUSH brd = CreateSolidBrush(RGB(60, 60, 80));
    FrameRect(mdc, &cr, brd);
    DeleteObject(brd);

    WCHAR buf[64];
    float vals[3] = { g_cpu, g_ramPct, g_gpu };
    const WCHAR *lbl[3] = { L"CPU", L"RAM", L"GPU" };
    int y;
    for (int i = 0; i < 3; i++) {
        y = PAD_Y + i * LINE_H;
        SetTextColor(mdc, RGB(200, 200, 225));
        TextOutW(mdc, PAD_X, y + 3, lbl[i], 3);
        if (i == 2 && (!g_gpuOK || vals[i] < 0)) {
            SetTextColor(mdc, RGB(130, 130, 140));
            TextOutW(mdc, LABEL_W, y + 3, L"N/A", 3);
            continue;
        }
        StringCchPrintfW(buf, 64, L"%3.0f%%", vals[i]);
        SetTextColor(mdc, ColorPct(vals[i]));
        TextOutW(mdc, LABEL_W, y + 3, buf, 5);
        DrawBar(mdc, BAR_X, y + (LINE_H - BAR_H) / 2, BAR_W, BAR_H, vals[i]);
    }

    HFONT fSm = CreateFontW(-7, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                            PROOF_QUALITY, FIXED_PITCH | FF_DONTCARE, L"Consolas");
    HGDIOBJ ofSm = SelectObject(mdc, fSm);
    SetTextColor(mdc, RGB(80, 80, 100));
    RECT rc = cr;
    rc.top = cr.bottom - 11;
    DrawTextW(mdc, L"Monitor by RoanDev", -1, &rc, DT_CENTER | DT_BOTTOM | DT_SINGLELINE);
    SelectObject(mdc, ofSm);
    DeleteObject(fSm);

    BitBlt(hdc, 0, 0, cr.right, cr.bottom, mdc, 0, 0, SRCCOPY);
    SelectObject(mdc, ob); SelectObject(mdc, of);
    DeleteObject(fnt); DeleteObject(bmp); DeleteDC(mdc);
    EndPaint(hWnd, &ps);
}

static void ShowMenu(HWND hWnd, int x, int y)
{
    HMENU m = CreatePopupMenu();
    AppendMenuW(m, MF_STRING | (g_locked ? MF_CHECKED : 0), ID_LOCK,
                g_locked ? L"Desbloquear (click-through)" : L"Fijar (click-through)");
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_STRING, ID_EXIT, L"Salir");
    SetForegroundWindow(hWnd);
    TrackPopupMenu(m, TPM_LEFTALIGN | TPM_BOTTOMALIGN, x, y, 0, hWnd, NULL);
    DestroyMenu(m);
}

static void ToggleLock(void)
{
    g_locked = !g_locked;
    LONG ex = GetWindowLongW(g_hWnd, GWL_EXSTYLE);
    SetWindowLongW(g_hWnd, GWL_EXSTYLE,
                   g_locked ? (ex | WS_EX_TRANSPARENT) : (ex & ~WS_EX_TRANSPARENT));
}

static void TrayAdd(HWND hWnd, HICON hIcon)
{
    ZeroMemory(&g_nid, sizeof(g_nid));
    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = hWnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAY;
    g_nid.hIcon = hIcon;
    StringCchCopyW(g_nid.szTip, 128, L"Monitor CPU/RAM/GPU (clic der.)");
    Shell_NotifyIconW(NIM_ADD, &g_nid);
    g_nid.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &g_nid);
}

static void TrayRemove(void)
{
    Shell_NotifyIconW(NIM_DELETE, &g_nid);
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_CREATE:
        SetTimer(hWnd, TIMER_ID, REFRESH_MS, NULL);
        return 0;

    case WM_TIMER:
        UpdateData();
        return 0;

    case WM_PAINT:
        Render(hWnd);
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_LBUTTONDOWN:
        if (!g_locked) {
            ReleaseCapture();
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ClientToScreen(hWnd, &pt);
            SendMessage(hWnd, WM_NCLBUTTONDOWN, HTCAPTION, MAKELPARAM(pt.x, pt.y));
        }
        return 0;

    case WM_RBUTTONUP:
        {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ClientToScreen(hWnd, &pt);
            ShowMenu(hWnd, pt.x, pt.y);
        }
        return 0;

    case WM_TRAY:
        if (lParam == WM_RBUTTONUP) {
            POINT pt;
            GetCursorPos(&pt);
            ShowMenu(hWnd, pt.x, pt.y);
        } else if (lParam == WM_LBUTTONDBLCLK) {
            ToggleLock();
        }
        return 0;

    case WM_COMMAND:
        if (LOWORD(wParam) == ID_LOCK) ToggleLock();
        else if (LOWORD(wParam) == ID_EXIT) {
            g_running = 0;
            DestroyWindow(hWnd);
        }
        return 0;

    case WM_DESTROY:
        KillTimer(hWnd, TIMER_ID);
        TrayRemove();
        if (g_iconCustom) DestroyIcon(g_hIcon);
        CPUUninit();
        GPUUninit();
        PostQuitMessage(0);
        return 0;

    case WM_TASKBAR:
        TrayRemove();
        TrayAdd(hWnd, g_nid.hIcon);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE hPrev, LPWSTR cmd, int show)
{
    HANDLE hMutex = CreateMutexW(NULL, TRUE, L"MonitorCPU_RAM_GPU_SingleInstance");
    if (!hMutex || GetLastError() == ERROR_ALREADY_EXISTS) {
        if (hMutex) CloseHandle(hMutex);
        return 0;
    }

    g_hInst = inst;
    g_taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
    g_iconCustom = 0;
    g_hIcon = (HICON)LoadImageW(inst, MAKEINTRESOURCEW(1), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
    if (g_hIcon) {
        g_iconCustom = 1;
    } else {
        g_hIcon = (HICON)LoadImageW(NULL, L"favicon.ico", IMAGE_ICON, 16, 16, LR_LOADFROMFILE);
        if (g_hIcon) g_iconCustom = 1;
        else g_hIcon = LoadIconW(NULL, (LPCWSTR)IDI_APPLICATION);
    }
    CPUInit();
    GPUInit();

    WNDCLASSW wc = {0};
    wc.style      = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance  = inst;
    wc.hIcon      = g_hIcon;
    wc.hCursor    = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = L"MonitorOverlay";
    if (!RegisterClassW(&wc)) { CPUUninit(); GPUUninit(); return 1; }

    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    int px = sw - WIDTH  - 20;
    int py = sh - HEIGHT - 50;

    g_hWnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        L"MonitorOverlay", L"Monitor", WS_POPUP,
        px, py, WIDTH, HEIGHT, NULL, NULL, inst, NULL);
    if (!g_hWnd) { CPUUninit(); GPUUninit(); return 1; }

    SetLayeredWindowAttributes(g_hWnd, 0, 210, LWA_ALPHA);
    TrayAdd(g_hWnd, g_hIcon);
    ShowWindow(g_hWnd, SW_SHOW);
    UpdateData();

    MSG msg;
    while (g_running && GetMessageW(&msg, NULL, 0, 0)) {
        if (msg.message == g_taskbarCreated)
            SendMessage(g_hWnd, WM_TASKBAR, msg.wParam, msg.lParam);
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    CloseHandle(hMutex);
    return 0;
}
