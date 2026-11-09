#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0601
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <pdh.h>
#include <iphlpapi.h>
#include <initguid.h>
#include <dxgi.h>
#include <strsafe.h>
#include <stdint.h>

typedef struct {
    int width;
    int header_h;
    int footer_h;
    int sec_h;

    int sec_h_net;
    int pad_x;
    int pad_y;
    int bar_h;
    int sz_hdr;
    int sz_ttl;
    int sz_sub;
    int sz_bar;
    int sz_gear;
    int sz_net;
    int sz_ftr;
} Layout;

static const int   SCALE_PCT[]    = { 50, 75, 100, 125, 150 };
static const float SCALE_VAL[]    = { 0.50f, 0.75f, 1.00f, 1.25f, 1.50f };
static const int   OPACITY_PCT[]  = { 100, 85, 70, 50 };
static const BYTE  OPACITY_VAL[]  = { 255, 217, 179, 128 };
#define N_SCALE   5
#define N_OPACITY 4
#define S(n) ((int)((n) * g_scale))

#define TIMER_ID      1
#define REFRESH_MS    1000
#define APP_VERSION   L"v1.5.0"
#define WM_TASKBAR    (WM_APP + 1)
#define WM_TRAY       (WM_APP + 2)
#define ID_LOCK       100
#define ID_EXIT       101
#define ID_OPACITY_100 110
#define ID_OPACITY_85  111
#define ID_OPACITY_70  112
#define ID_OPACITY_50  113
#define ID_SCALE_50    120
#define ID_SCALE_75    121
#define ID_SCALE_100   122
#define ID_SCALE_125   123
#define ID_SCALE_150   124
#define PDH_CSTATUS_VALID_DATA 0L
#define PDH_CSTATUS_NEW_DATA   1L
#define AC_SRC_OVER   0x00
#define IF_OPER_CONNECTED  5
#define IF_OPER_OPERATIONAL 6

#define COL_BG_TOP    RGB(8, 10, 16)
#define COL_BG_BOT    RGB(22, 24, 34)
#define COL_TRACK     RGB(20, 22, 30)
#define COL_FRAME     RGB(70, 80, 100)
#define COL_TEXT_HI   RGB(235, 238, 245)
#define COL_TEXT_DIM  RGB(120, 128, 145)
#define COL_TEXT_LOW  RGB(95, 100, 115)
#define COL_CPU       RGB(40, 220, 250)
#define COL_RAM       RGB(255, 230, 0)
#define COL_GPU       RGB(40, 230, 70)
#define COL_VRAM      RGB(120, 255, 130)
#define COL_NET       RGB(255, 60, 100)
#define COL_GEAR      RGB(180, 200, 230)

static HINSTANCE      g_hInst;
static HWND           g_hWnd;
static int            g_locked;
static int            g_running = 1;
static NOTIFYICONDATAW g_nid;
static UINT           g_taskbarCreated;
static int            g_currentH;
static Layout         g_l;
static BYTE           g_opacity = 255;
static float          g_scale   = 1.0f;
static int            g_gearDown;
static float          g_cpu;
static float          g_ramPct;
static DWORD          g_ramUsed, g_ramTotal;
static float          g_gpu;
static float          g_vramPct;
static DWORD          g_vramUsed, g_vramTotal;
static HICON          g_hIcon;
static int            g_iconCustom;

static HQUERY   g_hCPUQ;
static HCOUNTER g_hCPUctr;
static int      g_cpuOK;
static int      g_cpuBaseline;

static DWORD   g_netDownBps;
static DWORD   g_netUpBps;
static int     g_netOK;
static MIB_IFTABLE *g_pIfTable;
static ULONGLONG g_netPrevIn[64];
static ULONGLONG g_netPrevOut[64];
static int       g_netPrevValid[64];
static DWORD     g_netPrevCount;
static ULONGLONG g_netPrevTick;

static COLORREF g_glowColor;

static HFONT    g_fHdr;
static HFONT    g_fTtl;
static HFONT    g_fSub;
static HFONT    g_fBar;
static HFONT    g_fGear;
static HFONT    g_fNet;
static HFONT    g_fFtr;

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

static int GPUReenum(void)
{
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
        for (DWORD oi = 0; oi < oldCount; oi++)
            if (oldPaths && oldPaths[oi] && wcscmp(pp, oldPaths[oi]) == 0) {
                g_gpuPrev[idx] = oldPrev[oi]; break;
            }
        if (PdhAddEnglishCounterW(g_hGPUQ, pp, 0, &g_hGPUctrs[idx]) != ERROR_SUCCESS)
            g_hGPUctrs[idx] = NULL;
        idx++; pp += lstrlenW(pp) + 1;
    }
    g_gpuCount = idx;

    if (oldQ) { PdhCloseQuery(oldQ); }
    if (oldCtrs) HeapFree(GetProcessHeap(), 0, oldCtrs);
    if (oldPaths) HeapFree(GetProcessHeap(), 0, oldPaths);
    if (oldPrev) HeapFree(GetProcessHeap(), 0, oldPrev);

    PdhCollectQueryData(g_hGPUQ);
    return 1;

fail_reenum:
    if (g_hGPUQ) { PdhCloseQuery(g_hGPUQ); g_hGPUQ = NULL; }
    g_hGPUQ = oldQ; g_hGPUctrs = oldCtrs; g_gpuPaths = oldPaths; g_gpuPrev = oldPrev; g_gpuCount = oldCount;
    return 0;
}

static void GPUInit(void)
{
    g_gpuOK = 0; g_hGPUQ = NULL; g_hGPUctrs = NULL; g_gpuPaths = NULL; g_gpuPrev = NULL;
    g_gpuCount = 0; g_gpuLastTick = 0; g_gpuReenumTick = GetTickCount64();
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

    float r = max3D > 0 ? max3D : maxAll;
    if (r > 100) r = 100;
    return r;
}

static HQUERY   g_hVRAMQ;
static HCOUNTER g_hVRAMctr;
static int      g_vramOK;
static IDXGIFactory  *g_dxgiFactory;
static IDXGIAdapter  *g_dxgiAdapter;
static ULONGLONG     g_vramTotalBytes;

