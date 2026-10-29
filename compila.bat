@echo off
rem Compila Voice to Chat (Release) con Visual Studio / Build Tools 2022 o piu' recente.
rem Uso: doppio clic, oppure "compila.bat" dal prompt. Il risultato e' build\voice_to_chat.exe
rem (e build\vtc_tests.exe, le prove automatiche).
setlocal

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo Visual Studio non trovato: installa "Sviluppo di applicazioni desktop con C++".
    exit /b 1
)
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
if not defined VSDIR (
    echo Strumenti C++ di Visual Studio non trovati.
    exit /b 1
)
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1

cd /d "%~dp0"
if not exist external\whisper.cpp\CMakeLists.txt (
    echo Mancano le librerie: leggi BUILDING.md, punto 2.
    exit /b 1
)

rem Il Vulkan SDK (per la scheda video). Appena installato, la variabile
rem VULKAN_SDK arriva solo ai prompt aperti dopo: si cerca anche nella cartella.
if not defined VULKAN_SDK (
    for /d %%d in ("C:\VulkanSDK\*") do set "VULKAN_SDK=%%d"
)
if not defined VULKAN_SDK (
    echo Vulkan SDK non trovato: installalo con "winget install KhronosGroup.VulkanSDK" ^(vedi BUILDING.md^).
    exit /b 1
)

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release || exit /b 1
if "%~1"=="" (
    cmake --build build --config Release || exit /b 1
) else (
    cmake --build build --config Release --target %* || exit /b 1
)
echo.
echo Fatto: build\voice_to_chat.exe
endlocal
