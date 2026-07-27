@echo off
rem ============================================================================
rem  Double-click entry point for the installer.
rem
rem  PowerShell refuses to run .ps1 files on a default Windows installation, so
rem  a double-click on install.ps1 would only open it in an editor. This wrapper
rem  starts it with the execution policy relaxed for this one process, and keeps
rem  the window open afterwards so the result stays readable.
rem
rem  Extra arguments are passed through, e.g.:
rem     Install.cmd -InstallDir "D:\Games\FearVR"
rem ============================================================================
setlocal
title F.E.A.R. VR - Install

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\install.ps1" %*
set "FEARVR_EXIT=%ERRORLEVEL%"

echo.
if not "%FEARVR_EXIT%"=="0" (
    echo Setup stopped with error code %FEARVR_EXIT%. Nothing was left running.
    echo Read the message above; it names the path or file that was missing.
    echo.
)
pause
exit /b %FEARVR_EXIT%