static void GPUVramInit(void)
{
    g_vramOK = 0;
    g_vramTotalBytes = 0;
    if (CreateDXGIFactory(&IID_IDXGIFactory, (void**)&g_dxgiFactory) == S_OK) {
        if (g_dxgiFactory->lpVtbl->EnumAdapters(g_dxgiFactory, 0, &g_dxgiAdapter) == S_OK) {
            DXGI_ADAPTER_DESC desc;
            if (g_dxgiAdapter->lpVtbl->GetDesc(g_dxgiAdapter, &desc) == S_OK) {
                g_vramTotalBytes = desc.DedicatedVideoMemory;
            }
        }
    }
    if (PdhOpenQueryW(NULL, 0, &g_hVRAMQ) != ERROR_SUCCESS) return;
    if (PdhAddEnglishCounterW(g_hVRAMQ,
            L"\\GPU Adapter Memory(*)\\Dedicated Usage",
            0, &g_hVRAMctr) != ERROR_SUCCESS) {
        PdhCloseQuery(g_hVRAMQ); g_hVRAMQ = NULL;
        return;
    }
    if (PdhCollectQueryData(g_hVRAMQ) != ERROR_SUCCESS) {
        PdhCloseQuery(g_hVRAMQ); g_hVRAMQ = NULL;
        return;
    }
    PdhCollectQueryData(g_hVRAMQ);
    g_vramOK = 1;
}

static float GetVRAM(void)
{
    if (!g_vramOK || !g_hVRAMQ) return -1;
    if (PdhCollectQueryData(g_hVRAMQ) != ERROR_SUCCESS) return g_vramPct;

    DWORD bufSize = 0;
    DWORD itemCount = 0;
    PdhGetFormattedCounterArrayW(g_hVRAMctr, PDH_FMT_LARGE, &bufSize, &itemCount, NULL);

    ULONGLONG totalUsed = 0;
    if (bufSize > 0 && itemCount > 0) {
        PDH_FMT_COUNTERVALUE_ITEM_W *items =
            (PDH_FMT_COUNTERVALUE_ITEM_W*)HeapAlloc(GetProcessHeap(), 0, bufSize);
        if (items) {
            if (PdhGetFormattedCounterArrayW(g_hVRAMctr, PDH_FMT_LARGE,
                    &bufSize, &itemCount, items) == ERROR_SUCCESS) {
                for (DWORD i = 0; i < itemCount; i++) {
                    if (items[i].FmtValue.CStatus == PDH_CSTATUS_VALID_DATA ||
                        items[i].FmtValue.CStatus == PDH_CSTATUS_NEW_DATA) {
                        totalUsed += (ULONGLONG)items[i].FmtValue.largeValue;
                    }
                }
            }
            HeapFree(GetProcessHeap(), 0, items);
        }
    }
    g_vramUsed = (DWORD)(totalUsed >> 20);

    g_vramTotal = (DWORD)(g_vramTotalBytes >> 20);

    if (g_vramTotal > 0) {
        g_vramPct = (float)((double)g_vramUsed * 100.0 / (double)g_vramTotal);
        if (g_vramPct > 100.0f) g_vramPct = 100.0f;
    } else {
        g_vramPct = -1;
    }
    return g_vramPct;
}

static void GPUVramUninit(void)
{
    if (g_hVRAMQ) { PdhCloseQuery(g_hVRAMQ); g_hVRAMQ = NULL; }
    if (g_dxgiAdapter) { g_dxgiAdapter->lpVtbl->Release(g_dxgiAdapter); g_dxgiAdapter = NULL; }
    if (g_dxgiFactory) { g_dxgiFactory->lpVtbl->Release(g_dxgiFactory); g_dxgiFactory = NULL; }
    g_vramOK = 0;
}

static int NetInit(void)
{
    g_netOK = 0;
    g_netDownBps = g_netUpBps = 0;
    g_netPrevCount = 0;
    ZeroMemory(g_netPrevIn, sizeof(g_netPrevIn));
    ZeroMemory(g_netPrevOut, sizeof(g_netPrevOut));
    ZeroMemory(g_netPrevValid, sizeof(g_netPrevValid));
    g_netPrevTick = GetTickCount64();

    ULONG sz = sizeof(MIB_IFTABLE);
    g_pIfTable = (MIB_IFTABLE*)HeapAlloc(GetProcessHeap(), 0, sz);
    if (!g_pIfTable) return 0;

    DWORD rc = GetIfTable(g_pIfTable, &sz, FALSE);
    if (rc == ERROR_INSUFFICIENT_BUFFER) {
        HeapFree(GetProcessHeap(), 0, g_pIfTable);
        g_pIfTable = (MIB_IFTABLE*)HeapAlloc(GetProcessHeap(), 0, sz);
        if (!g_pIfTable) return 0;
        rc = GetIfTable(g_pIfTable, &sz, FALSE);
    }
    if (rc != NO_ERROR) {
        HeapFree(GetProcessHeap(), 0, g_pIfTable);
        g_pIfTable = NULL;
        return 0;
    }
    g_netOK = 1;
    return 1;
}

