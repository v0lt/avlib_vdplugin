@ECHO OFF
SETLOCAL
CD /D %~dp0

REM -------------------------------------

FOR /F "tokens=2*" %%A IN (
  'REG QUERY "HKLM\SOFTWARE\7-Zip" /v "Path" 2^>NUL ^| FIND "REG_SZ" ^|^|
   REG QUERY "HKLM\SOFTWARE\Wow6432Node\7-Zip" /v "Path" 2^>NUL ^| FIND "REG_SZ"') DO SET "SEVENZIP=%%B\7z.exe"

IF NOT EXIST "%SEVENZIP%" (
  ECHO 7Zip not found.
  GOTO :END
)

MKDIR _bin\plugins32

COPY /Y /V "_bin\Release_x86\avlib-1.vdplugin" "_bin\plugins32\avlib-1.vdplugin"
COPY /Y /V "Readme.md" "_bin\plugins32\avlib-1_Readme.md"
COPY /Y /V "history.txt" "_bin\plugins32\avlib-1_history.txt"

REM -------------------------------------

FOR /F "USEBACKQ" %%F IN (`powershell -NoLogo -NoProfile -Command ^(Get-Item "_bin\plugins32\avlib-1.vdplugin"^).VersionInfo.FileVersion`) DO (SET FILE_VERSION=%%F)

SET PCKG_NAME=avlib_vdplugin_win32_%FILE_VERSION%

"%SEVENZIP%" a -m0=lzma -mx9 -ms=on "_bin\%PCKG_NAME%.7z" ^
.\_bin\plugins32 ^
.\ffmpeg\bin_win32\avcodec-62.dll ^
.\ffmpeg\bin_win32\avformat-62.dll ^
.\ffmpeg\bin_win32\avutil-60.dll ^
.\ffmpeg\bin_win32\swresample-6.dll ^
.\ffmpeg\bin_win32\swscale-9.dll

IF EXIST "_bin\plugins32" RD /Q /S "_bin\plugins32"

:END
ENDLOCAL
TIMEOUT /T 5
EXIT /B
