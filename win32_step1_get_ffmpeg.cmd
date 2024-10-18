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

REM -------------------------------------

SET "FFMPEG_DLLS=avcodec-61.dll avformat-61.dll avutil-59.dll swresample-5.dll swscale-8.dll"
SET "FFMPEG_LIBS=avcodec.lib avformat.lib avutil.lib swresample.lib swscale.lib"
SET "FFMPEG_DEFS=avcodec-61.def avformat-61.def avutil-59.def swresample-5.def swscale-8.def"

SET /A COUNT=0
FOR %%D IN (%FFMPEG_DLLS%) DO IF EXIST "_bin\ffmpeg_win32\%%D" SET /A COUNT+=1
FOR %%D IN (%FFMPEG_LIBS%) DO IF EXIST "ffmpeg_win32\%%D" SET /A COUNT+=1
FOR %%D IN (%FFMPEG_DEFS%) DO IF EXIST "ffmpeg_win32\%%D" SET /A COUNT+=1

IF %COUNT% EQU 15 (
  ECHO FFmpeg files are already downloaded and unpacked.
  GOTO :END
)

SET FFMPEG_ZIP=ffmpeg-n7.1-latest-win32-gpl-shared-7.1.zip

IF NOT EXIST _bin\ffmpeg_win32\%FFMPEG_ZIP% (
  ECHO Downloading "%FFMPEG_ZIP%"...
  MKDIR _bin\ffmpeg_win32
  curl -o "_bin\ffmpeg_win32\%FFMPEG_ZIP%" --insecure -L "https://github.com/yt-dlp/FFmpeg-Builds/releases/download/latest/%FFMPEG_ZIP%"
)

IF NOT EXIST _bin\ffmpeg_win32\%FFMPEG_ZIP% (
  ECHO Failed to download %FFMPEG_ZIP%.
  GOTO :END
)

"%SEVENZIP%" e "_bin\ffmpeg_win32\%FFMPEG_ZIP%" -o"_bin\ffmpeg_win32\" %FFMPEG_DLLS% -r -aos
"%SEVENZIP%" e "_bin\ffmpeg_win32\%FFMPEG_ZIP%" -o"ffmpeg\lib_win32\" %FFMPEG_LIBS% -r -aos
"%SEVENZIP%" e "_bin\ffmpeg_win32\%FFMPEG_ZIP%" -o"ffmpeg\lib_win32\" %FFMPEG_DEFS% -r -aos

:END
ENDLOCAL
TIMEOUT /T 5
EXIT /B