static void SampleNet(void)
{
    if (!g_netOK || !g_pIfTable) return;

    ULONG sz = (ULONG)HeapSize(GetProcessHeap(), 0, g_pIfTable);
    if (sz == 0) sz = sizeof(MIB_IFTABLE);
    if (GetIfTable(g_pIfTable, &sz, FALSE) != NO_ERROR) {
        g_netOK = 0;
        return;
    }

    ULONGLONG now = GetTickCount64();
    ULONGLONG elapsed = now - g_netPrevTick;
    if (elapsed == 0) return;

    ULONGLONG totalIn = 0, totalOut = 0;
    DWORD cnt = g_pIfTable->dwNumEntries;
    if (cnt > 64) cnt = 64;
    g_netPrevCount = cnt;

    for (DWORD i = 0; i < cnt; i++) {
        MIB_IFROW *r = &g_pIfTable->table[i];
        if (r->dwType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
        if (r->dwOperStatus < IF_OPER_CONNECTED) continue;

        DWORD inOct = r->dwInOctets;
        DWORD outOct = r->dwOutOctets;

        if (g_netPrevValid[i]) {
            DWORD prevIn = (DWORD)g_netPrevIn[i];
            DWORD prevOut = (DWORD)g_netPrevOut[i];
            DWORD dIn = (DWORD)(((ULONGLONG)inOct - prevIn) & 0xFFFFFFFFULL);
            DWORD dOut = (DWORD)(((ULONGLONG)outOct - prevOut) & 0xFFFFFFFFULL);
            totalIn += dIn;
            totalOut += dOut;
        }
        g_netPrevIn[i] = inOct;
        g_netPrevOut[i] = outOct;
        g_netPrevValid[i] = 1;
    }
    g_netPrevTick = now;

    g_netDownBps = (DWORD)((totalIn * 8ULL * 1000ULL) / elapsed);
    g_netUpBps   = (DWORD)((totalOut * 8ULL * 1000ULL) / elapsed);
}

static void NetUninit(void)
{
    if (g_pIfTable) {
        HeapFree(GetProcessHeap(), 0, g_pIfTable);
        g_pIfTable = NULL;
    }
    g_netOK = 0;
}

static void GlowSetColor(COLORREF c)
{
    g_glowColor = c;
}

static void GlowDraw(HDC dst, int x, int y, int w, int h, BYTE alpha)
{
    if (alpha == 0 || w <= 0 || h <= 0) return;

    BITMAPINFO bi = {0};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    BYTE *bits = NULL;
    HDC memDC = CreateCompatibleDC(dst);
    HBITMAP bmp = CreateDIBSection(memDC, &bi, DIB_RGB_COLORS, (void**)&bits, NULL, 0);
    if (!bmp || !bits) {
        if (bmp) DeleteObject(bmp);
        DeleteDC(memDC);
        return;
    }
    HGDIOBJ old = SelectObject(memDC, bmp);

    int cx = w / 2;
    int cy = h / 2;
    int rx = w - cx;   if (rx < 1) rx = 1;
    int ry = h - cy;   if (ry < 1) ry = 1;
    int rx2 = rx * rx;
    int ry2 = ry * ry;
    DWORD col = 0xFF000000
              | ((DWORD)GetBValue(g_glowColor))
              | ((DWORD)GetGValue(g_glowColor) << 8)
              | ((DWORD)GetRValue(g_glowColor) << 16);

    DWORD *px = (DWORD*)bits;
    for (int py = 0; py < h; py++) {
        int dy = py - cy;
        int dy2 = dy * dy;
        for (int px_ = 0; px_ < w; px_++) {
            int dx = px_ - cx;
            double d2 = (double)(dx * dx) / (double)rx2 + (double)dy2 / (double)ry2;
            if (d2 < 1.0) {
                double t = 1.0 - d2;
                double falloff = t * t * t * t;  /* sharp falloff like neon tube */
                BYTE a = (BYTE)((double)alpha * falloff);
                if (a > 0) {
                    *px = (col & 0x00FFFFFF) | ((DWORD)a << 24);
                }
            }
            px++;
        }
    }

    BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    AlphaBlend(dst, x, y, w, h, memDC, 0, 0, w, h, bf);

    SelectObject(memDC, old);
    DeleteObject(bmp);
    DeleteDC(memDC);
}

static void FontsInit(void)
{
    g_fHdr  = CreateFontW(g_l.sz_hdr, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                          CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    g_fTtl  = CreateFontW(g_l.sz_ttl, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                          CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    g_fSub  = CreateFontW(g_l.sz_sub, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                          CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    g_fBar  = CreateFontW(g_l.sz_bar, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                          CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    g_fGear = CreateFontW(g_l.sz_gear, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                          CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI Symbol");
    g_fNet  = CreateFontW(g_l.sz_net, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                          CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    g_fFtr  = CreateFontW(g_l.sz_ftr, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                          CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
}

static void FontsUninit(void)
{
    if (g_fHdr)  DeleteObject(g_fHdr);
    if (g_fTtl)  DeleteObject(g_fTtl);
    if (g_fSub)  DeleteObject(g_fSub);
    if (g_fBar)  DeleteObject(g_fBar);
    if (g_fGear) DeleteObject(g_fGear);
    if (g_fNet)  DeleteObject(g_fNet);
    if (g_fFtr)  DeleteObject(g_fFtr);
    g_fHdr = g_fTtl = g_fSub = g_fBar = g_fGear = g_fNet = g_fFtr = NULL;
}

static int ComputeLayout(void)
{
    int h = g_l.header_h + g_l.pad_y;
    h += g_l.sec_h;                 /* CPU */
    h += g_l.sec_h;                 /* RAM */
    if (g_gpuOK && g_gpu >= 0) {
        h += g_l.sec_h;             /* GPU bar */
        if (g_vramOK && g_vramPct >= 0 && g_vramTotal > 0)
            h += g_l.sec_h;         /* VRAM bar */
    }
    if (g_netOK) {
        h += g_l.sec_h_net;         /* Net bar */
    }
    h += g_l.pad_y + g_l.footer_h;
    return h;
}

static void ApplyOpacity(void)
{
    if (g_hWnd) {
        SetLayeredWindowAttributes(g_hWnd, 0, g_opacity, LWA_ALPHA);
    }
}

static void RecalcLayout(void)
{
    g_l.width      = (int)(280.0f * g_scale);
    g_l.header_h   = (int)(22.0f  * g_scale);
    g_l.footer_h   = (int)(18.0f  * g_scale);
    g_l.sec_h      = (int)(40.0f  * g_scale);
    g_l.sec_h_net  = (int)(32.0f  * g_scale);
    g_l.pad_x      = (int)(8.0f   * g_scale);
    g_l.pad_y      = (int)(6.0f   * g_scale);
    g_l.bar_h      = (int)(16.0f  * g_scale);
    if (g_l.bar_h < 9) g_l.bar_h = 9;
    g_l.sec_h      = g_l.bar_h + S(4);
    g_l.sec_h_net  = g_l.bar_h + S(4);
    g_l.sz_hdr     = (int)(-12.0f * g_scale);
    g_l.sz_ttl     = (int)(-14.0f * g_scale);
    g_l.sz_sub     = (int)(-10.0f * g_scale);
    g_l.sz_bar     = (int)(-12.0f * g_scale);
    g_l.sz_gear    = (int)(-14.0f * g_scale);
    g_l.sz_net     = (int)(-12.0f * g_scale);
    g_l.sz_ftr     = (int)(-9.0f  * g_scale);

    if (g_l.sz_hdr > -8)  g_l.sz_hdr  = -8;
    if (g_l.sz_ttl > -9)  g_l.sz_ttl  = -9;
    if (g_l.sz_sub > -8)  g_l.sz_sub  = -8;
    if (g_l.sz_bar > -8)  g_l.sz_bar  = -8;
    if (g_l.sz_gear > -8) g_l.sz_gear = -8;
    if (g_l.sz_net > -8)  g_l.sz_net  = -8;
    if (g_l.sz_ftr > -8)  g_l.sz_ftr  = -8;

    FontsUninit();
    FontsInit();

    int newH = ComputeLayout();
    if (g_hWnd) {
        RECT rc; GetWindowRect(g_hWnd, &rc);
        SetWindowPos(g_hWnd, NULL, rc.left, rc.top, g_l.width, newH,
                     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        ApplyOpacity();
    }
    g_currentH = newH;
    if (g_hWnd) InvalidateRect(g_hWnd, NULL, FALSE);
}

static void LoadSettings(void)
{
    g_opacity = 255;
    g_scale = 1.0f;
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\RoanDev\\MonitorOverlay",
                      0, KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS) {
        DWORD v, sz = sizeof(DWORD);
        if (RegQueryValueExW(hKey, L"Opacity", NULL, NULL, (BYTE*)&v, &sz) == ERROR_SUCCESS) {
            if (v >= 50 && v <= 255) g_opacity = (BYTE)v;
        }
        sz = sizeof(DWORD);
        if (RegQueryValueExW(hKey, L"Scale", NULL, NULL, (BYTE*)&v, &sz) == ERROR_SUCCESS) {
            int pct = (int)v;
            for (int i = 0; i < N_SCALE; i++) {
                if (pct == SCALE_PCT[i]) { g_scale = SCALE_VAL[i]; break; }
            }
        }
        RegCloseKey(hKey);
    }
}

static void SaveSettings(void)
{
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\RoanDev\\MonitorOverlay",
                        0, NULL, 0, KEY_SET_VALUE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        DWORD v = g_opacity;
        RegSetValueExW(hKey, L"Opacity", 0, REG_DWORD, (BYTE*)&v, sizeof(v));
        int pct = 100;
        for (int i = 0; i < N_SCALE; i++) {
            if (g_scale == SCALE_VAL[i]) { pct = SCALE_PCT[i]; break; }
        }
        v = (DWORD)pct;
        RegSetValueExW(hKey, L"Scale", 0, REG_DWORD, (BYTE*)&v, sizeof(v));
        RegCloseKey(hKey);
    }
}

static void SetOpacityByIdx(int idx)
{
    if (idx < 0 || idx >= N_OPACITY) return;
    g_opacity = OPACITY_VAL[idx];
    ApplyOpacity();
    SaveSettings();
}

static void SetScaleByIdx(int idx)
{
    if (idx < 0 || idx >= N_SCALE) return;
    g_scale = SCALE_VAL[idx];
    RecalcLayout();
    SaveSettings();
}

static double cos_approx(int deg) {
    int d = deg % 360;
    if (d < 0) d += 360;
    if (d == 0) return 1.0;
    if (d == 90) return 0.0;
    if (d == 180) return -1.0;
    if (d == 270) return 0.0;
    if (d < 90)  return 1.0 - (double)d / 90.0;
    if (d < 180) return -((double)(d - 90)) / 90.0;
    if (d < 270) return -1.0 + (double)(d - 180) / 90.0;
    return ((double)(d - 270)) / 90.0;
}
static double sin_approx(int deg) {
    int d = deg % 360;
    if (d < 0) d += 360;
    if (d == 0) return 0.0;
    if (d == 90) return 1.0;
    if (d == 180) return 0.0;
    if (d == 270) return -1.0;
    if (d < 90)  return (double)d / 90.0;
    if (d < 180) return 1.0 - (double)(d - 90) / 90.0;
    if (d < 270) return -((double)(d - 180)) / 90.0;
    return -1.0 + (double)(d - 270) / 90.0;
}

static void DrawBackground(HDC hdc, RECT cr)
{
    TRIVERTEX v[2] = {
        { cr.left, cr.top,  (COLOR16)GetRValue(COL_BG_TOP)  << 8, (COLOR16)GetGValue(COL_BG_TOP)  << 8, (COLOR16)GetBValue(COL_BG_TOP)  << 8, 0xFF00 },
        { cr.right, cr.bottom, (COLOR16)GetRValue(COL_BG_BOT) << 8, (COLOR16)GetGValue(COL_BG_BOT) << 8, (COLOR16)GetBValue(COL_BG_BOT) << 8, 0xFF00 }
    };
    GRADIENT_RECT g = { 0, 1 };
    GradientFill(hdc, v, 2, &g, 1, GRADIENT_FILL_RECT_V);

    HBRUSH fb = CreateSolidBrush(COL_FRAME);
    FrameRect(hdc, &cr, fb);
    DeleteObject(fb);

    RECT hl = { g_l.pad_x, g_l.header_h - 1, cr.right - g_l.pad_x, g_l.header_h };
    HBRUSH hb = CreateSolidBrush(RGB(50, 55, 70));
    FillRect(hdc, &hl, hb);
    DeleteObject(hb);
}

static void DrawGearIcon(HDC hdc, int cx, int cy, int size, COLORREF c);

static void TextOutOutlined(HDC hdc, int x, int y, const WCHAR *s, COLORREF fg, COLORREF outline)
{
    int len = (int)lstrlenW(s);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, outline);
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) continue;
            TextOutW(hdc, x + dx, y + dy, s, len);
        }
    }
    SetTextColor(hdc, fg);
    TextOutW(hdc, x, y, s, len);
}

static void DrawTextOutlined(HDC hdc, LPCWSTR s, int c, RECT *r, UINT fmt, COLORREF fg, COLORREF outline)
{
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, outline);
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) continue;
            RECT rr = { r->left + dx, r->top + dy, r->right + dx, r->bottom + dy };
            DrawTextW(hdc, s, c, &rr, fmt);
        }
    }
    SetTextColor(hdc, fg);
    DrawTextW(hdc, s, c, r, fmt);
}

