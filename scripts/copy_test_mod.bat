@echo off
REM Copy test mod to game directory
REM Usage: copy_test_mod.bat [game_directory]
REM Example: copy_test_mod.bat C:\games\caster

set GAME_DIR=%~1
if "%GAME_DIR%"=="" set GAME_DIR=C:\games\caster

set MOD_SOURCE=%~dp0..\mods\test-health-bar-theme
set MOD_DEST=%GAME_DIR%\mods\test-health-bar-theme

echo Copying test mod from:
echo   %MOD_SOURCE%
echo to:
echo   %MOD_DEST%
echo.

if not exist "%MOD_SOURCE%" (
    echo ERROR: Source mod directory not found: %MOD_SOURCE%
    pause
    exit /b 1
)

if not exist "%GAME_DIR%\mods" (
    echo Creating mods directory: %GAME_DIR%\mods
    mkdir "%GAME_DIR%\mods"
)

if exist "%MOD_DEST%" (
    echo Removing existing mod directory...
    rmdir /s /q "%MOD_DEST%"
)

echo Copying mod files...
xcopy /E /I /Y "%MOD_SOURCE%" "%MOD_DEST%"

if %ERRORLEVEL% EQU 0 (
    echo.
    echo SUCCESS: Test mod copied to %MOD_DEST%
    echo.
    echo The mod should now be visible when you:
    echo   1. Launch CCCaster
    echo   2. Go to Mods menu [M]
    echo   3. Select "List Mods"
    echo.
) else (
    echo.
    echo ERROR: Failed to copy mod files
    echo.
)

pause



