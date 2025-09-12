@ECHO OFF
SETLOCAL ENABLEDELAYEDEXPANSION
CD /D %~dp0

REM -------------------------------------

FOR /F "tokens=2*" %%A IN (
  'REG QUERY "HKLM\SOFTWARE\7-Zip" /v "Path" 2^>NUL ^| FIND "REG_SZ" ^|^|
   REG QUERY "HKLM\SOFTWARE\Wow6432Node\7-Zip" /v "Path" 2^>NUL ^| FIND "REG_SZ"') DO SET "SEVENZIP=%%B\7z.exe"

IF NOT EXIST "%SEVENZIP%" (
  ECHO 7Zip not found.
  GOTO :END
)

REM -------------------------------------

SET "FFMPEG_DLLS=avcodec-62.dll avformat-62.dll avutil-60.dll swresample-6.dll swscale-9.dll"
SET "FFMPEG_LIBS=avcodec.lib avformat.lib avutil.lib swresample.lib swscale.lib"
SET "FFMPEG_DEFS=avcodec-62.def avformat-62.def avutil-60.def swresample-6.def swscale-9.def"

SET /A COUNT=0
FOR %%D IN (%FFMPEG_DLLS%) DO IF EXIST "ffmpeg\bin_win32\%%D" SET /A COUNT+=1
FOR %%D IN (%FFMPEG_LIBS%) DO IF EXIST "ffmpeg\lib_win32\%%D" SET /A COUNT+=1
FOR %%D IN (%FFMPEG_DEFS%) DO IF EXIST "ffmpeg\lib_win32\%%D" SET /A COUNT+=1

IF %COUNT% EQU 15 (
  ECHO FFmpeg files are already downloaded and unpacked.
  GOTO :END
)

SET FFMPEG_ZIP=ffmpeg-n8.0-latest-win32-gpl-shared-8.0.zip

IF NOT EXIST ffmpeg\%FFMPEG_ZIP% (
  ECHO Downloading "%FFMPEG_ZIP%"...
  curl -o "ffmpeg\%FFMPEG_ZIP%" --insecure -L "https://github.com/defisym/FFmpeg-Builds-Win32/releases/download/latest/%FFMPEG_ZIP%"
)

IF NOT EXIST ffmpeg\%FFMPEG_ZIP% (
  ECHO Failed to download %FFMPEG_ZIP%.
  GOTO :END
)

"%SEVENZIP%" e "ffmpeg\%FFMPEG_ZIP%" -o"ffmpeg\bin_win32\" %FFMPEG_DLLS% -r -aos
"%SEVENZIP%" e "ffmpeg\%FFMPEG_ZIP%" -o"ffmpeg\lib_win32\" %FFMPEG_LIBS% -r -aos
"%SEVENZIP%" e "ffmpeg\%FFMPEG_ZIP%" -o"ffmpeg\lib_win32\" %FFMPEG_DEFS% -r -aos

:END
ENDLOCAL
TIMEOUT /T 5
EXIT /B