static void DrawHeader(HDC hdc, RECT cr)
{
    /* Measure MONITOR text metrics */
    HGDIOBJ old = SelectObject(hdc, g_fHdr);
    TEXTMETRIC tmHdr;
    GetTextMetrics(hdc, &tmHdr);
    int hdrAscent = tmHdr.tmAscent;
    int hdrCenter = S(4) - hdrAscent / 2;  /* visual center of MONITOR */
    SelectObject(hdc, old);

    old = SelectObject(hdc, g_fHdr);
    TextOutOutlined(hdc, g_l.pad_x, S(4), L"MONITOR", COL_TEXT_HI, RGB(0,0,0));
    SelectObject(hdc, old);

    /* Gear icon aligned so its visual center matches MONITOR's visual center */
    old = SelectObject(hdc, g_fGear);
    TEXTMETRIC tmGear;
    GetTextMetrics(hdc, &tmGear);
    WCHAR gearStr[] = { 0x2699, 0 };  /* null-terminated for TextOutOutlined */
    SIZE gearSz;
    GetTextExtentPoint32W(hdc, gearStr, 1, &gearSz);
    /* gearY = baseline position. visual center = gearY - ascent/2. Match to hdrCenter. */
    int gearY = hdrCenter + tmGear.tmAscent / 2;
    int gearX = cr.right - g_l.pad_x - gearSz.cx;
    TextOutOutlined(hdc, gearX, gearY, gearStr, COL_GEAR, RGB(0,0,0));
    SelectObject(hdc, old);
}

