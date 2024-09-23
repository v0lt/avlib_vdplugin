@ECHO OFF
SETLOCAL
CD /D %~dp0

REM -------------------------------------

FOR /F "tokens=2*" %%A IN (
  'REG QUERY "HKLM\SOFTWARE\7-Zip" /v "Path" 2^>NUL ^| FIND "REG_SZ" ^|^|
   REG QUERY "HKLM\SOFTWARE\Wow6432Node\7-Zip" /v "Path" 2^>NUL ^| FIND "REG_SZ"') DO SET "SEVENZIP=%%B\7z.exe"

ECHO %SEVENZIP%

IF NOT EXIST "%SEVENZIP%" (
  ECHO 7Zip not found.
  GOTO :END
)

REM -------------------------------------

SET "FFMPEG_DLLS=avcodec-61.dll avformat-61.dll avutil-59.dll swresample-5.dll swscale-8.dll"

ECHO %FFMPEG_DLLS%

SET /A COUNT=0
FOR %%D IN (%FFMPEG_DLLS%) DO IF EXIST "_bin\ffmpeg\%%D" SET /A COUNT+=1

IF %COUNT% EQU 5 (
  ECHO FFmpeg files are already downloaded and unpacked.
  GOTO :MakeArchive
)

SET FFMPEG_7Z=ffmpeg-7.0.2-full_build-shared.7z

IF NOT EXIST _bin\ffmpeg\%FFMPEG_7Z% (
  MKDIR _bin\ffmpeg
  curl -o "_bin\ffmpeg\%FFMPEG_7Z%" --insecure "https://www.gyan.dev/ffmpeg/builds/packages/%FFMPEG_7Z%"
)

IF NOT EXIST _bin\ffmpeg\%FFMPEG_7Z% (
  ECHO Failed to download %FFMPEG_7Z%.
  GOTO :END
)

"%SEVENZIP%" e "_bin\ffmpeg\%FFMPEG_7Z%" -o"_bin\ffmpeg\" %FFMPEG_DLLS% -r -aos

REM -------------------------------------

:MakeArchive

MKDIR _bin\plugins64

COPY /Y /V "_bin\avlib_x64\avlib-1.vdplugin" "_bin\plugins64\avlib-1.vdplugin"
COPY /Y /V "Readme.md" "_bin\plugins64\avlib-1_Readme.md"
COPY /Y /V "history.txt" "_bin\plugins64\avlib-1_history.txt"

REM -------------------------------------

FOR /F "USEBACKQ" %%F IN (`powershell -NoLogo -NoProfile -Command ^(Get-Item "_bin\plugins64\avlib-1.vdplugin"^).VersionInfo.FileVersion`) DO (SET FILE_VERSION=%%F)

SET PCKG_NAME=avlib_vdplugin_%FILE_VERSION%

"%SEVENZIP%" a -m0=lzma -mx9 -ms=on "_bin\%PCKG_NAME%.7z" ^
.\_bin\plugins64 ^
.\_bin\ffmpeg\avcodec-61.dll ^
.\_bin\ffmpeg\avformat-61.dll ^
.\_bin\ffmpeg\avutil-59.dll ^
.\_bin\ffmpeg\swresample-5.dll ^
.\_bin\ffmpeg\swscale-8.dll

IF EXIST "_bin\plugins64" RD /Q /S "_bin\plugins64"

:END
ENDLOCAL
TIMEOUT /T 5
EXIT /B
