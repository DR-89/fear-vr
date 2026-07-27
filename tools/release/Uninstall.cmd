@echo off
rem ============================================================================
rem  Double-click entry point for the uninstaller.
rem
rem  Deleting is not undoable, so this always shows the dry run first and only
rem  removes anything after an explicit confirmation. Saved games and profiles
rem  under <InstallDir>\userdata are kept either way; use
rem     Uninstall.cmd -IncludeUserData
rem  to remove those as well.
rem ============================================================================
setlocal
title F.E.A.R. VR - Uninstall

echo This first pass only lists what would be removed. Nothing is deleted yet.
echo.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\uninstall.ps1" %*
set "FEARVR_EXIT=%ERRORLEVEL%"
if not "%FEARVR_EXIT%"=="0" goto :failed

echo.
choice /c YN /n /m "Remove F.E.A.R. VR now? [Y=yes, N=cancel] "
if errorlevel 2 goto :cancelled

echo.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\uninstall.ps1" -Apply %*
set "FEARVR_EXIT=%ERRORLEVEL%"
if not "%FEARVR_EXIT%"=="0" goto :failed
echo.
pause
exit /b 0

:cancelled
echo.
echo Cancelled. Nothing was changed.
echo.
pause
exit /b 0

:failed
echo.
echo Uninstall stopped with error code %FEARVR_EXIT%.
echo Read the message above; your installation was left as it was.
echo.
pause
exit /b %FEARVR_EXIT%