static void DrawFooter(HDC hdc, RECT cr)
{
    HGDIOBJ old = SelectObject(hdc, g_fFtr);
    WCHAR buf[64];
    StringCchPrintfW(buf, 64, L"Desarrollado por RoanDev %s", APP_VERSION);
    SIZE sz;
    GetTextExtentPoint32W(hdc, buf, (int)lstrlenW(buf), &sz);
    TextOutOutlined(hdc, cr.right - g_l.pad_x - sz.cx, cr.bottom - g_l.footer_h + S(2), buf, COL_TEXT_LOW, RGB(0,0,0));
    SelectObject(hdc, old);
}

static void DrawNeonGlowBar(HDC hdc, int x, int y, int w, int h, float pct, COLORREF c)
{
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;

    HBRUSH oldBr = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    HPEN oldPen = (HPEN)SelectObject(hdc, GetStockObject(NULL_PEN));

    HBRUSH tb = CreateSolidBrush(COL_TRACK);
    SelectObject(hdc, tb);
    Rectangle(hdc, x, y, x + w, y + h);
    DeleteObject(tb);

    int fw = (int)((double)w * pct / 100.0);
    if (fw > w) fw = w;

    if (fw > 2) {
        HBRUSH fb = CreateSolidBrush(c);
        SelectObject(hdc, fb);
        Rectangle(hdc, x, y, x + fw, y + h);
        DeleteObject(fb);

        HBRUSH hb = CreateSolidBrush(RGB(
            (GetRValue(c) + 100) > 255 ? 255 : (GetRValue(c) + 100),
            (GetGValue(c) + 100) > 255 ? 255 : (GetGValue(c) + 100),
            (GetBValue(c) + 100) > 255 ? 255 : (GetBValue(c) + 100)));
        SelectObject(hdc, hb);
        Rectangle(hdc, x, y, x + fw, y + 2);
        DeleteObject(hb);
    }

    HPEN pen = CreatePen(PS_INSIDEFRAME, 1, c);
    SelectObject(hdc, pen);
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, x, y, x + w, y + h);
    DeleteObject(pen);

    SelectObject(hdc, oldBr);
    SelectObject(hdc, oldPen);
}

static void DrawSection(HDC hdc, int x, int y, int w, const WCHAR *label,
                        float pct, COLORREF c, const WCHAR *displayPct)
{
    int barH = g_l.bar_h;
    DrawNeonGlowBar(hdc, x, y, w, barH, pct, c);

    HGDIOBJ old = SelectObject(hdc, g_fTtl);
    WCHAR buf[64];
    StringCchPrintfW(buf, 64, L"%s %s", label, displayPct);
    RECT tr = { x + S(6), y, x + w, y + barH };
    DrawTextOutlined(hdc, buf, -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE, RGB(255,255,255), RGB(0,0,0));
    SelectObject(hdc, old);
}

static void DrawGPUSection(HDC hdc, int x, int y, int w, float pct)
{
    int barH = g_l.bar_h;
    DrawNeonGlowBar(hdc, x, y, w, barH, pct, COL_GPU);

    HGDIOBJ old = SelectObject(hdc, g_fTtl);
    WCHAR buf[64];
    StringCchPrintfW(buf, 64, L"GPU %3.0f%%", pct);
    RECT tr = { x + S(6), y, x + w, y + barH };
    DrawTextOutlined(hdc, buf, -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE, RGB(255,255,255), RGB(0,0,0));
    SelectObject(hdc, old);
}

static void DrawVramSection(HDC hdc, int x, int y, int w, float pct, DWORD usedMB, DWORD totalMB)
{
    int barH = g_l.bar_h;
    DrawNeonGlowBar(hdc, x, y, w, barH, pct, COL_VRAM);

    /* pill inside the bar on the left */
    int pillW = S(40); if (pillW < 24) pillW = 24;
    int pillX = x + S(2);
    int pillGap = S(1);
    RECT pr = { pillX, y + pillGap, pillX + pillW, y + barH - pillGap };
    HBRUSH pb = CreateSolidBrush(COL_VRAM);
    FillRect(hdc, &pr, pb);
    DeleteObject(pb);

    HGDIOBJ old = SelectObject(hdc, g_fBar);
    WCHAR pbuf[16];
    StringCchPrintfW(pbuf, 16, L"%3.0f%%", pct);
    DrawTextOutlined(hdc, pbuf, -1, &pr, DT_CENTER | DT_VCENTER | DT_SINGLELINE, RGB(255,255,255), RGB(0,0,0));
    SelectObject(hdc, old);

    /* text to the right of the pill inside the same bar */
    old = SelectObject(hdc, g_fSub);
    WCHAR tbuf[96];
    double usedGB = (double)usedMB / 1024.0;
    double totalGB = (double)totalMB / 1024.0;
    StringCchPrintfW(tbuf, 96, L"VRAM %3.0f%% (%.1f / %.0fGB)", pct, usedGB, totalGB);
    TextOutOutlined(hdc, pillX + pillW + S(4), y + pillGap + S(1), tbuf, COL_TEXT_HI, RGB(0,0,0));
    SelectObject(hdc, old);
}

