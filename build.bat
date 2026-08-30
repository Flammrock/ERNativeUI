@echo off
setlocal EnableExtensions
cd /d "%~dp0"

where cmake >nul 2>nul || (
  echo ERROR: CMake 3.28 or newer was not found in PATH.
  exit /b 1
)
where git >nul 2>nul || (
  echo ERROR: Git was not found in PATH.
  exit /b 1
)

echo [ERNativeUI] Configuring Windows x64 Release...
cmake --preset windows-release || exit /b 1

echo [ERNativeUI] Building from a clean object set...
rem Clean-first avoids stale objects after switching between development snapshots.
cmake --build --preset windows-release --clean-first || exit /b 1

echo [ERNativeUI] Running tests...
ctest --preset windows-release || exit /b 1

echo.
echo Build and tests completed successfully.
echo Runtime staging: %CD%\build\preset-release\deploy\Release
echo Host DLL:       %CD%\build\preset-release\Release\ERNativeUI.dll
echo GFX patcher:    %CD%\build\preset-release\Release\ERNativeUIGfxPatcher.exe
exit /b 0
