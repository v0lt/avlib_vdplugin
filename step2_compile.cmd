@ECHO OFF
SETLOCAL

FOR /f "delims=" %%A IN ('"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -property installationPath -latest -requires Microsoft.Component.MSBuild') DO SET VS_PATH=%%A

SET DEVENV_PATH=%VS_PATH%\Common7\IDE\devenv

ECHO Building the avlib_vdplugin Release x64.

"%DEVENV_PATH%" avlib.sln /Rebuild "Release|x64"

ENDLOCAL
TIMEOUT /T 5
EXIT /B