static void DrawGearIcon(HDC hdc, int cx, int cy, int size, COLORREF c)
{
    int R = size / 2;
    int rHole = R * 4 / 10; if (rHole < 2) rHole = 2;
    int rInner = R * 7 / 10;

    /* hexagon points */
    POINT pts[6];
    static const double hc[6] = { 1.0, 0.5, -0.5, -1.0, -0.5, 0.5 };
    static const double hs[6] = { 0.0, 0.8660254037844386, 0.8660254037844386,
                                  0.0, -0.8660254037844386, -0.8660254037844386 };
    for (int i = 0; i < 6; i++) {
        pts[i].x = cx + (int)(hc[i] * (double)R);
        pts[i].y = cy + (int)(hs[i] * (double)R);
    }

    HBRUSH b = CreateSolidBrush(c);
    HPEN pn = CreatePen(PS_SOLID, 1, c);
    HBRUSH ob = (HBRUSH)SelectObject(hdc, b);
    HPEN op = (HPEN)SelectObject(hdc, pn);

    /* outer hexagon */
    Polygon(hdc, pts, 6);

    /* six rounded teeth as small ellipses at each vertex */
    for (int i = 0; i < 6; i++) {
        int tx = (pts[i].x - cx) * rInner / R + cx;
        int ty = (pts[i].y - cy) * rInner / R + cy;
        int td = R / 4; if (td < 2) td = 2;
        Ellipse(hdc, tx - td, ty - td, tx + td, ty + td);
    }

    /* central hole */
    HBRUSH hb = CreateSolidBrush(COL_BG_TOP);
    HBRUSH oh = (HBRUSH)SelectObject(hdc, hb);
    HPEN hpen = CreatePen(PS_SOLID, 1, COL_BG_TOP);
    HPEN oph = (HPEN)SelectObject(hdc, hpen);
    Ellipse(hdc, cx - rHole, cy - rHole, cx + rHole, cy + rHole);
    SelectObject(hdc, oh);
    SelectObject(hdc, oph);
    DeleteObject(hb);
    DeleteObject(hpen);

    SelectObject(hdc, ob);
    SelectObject(hdc, op);
    DeleteObject(b);
    DeleteObject(pn);
}

static void DrawArrow(HDC hdc, int cx, int cy, int size, int up, COLORREF c)
{
    HBRUSH b = CreateSolidBrush(c);
    HPEN pn = CreatePen(PS_INSIDEFRAME, 1, c);
    HBRUSH ob = (HBRUSH)SelectObject(hdc, b);
    HPEN op = (HPEN)SelectObject(hdc, pn);
    POINT pts[3];
    int h = size / 2;
    if (up) {
        pts[0].x = cx;     pts[0].y = cy - h;
        pts[1].x = cx - h; pts[1].y = cy + h;
        pts[2].x = cx + h; pts[2].y = cy + h;
    } else {
        pts[0].x = cx - h; pts[0].y = cy - h;
        pts[1].x = cx + h; pts[1].y = cy - h;
        pts[2].x = cx;     pts[2].y = cy + h;
    }
    Polygon(hdc, pts, 3);
    SelectObject(hdc, ob);
    SelectObject(hdc, op);
    DeleteObject(b);
    DeleteObject(pn);
}

static void DrawNetSection(HDC hdc, int x, int y, int w, DWORD downBps, DWORD upBps)
{
    int barH = g_l.bar_h;
    DrawNeonGlowBar(hdc, x, y, w, barH, 100, COL_NET);

    HGDIOBJ old = SelectObject(hdc, g_fNet);
    SetTextColor(hdc, RGB(255, 255, 255));
    SetBkMode(hdc, TRANSPARENT);

    double downMbps = (double)downBps / 1000000.0;
    double upMbps = (double)upBps / 1000000.0;

    WCHAR tbuf[128];
    StringCchPrintfW(tbuf, 128, L"\x2193 %.0f | \x2191 %.0f Mbps", downMbps, upMbps);

    RECT tr = { x, y, x + w, y + barH };
    DrawTextOutlined(hdc, tbuf, -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE, RGB(255,255,255), RGB(0,0,0));
    SelectObject(hdc, old);
}

static void Render(HWND hWnd)
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hWnd, &ps);
    RECT cr; GetClientRect(hWnd, &cr);

    SetBkMode(hdc, TRANSPARENT);

    DrawBackground(hdc, cr);
    DrawHeader(hdc, cr);
    DrawFooter(hdc, cr);

    int x = g_l.pad_x;
    int w = cr.right - 2 * g_l.pad_x;
    int y = g_l.header_h + g_l.pad_y;

    WCHAR pctTxt[16];

    StringCchPrintfW(pctTxt, 16, L"%3.0f%%", g_cpu);
    DrawSection(hdc, x, y, w, L"CPU", g_cpu, COL_CPU, pctTxt);
    y += g_l.sec_h;

    StringCchPrintfW(pctTxt, 16, L"%3.0f%%", g_ramPct);
    DrawSection(hdc, x, y, w, L"RAM", g_ramPct, COL_RAM, pctTxt);
    y += g_l.sec_h;

    if (g_gpuOK && g_gpu >= 0) {
        DrawGPUSection(hdc, x, y, w, g_gpu);
        y += g_l.sec_h;
        if (g_vramOK && g_vramPct >= 0 && g_vramTotal > 0) {
            DrawVramSection(hdc, x, y, w, g_vramPct, g_vramUsed, g_vramTotal);
            y += g_l.sec_h;
        }
    }

    if (g_netOK) {
        DrawNetSection(hdc, x, y, w, g_netDownBps, g_netUpBps);
    }

    EndPaint(hWnd, &ps);
}

