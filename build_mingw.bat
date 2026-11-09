@echo off
setlocal enabledelayedexpansion

:: Buscar MinGW-w64
set MINGW=
set MINGW_PATHS=^
C:\mingw64\bin\gcc.exe ^
C:\tools\mingw64\bin\gcc.exe ^
C:\msys64\ucrt64\bin\gcc.exe ^
C:\msys64\mingw64\bin\gcc.exe ^
C:\msys32\mingw64\bin\gcc.exe ^
C:\ProgramData\chocolatey\lib\mingw\tools\mingw64\bin\gcc.exe

for %%p in (%MINGW_PATHS%) do if exist %%p set MINGW=%%p
if not defined MINGW (
    for /f "delims=" %%g in ('where gcc 2^>nul') do if not defined MINGW set MINGW=%%g
)

if not defined MINGW (
    echo [ERROR] No se encontro MinGW-w64. Instala desde:
    echo https://github.com/niXman/mingw-builds-binaries/releases
    echo O con: winget install -e --id MSYS2.MSYS2
    exit /b 1
)

:: Extraer directorio
for %%f in ("%MINGW%") do set BIN_DIR=%%~dpf

echo [1/3] Compilando icono...
"%BIN_DIR%windres.exe" "%~dp0icon.rc" -o "%~dp0icon.o"
if errorlevel 1 (
    echo [WARN] No se pudo compilar icono, continuando sin el
    set ICON_RES=
) else (
    set ICON_RES=%~dp0icon.o
)

echo [2/3] Compilando monitor.c (%MINGW%)...

"%MINGW%" -Os -s -mwindows -municode -m64 -fno-ident -o "%~dp0monitor.exe" "%~dp0monitor.c" !ICON_RES! ^
    -lgdi32 -lshell32 -ladvapi32 -lpdh -liphlpapi -lmsimg32 -ldxgi -ld3d11 -lwinhttp -Wl,--gc-sections

if errorlevel 1 (
    echo [ERROR] Compilacion fallida
    timeout /t 5 >nul
    exit /b 1
)

:: Stripear simbolos extra
if exist "%~dp0monitor.exe" (
    :: Intentar strip adicional
    for %%f in ("%~dp0monitor.exe") do (
        set SIZE=%%~zf
        echo [OK] monitor.exe creado: !SIZE! bytes
        if !SIZE! gtr 307200 (
            echo [WARN] Supera 300 KB, revisar flags
        ) else (
            echo [OK] Dentro del limite de 300 KB
        )
    )
)

echo [3/3] Hecho. Para ejecutar:
echo       monitor.exe
echo       Clic izquierdo = arrastrar ventana
echo       Clic derecho   = menu (Fijar/Salir)
echo       Bandeja        = clic derecho para menu

if exist "%~dp0icon.o" del "%~dp0icon.o"
