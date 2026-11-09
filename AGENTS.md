# Repository Overview & Agent Instructions

Lightweight C Windows desktop application (`monitor.c`) for real-time CPU, RAM, and GPU performance monitoring in a transparent overlay.

## Build Commands

- **MinGW-w64 (GCC)**: `.\build_mingw.bat`
  - Uses `gcc` with `-Os -s -mwindows -municode -m64`.
  - Compiles `icon.rc` via `windres` if present.
- **MSVC (Visual Studio)**: `.\build_msvc.bat`
  - Automatically discovers Visual Studio Build Tools via `vswhere.exe` or standard installation paths.
  - Uses `cl /O1 /GL /MT` size-optimization flags.

## Build Constraints & Size Budget

- **Output Binary**: `monitor.exe`
- **Size Limit**: Must stay under **300 KB (307,200 bytes)**. Both build scripts check file size after compilation and warn if exceeded.
- **Target OS**: Windows 7 or higher (`_WIN32_WINNT 0x0601`).

## Architecture & Code Conventions

- **Single-Source Core**: All application logic resides in `monitor.c` (Win32 GDI rendering, tray icon, context menus, PDH performance counters).
- **Unicode Mandatory**: Entry point is `wWinMain`. Always use wide strings (`L"..."`) and wide Win32/PDH functions (`PdhOpenQueryW`, `PdhAddEnglishCounterW`, etc.).
- **Localized PDH Avoidance**: Use `PdhAddEnglishCounterW` rather than `PdhAddCounterW` so counter paths work across non-English Windows installs.
- **Linked Libraries**: `user32.lib gdi32.lib shell32.lib advapi32.lib pdh.lib` (or `-lgdi32 -lshell32 -ladvapi32 -lpdh`).

## Verification

No unit test suite exists. Verify changes by:
1. Executing `.\build_mingw.bat` or `.\build_msvc.bat` to confirm clean compilation.
2. Ensuring `monitor.exe` is created and stays within the 300 KB budget.

## Release Procedure (OBLIGATORIO en cada nueva actualización)

1. **Subir la versión** en ambos lugares, siempre iguales:
   - monitor.c: `#define APP_VERSION L"vX.Y.Z"`
   - `icon.rc`: `FILEVERSION`/`PRODUCTVERSION` (`X,Y,Z,0`) y los `FileVersion`/`ProductVersion` (`"X.Y.Z.0"`) en ambos bloques de idioma.
2. **Compilar AMBAS versiones** (ambos scripts generan `monitor.exe`, copiar cada resultado antes de la siguiente compilación; cerrar `monitor.exe` si está corriendo):
   - `.\build_mingw.bat` → copiar a `dist\monitor-mingw.exe`
   - `.\build_msvc.bat` → copiar a `dist\monitor-msvc.exe`
   - Verificar que ambos estén bajo 300 KB y que `VersionInfo.FileVersion` sea `X.Y.Z.0`.
3. **Commit** a `main`, **crear el tag** anotado `vX.Y.Z` y hacer push del commit y del tag.
4. **Crear el release** con `gh release create vX.Y.Z dist\monitor-mingw.exe dist\monitor-msvc.exe` incluyendo notas de cambios en español.