static void UpdateTrayTip(void)
{
    WCHAR tip[160];
    double ud = (double)g_ramUsed / 1024.0;
    double ut = (double)g_ramTotal / 1024.0;
    if (g_gpuOK && g_gpu >= 0 && g_netOK) {
        StringCchPrintfW(tip, 160,
            L"CPU %3.0f%% | RAM %.1f/%.0fGB (%3.0f%%) | GPU %3.0f%% | \x2193 %.0f Mbps \x2191 %.0f Mbps",
            g_cpu, ud, ut, g_ramPct, g_gpu, (double)g_netDownBps/1e6, (double)g_netUpBps/1e6);
    } else if (g_gpuOK && g_gpu >= 0) {
        StringCchPrintfW(tip, 160, L"CPU %3.0f%% | RAM %.1f/%.0fGB (%3.0f%%) | GPU %3.0f%%",
            g_cpu, ud, ut, g_ramPct, g_gpu);
    } else if (g_netOK) {
        StringCchPrintfW(tip, 160, L"CPU %3.0f%% | RAM %.1f/%.0fGB (%3.0f%%) | \x2193 %.0f Mbps \x2191 %.0f Mbps",
            g_cpu, ud, ut, g_ramPct, (double)g_netDownBps/1e6, (double)g_netUpBps/1e6);
    } else {
        StringCchPrintfW(tip, 160, L"CPU %3.0f%% | RAM %.1f/%.0fGB (%3.0f%%)",
            g_cpu, ud, ut, g_ramPct);
    }
    StringCchCopyW(g_nid.szTip, 128, tip);
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

static void UpdateData(void)
{
    g_cpu = GetCPU();
    GetRAM();
    if (g_gpuOK) g_gpu = GetGPU();
    GetVRAM();
    SampleNet();
    if (g_nid.hWnd) UpdateTrayTip();

    int newH = ComputeLayout();
    if (newH != g_currentH && g_hWnd) {
        g_currentH = newH;
        RECT rc; GetWindowRect(g_hWnd, &rc);
        SetWindowPos(g_hWnd, NULL, rc.left, rc.top, g_l.width, newH,
                     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        ApplyOpacity();
    }

    InvalidateRect(g_hWnd, NULL, FALSE);
    UpdateWindow(g_hWnd);
}

static void ShowSettingsMenu(HWND hWnd, int x, int y)
{
    HMENU m = CreatePopupMenu();
    HMENU op = CreatePopupMenu();
    HMENU sc = CreatePopupMenu();

    int curOpIdx = 0;
    for (int i = 0; i < N_OPACITY; i++) {
        if (g_opacity == OPACITY_VAL[i]) { curOpIdx = i; break; }
    }
    WCHAR opLbl[16];
    for (int i = 0; i < N_OPACITY; i++) {
        StringCchPrintfW(opLbl, 16, L"%d%%", OPACITY_PCT[i]);
        AppendMenuW(op, MF_STRING | (curOpIdx == i ? MF_CHECKED : 0),
                    ID_OPACITY_100 + i, opLbl);
    }

    int curScIdx = 0;
    for (int i = 0; i < N_SCALE; i++) {
        if (g_scale == SCALE_VAL[i]) { curScIdx = i; break; }
    }
    WCHAR scLbl[16];
    for (int i = 0; i < N_SCALE; i++) {
        StringCchPrintfW(scLbl, 16, L"%d%%", SCALE_PCT[i]);
        AppendMenuW(sc, MF_STRING | (curScIdx == i ? MF_CHECKED : 0),
                    ID_SCALE_50 + i, scLbl);
    }

    AppendMenuW(m, MF_POPUP, (UINT_PTR)op, L"Opacidad");
    AppendMenuW(m, MF_POPUP, (UINT_PTR)sc, L"Escala");
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_STRING | (g_locked ? MF_CHECKED : 0), ID_LOCK,
                g_locked ? L"Desfijar posición" : L"Fijar posición");
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_STRING, ID_EXIT, L"Salir");

    SetForegroundWindow(hWnd);
    TrackPopupMenu(m, TPM_LEFTALIGN | TPM_BOTTOMALIGN, x, y, 0, hWnd, NULL);
    DestroyMenu(m);
}

static void ShowContextMenu(HWND hWnd, int x, int y)
{
    HMENU m = CreatePopupMenu();
    AppendMenuW(m, MF_STRING, ID_LOCK,
                g_locked ? L"Desfijar posición" : L"Fijar posición");
    AppendMenuW(m, MF_STRING, 130, L"Configuración...");
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_STRING, ID_EXIT, L"Salir");
    SetForegroundWindow(hWnd);
    int cmd = TrackPopupMenu(m, TPM_LEFTALIGN | TPM_BOTTOMALIGN | TPM_RETURNCMD, x, y, 0, hWnd, NULL);
    DestroyMenu(m);
    if (cmd == 130) {
        ShowSettingsMenu(hWnd, x, y);
    } else if (cmd == ID_LOCK) {
        g_locked = !g_locked;
    } else if (cmd == ID_EXIT) {
        g_running = 0;
        DestroyWindow(hWnd);
    }
}

static void ToggleLock(void)
{
    g_locked = !g_locked;
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
    StringCchCopyW(g_nid.szTip, 128, L"Monitor (clic der.)");
    Shell_NotifyIconW(NIM_ADD, &g_nid);
    g_nid.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &g_nid);
}

static void TrayRemove(void)
{
    Shell_NotifyIconW(NIM_DELETE, &g_nid);
}

