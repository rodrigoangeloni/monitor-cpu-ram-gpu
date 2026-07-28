@echo off
setlocal enabledelayedexpansion

:: Buscar MSVC (Visual Studio)
set VC=0

:: vswhere (VS 2017+)
set "VSW=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSW%" (
    for /f "delims=" %%p in ('"%VSW%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath' 2^>nul') do (
        if exist "%%p\VC\Auxiliary\Build\vcvars64.bat" (
            call "%%p\VC\Auxiliary\Build\vcvars64.bat" >nul
            set VC=1
        )
    )
)

:: Rutas fijas conocidas si vswhere falla
if %VC%==0 (
    for %%v in (2022 2019 2017) do for %%e in (Community Professional Enterprise BuildTools) do (
        if exist "C:\Program Files\Microsoft Visual Studio\%%v\%%e\VC\Auxiliary\Build\vcvars64.bat" (
            call "C:\Program Files\Microsoft Visual Studio\%%v\%%e\VC\Auxiliary\Build\vcvars64.bat" >nul
            set VC=1 & goto :found
        )
        if exist "C:\Program Files (x86)\Microsoft Visual Studio\%%v\%%e\VC\Auxiliary\Build\vcvars64.bat" (
            call "C:\Program Files (x86)\Microsoft Visual Studio\%%v\%%e\VC\Auxiliary\Build\vcvars64.bat" >nul
            set VC=1 & goto :found
        )
    )
)

:found
if %VC%==0 (
    echo [ERROR] No se encontro Visual Studio. Instala Build Tools:
    echo https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022
    exit /b 1
)

if %VC%==0 (
    echo [ERROR] No se encontro Visual Studio. Instala Build Tools:
    echo https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022
    exit /b 1
)

echo [1/3] Compilando icono...
rc /nologo /fo"%~dp0icon.res" "%~dp0icon.rc" 2>nul
if errorlevel 1 (
    echo [WARN] No se pudo compilar icono, continuando sin el
    set ICON_RES=
) else (
    set ICON_RES=%~dp0icon.res
)

echo [2/3] Compilando monitor.c (MSVC x64, optimizado para tamano)...

:: Compilar con flags de minimo tamano
cl /nologo /utf-8 /O1 /GL /MT /EHs-c- /W3 /wd4100 /wd4101 /Fo"%~dp0monitor.obj" "%~dp0monitor.c" /link /SUBSYSTEM:WINDOWS /LTCG /NOLOGO user32.lib gdi32.lib shell32.lib advapi32.lib pdh.lib iphlpapi.lib msimg32.lib dxgi.lib d3d11.lib !ICON_RES! /MERGE:.rdata=.text /MERGE:.data=.text /SECTION:.text,ERW /EMITPOGOPHASEINFO /STACK:65536

if errorlevel 1 (
    echo [ERROR] Compilacion fallida
    exit /b 1
)

:: Borrar archivos temporales
del "%~dp0monitor.obj" 2>nul
del "%~dp0monitor.ilk" 2>nul
del "%~dp0monitor.pdb" 2>nul
if exist "%~dp0icon.res" del "%~dp0icon.res" 2>nul

:: Mostrar resultado
for %%f in ("%~dp0monitor.exe") do (
    set SIZE=%%~zf
    echo [OK] monitor.exe creado: !SIZE! bytes
    if !SIZE! gtr 307200 (
        echo [WARN] Supera 300 KB, revisar optimizaciones
    ) else (
        echo [OK] Dentro del limite de 300 KB ^(307200 bytes^)
    )
)

echo [3/3] Hecho. Usa: monitor.exe
echo       Clic izquierdo = arrastrar
echo       Clic derecho  = menu contextual
echo       Bandeja       = clic derecho para menu
