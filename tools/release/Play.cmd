@echo off
rem Directly launch an overlay extracted into the existing F.E.A.R. folder.
setlocal
title F.E.A.R. VR

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0FEARVR\tools\play.ps1" -InstallDir "%~dp0FEARVR" -RetailRoot "%~dp0." %*
set "FEARVR_EXIT=%ERRORLEVEL%"

if not "%FEARVR_EXIT%"=="0" (
    echo.
    echo F.E.A.R. VR stopped with error code %FEARVR_EXIT%.
    echo Read the message above for the missing file or runtime.
    echo.
    pause
)
exit /b %FEARVR_EXIT%