static void SavePosition(void);
static void LoadPosition(int *px, int *py);

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
        {
            int mx = GET_X_LPARAM(lParam);
            int my = GET_Y_LPARAM(lParam);
            RECT cr;
            GetClientRect(hWnd, &cr);
            int gearSize = S(20); if (gearSize < 14) gearSize = 14;
            int gearX = cr.right - g_l.pad_x - gearSize;
            int gearY = S(2);
            int inGear = (mx >= gearX && mx <= gearX + gearSize &&
                          my >= gearY && my <= gearY + gearSize);
            if (inGear) {
                g_gearDown = 1;
                SetCapture(hWnd);
            } else if (!g_locked) {
                ReleaseCapture();
                POINT pt = { mx, my };
                ClientToScreen(hWnd, &pt);
                SendMessage(hWnd, WM_NCLBUTTONDOWN, HTCAPTION, MAKELPARAM(pt.x, pt.y));
            }
        }
        return 0;

    case WM_MOUSEMOVE:
        if (g_gearDown) {
            int mx = GET_X_LPARAM(lParam);
            int my = GET_Y_LPARAM(lParam);
            RECT cr;
            GetClientRect(hWnd, &cr);
            int gearSize = S(20); if (gearSize < 14) gearSize = 14;
            int gearX = cr.right - g_l.pad_x - gearSize;
            int gearY = S(2);
            if (mx < gearX || mx > gearX + gearSize ||
                my < gearY || my > gearY + gearSize) {
                g_gearDown = 0;
                ReleaseCapture();
            }
        }
        return 0;

    case WM_LBUTTONUP:
        if (g_gearDown) {
            g_gearDown = 0;
            ReleaseCapture();
            int mx = GET_X_LPARAM(lParam);
            int my = GET_Y_LPARAM(lParam);
            RECT cr;
            GetClientRect(hWnd, &cr);
            int gearSize = S(20); if (gearSize < 14) gearSize = 14;
            int gearX = cr.right - g_l.pad_x - gearSize;
            int gearY = S(2);
            if (mx >= gearX && mx <= gearX + gearSize &&
                my >= gearY && my <= gearY + gearSize) {
                POINT pt;
                GetCursorPos(&pt);
                ShowSettingsMenu(hWnd, pt.x, pt.y);
            }
        }
        return 0;

    case WM_RBUTTONUP:
        {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ClientToScreen(hWnd, &pt);
            ShowContextMenu(hWnd, pt.x, pt.y);
        }
        return 0;

    case WM_TRAY:
        /* Con NOTIFYICON_VERSION_4 el evento viene en LOWORD(lParam) */
        if (LOWORD(lParam) == WM_RBUTTONUP || LOWORD(lParam) == WM_CONTEXTMENU) {
            POINT pt;
            GetCursorPos(&pt);
            ShowContextMenu(hWnd, pt.x, pt.y);
            PostMessageW(hWnd, WM_NULL, 0, 0);
        } else if (LOWORD(lParam) == WM_LBUTTONDBLCLK) {
            ToggleLock();
        }
        return 0;

    case WM_COMMAND:
        {
            int id = LOWORD(wParam);
            if (id == ID_LOCK) {
                ToggleLock();
            } else if (id == ID_EXIT) {
                g_running = 0;
                DestroyWindow(hWnd);
            } else if (id >= ID_OPACITY_100 && id <= ID_OPACITY_50) {
                SetOpacityByIdx(id - ID_OPACITY_100);
            } else if (id >= ID_SCALE_50 && id <= ID_SCALE_150) {
                SetScaleByIdx(id - ID_SCALE_50);
            }
        }
        return 0;

    case WM_DESTROY:
        KillTimer(hWnd, TIMER_ID);
        TrayRemove();
        if (g_iconCustom) DestroyIcon(g_hIcon);
        CPUUninit();
        GPUUninit();
        GPUVramUninit();
        NetUninit();
        FontsUninit();
        PostQuitMessage(0);
        return 0;

    case WM_TASKBAR:
        TrayRemove();
        TrayAdd(hWnd, g_nid.hIcon);
        return 0;

    case WM_WINDOWPOSCHANGED:
        SavePosition();
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

static void SavePosition(void)
{
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\RoanDev\\MonitorOverlay",
                        0, NULL, 0, KEY_SET_VALUE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        RECT rc;
        if (!g_hWnd || !GetWindowRect(g_hWnd, &rc)) { RegCloseKey(hKey); return; }
        DWORD v;
        v = (DWORD)rc.left;
        RegSetValueExW(hKey, L"PosX", 0, REG_DWORD, (BYTE*)&v, sizeof(v));
        v = (DWORD)rc.top;
        RegSetValueExW(hKey, L"PosY", 0, REG_DWORD, (BYTE*)&v, sizeof(v));
        RegCloseKey(hKey);
    }
}

static void LoadPosition(int *px, int *py)
{
    int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int vr = vx + GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int vb = vy + GetSystemMetrics(SM_CYVIRTUALSCREEN);
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    int minH = g_l.header_h + g_l.pad_y * 2 + g_l.sec_h * 2 + g_l.footer_h;
    *px = sw - g_l.width  - 20;
    *py = sh - minH - 50;
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\RoanDev\\MonitorOverlay",
                      0, KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS) {
        DWORD v, sz = sizeof(DWORD);
        if (RegQueryValueExW(hKey, L"PosX", NULL, NULL, (BYTE*)&v, &sz) == ERROR_SUCCESS) {
            if ((int)v >= vx && (int)v <= vr - g_l.width) *px = (int)v;
        }
        sz = sizeof(DWORD);
        if (RegQueryValueExW(hKey, L"PosY", NULL, NULL, (BYTE*)&v, &sz) == ERROR_SUCCESS) {
            if ((int)v >= vy && (int)v <= vb - minH) *py = (int)v;
        }
        RegCloseKey(hKey);
    }
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

    LoadSettings();
    RecalcLayout();
    CPUInit();
    GPUInit();
    GPUVramInit();
    NetInit();

    WNDCLASSW wc = {0};
    wc.style      = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance  = inst;
    wc.hIcon      = g_hIcon;
    wc.hCursor    = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = L"MonitorOverlay";
    if (!RegisterClassW(&wc)) {
        CPUUninit(); GPUUninit(); GPUVramUninit(); NetUninit();
        FontsUninit();
        return 1;
    }

    int px, py;
    LoadPosition(&px, &py);

    g_currentH = ComputeLayout();

    g_hWnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        L"MonitorOverlay", L"Monitor", WS_POPUP,
        px, py, g_l.width, g_currentH, NULL, NULL, inst, NULL);
    if (!g_hWnd) {
        CPUUninit(); GPUUninit(); GPUVramUninit(); NetUninit();
        FontsUninit();
        return 1;
    }

    SetLayeredWindowAttributes(g_hWnd, 0, g_opacity, LWA_ALPHA);
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
